// Offline checks of the Lifegiver engine: the audio thread (Amount, Compare, plans, Listen's capture), the main
// thread (messages, the Listen → reading → solve flow, saving), the chain, the measures and the brain.
// Build: g++ -std=c++17 -O2 -Wall -Wextra -o engine_test engine_test.cpp -lpthread
#include <chrono>

#include "harness.h"

using namespace test;
using namespace lg;

static int fails = 0;

static void setPlan(Rig& rig, const Plan& p) { rig.pr->sendPlan(new Plan(p)); }
static Plan gainPlan(double db) { Plan p; p[P_MAKEUP_DB] = db; return p; }

#include "dsp_tests.h"

// 0 % and 100 % (before any Listen the plan is neutral) give the input back bit for bit: any block size, any sample
// rate, in place too
static void testPassThrough() {
    std::printf("pass-through\n");
    const double rates[] = {44100, 48000, 96000, 192000};
    const long blocks[] = {1, 7, 64, 512, 4096};
    for (double amt : {100.0, 0.0}) {
        bool exact = true, exactInPlace = true;
        for (double sr : rates)
            for (long b : blocks) {
                Rig rig(sr);
                rig.msg("amount", {amt});
                Stereo in = noiseSig(0.25, sr), out;
                rig.run(in, 0, in.size(), out, b);
                for (size_t i = 0; i < in.size() && exact; i++)
                    if (!bitEqual(in.l[i], out.l[i]) || !bitEqual(in.r[i], out.r[i])) exact = false;
                Rig rig2(sr);
                rig2.msg("amount", {amt});
                Stereo buf = in;
                for (size_t d = 0; d < buf.size(); d += (size_t)b) {
                    const long nb = (long)std::min<size_t>((size_t)b, buf.size() - d);
                    rig2.pr->process(buf.l.data() + d, buf.r.data() + d, buf.l.data() + d, buf.r.data() + d, nb);
                }
                for (size_t i = 0; i < in.size() && exactInPlace; i++)
                    if (!bitEqual(in.l[i], buf.l[i]) || !bitEqual(in.r[i], buf.r[i])) exactInPlace = false;
            }
        std::printf("    Amount %.0f %%: 4 sample rates x 5 block sizes\n", amt);
        fails += expect(exact, amt > 0 ? "100 % before Listen (neutral plan) = the input, bit for bit" : "0 % = the input, bit for bit");
        fails += expect(exactInPlace, "the same, processing in place");
    }
    // 0 % with a busy plan playing: still the input, bit for bit
    Rig rig(48000);
    setPlan(rig, busyPlan());
    rig.msg("amount", {0});
    Stereo in = noiseSig(0.5, 48000), out;
    rig.run(in, 0, in.size(), out);
    bool exact = true;
    for (size_t i = 0; i < in.size(); i++) if (!bitEqual(in.l[i], out.l[i]) || !bitEqual(in.r[i], out.r[i])) exact = false;
    fails += expect(exact, "0 % with every module working = the input, bit for bit");
}

// a step in Amount: S-shaped, at least 3 ms, no click, lands exactly on the target
static void testFade() {
    std::printf("Amount fades\n");
    for (double sr : {44100.0, 48000.0, 192000.0}) {
        Rig rig(sr);
        setPlan(rig, gainPlan(-6));   // make wet and dry differ, so the fade is audible
        Stereo in = sineSig(220, 1.0, sr, 0.5), out;
        rig.run(in, 0, (size_t)(0.3 * sr), out);
        rig.msg("amount", {0});
        const size_t at = out.size();
        rig.run(in, at, in.size() - at, out);
        const double gw = dbToGain(-6);
        Rig probe(sr);
        setPlan(probe, gainPlan(-6));
        Stereo one; one.l.assign((size_t)(0.2 * sr), 1.0); one.r = one.l;
        Stereo pre; probe.run(one, 0, (size_t)(0.05 * sr), pre);
        probe.msg("amount", {0});
        Stereo po; probe.run(one, 0, one.size(), po);
        long t10 = -1, t90 = -1, tExact = -1;
        const double step = 1 - gw;
        double maxD1 = 0, maxD2 = 0;
        const double g0 = pre.l.back();
        for (size_t i = 0; i < po.size(); i++) {
            const double a = (po.l[i] - 1) / (gw - 1);
            if (t90 < 0 && a <= 0.9) t90 = (long)i;
            if (t10 < 0 && a <= 0.1) t10 = (long)i;
            if (tExact < 0 && po.l[i] == 1.0) tExact = (long)i;
            const double prev = i ? po.l[i - 1] : g0, prev2 = i > 1 ? po.l[i - 2] : g0;
            maxD1 = std::max(maxD1, std::fabs(po.l[i] - prev) / step);
            maxD2 = std::max(maxD2, std::fabs(po.l[i] - 2 * prev + prev2) / step);
        }
        const double fadeMs = (t10 - t90) / sr * 1000, exactMs = tExact / sr * 1000;
        ClickStats cs = findClicks(out.l), cs2 = findClicks(out.r);
        bool tailExact = true;
        for (size_t i = out.size() - (size_t)(0.2 * sr); i < out.size(); i++) if (!bitEqual(out.l[i], in.l[i])) tailExact = false;
        std::printf("    %.0f Hz: 90→10 %% in %.1f ms, exact after %.1f ms, largest step %.4f, largest bend %.6f, clicks %ld/%ld\n",
                    sr, fadeMs, exactMs, maxD1, maxD2, cs.count, cs2.count);
        fails += expect(fadeMs >= 3 && fadeMs <= 40, "the fade lasts between 3 and 40 ms");
        fails += expect(maxD1 < 0.01, "no jump: no sample moves more than 1 % of the fade");
        fails += expect(maxD2 < 0.001, "no corner: the gain curve bends smoothly (S-shaped)");
        fails += expect(cs.count == 0 && cs2.count == 0, "no clicks in the audio");
        fails += expect(tExact > 0 && exactMs < 80 && tailExact, "it lands exactly on the dry signal (bit for bit) within 80 ms");
    }
}

// automation: a new Amount every 1-8 ms for 3 s, including full jumps; Compare flipped now and then: never a click
static void testAutomation() {
    std::printf("automation\n");
    const double sr = 48000;
    Rig rig(sr);
    setPlan(rig, gainPlan(-12));
    Stereo in = sineSig(220, 3.0, sr, 0.6), out;
    uint32_t seed = 3;
    size_t pos = 0;
    int changes = 0;
    while (pos < in.size()) {
        const size_t step = (size_t)((1 + (lcg(seed) % 8)) * sr / 1000);
        const double r = (lcg(seed) % 1000) / 10.0;
        rig.msg("amount", {(lcg(seed) % 4 == 0) ? ((changes % 2) ? 0.0 : 100.0) : r});
        if (changes % 37 == 0) rig.msg("compare", {(double)((changes / 37) % 2)});
        changes++;
        const size_t n = std::min(step, in.size() - pos);
        rig.run(in, pos, n, out, 128);
        pos += n;
    }
    ClickStats cs = findClicks(out.l), cs2 = findClicks(out.r);
    std::printf("    %d changes, clicks %ld/%ld (worst %.2f)\n", changes, cs.count, cs2.count, std::max(cs.worst, cs2.worst));
    fails += expect(cs.count == 0 && cs2.count == 0, "fast automation and Compare flips stay click-free");
}

// the audio thread never allocates (plans arriving, Listen capturing, values changing), and a stopped audio thread
// never loses a value
static void testRealtimeSafety() {
    std::printf("real-time safety\n");
    Rig rig(48000);
    Stereo in = noiseSig(3.0, 48000), out;
    std::vector<Plan*> plans;
    for (int i = 0; i < 12; i++) { Plan p = busyPlan(); p[P_BITE_DB] = (i % 5) - 2; p[P_PAN] = (i % 3 - 1) * 0.4; plans.push_back(new Plan(p)); }
    rig.pr->capture.arm();
    g_allocs = 0;
    g_watchAlloc = true;
    size_t next = 0;
    for (size_t d = 0; d + 64 <= in.size(); d += 64) {
        if (d % 6400 == 0) rig.pr->setAmount((d / 6400) % 2 ? 0.2 : 0.9);
        if (d % 9600 == 0 && next < plans.size()) rig.pr->sendPlan(plans[next++]);   // the push itself doesn't allocate
        rig.pr->process(in.l.data() + d, in.r.data() + d, in.l.data() + d, in.r.data() + d, 64);
    }
    g_watchAlloc = false;
    std::printf("    allocations while processing (12 plans, Listen capturing): %ld\n", g_allocs.load());
    fails += expect(g_allocs.load() == 0, "process() never allocates");
    Report r;
    int garbage = 0;
    while (rig.pr->reps.pop(r)) if (r.type == R_GARBAGE_PLAN) { delete (Plan*)r.ptr; garbage++; }
    fails += expect(garbage == (int)next - 1, "every replaced plan comes back to the main thread to be freed");
    bool finite = true;
    for (size_t i = 0; i < in.size(); i++) if (!std::isfinite(in.l[i]) || !std::isfinite(in.r[i])) finite = false;
    fails += expect(finite, "full-scale noise through a busy chain: no NaN or infinity");

    // audio stopped: 1000 knob moves, then audio starts: the last one wins
    Rig r2(48000);
    setPlan(r2, gainPlan(-6));
    for (int i = 0; i < 1000; i++) r2.msg("amount", {(double)(i % 101)});
    r2.msg("amount", {0});
    Stereo one; one.l.assign(48000, 0.5); one.r = one.l;
    Stereo o2;
    r2.run(one, 0, one.size(), o2);
    fails += expect(bitEqual(o2.l.back(), 0.5), "after 1000 moves with audio stopped, the last value (0 %) is the one used");
}

static void testCodec() {
    std::printf("state codec\n");
    StateDoc d;
    d.num["v"] = {1};
    d.num["id"] = {4821};
    d.num["plan"] = {0.125, -3.5, 1e-9, 12345.678};
    d.num["empty"] = {};
    d.str["saved_by"] = "2026-10-15b";
    d.str["note"] = "";
    std::vector<double> v = encodeState(d);
    StateDoc back;
    const bool ok = decodeState(v, back);
    fails += expect(ok && back.num == d.num && back.str == d.str, "a state survives encode → decode exactly");
    std::vector<double> f32 = v;
    for (double& x : f32) x = (double)(float)x;
    StateDoc b32;
    fails += expect(decodeState(f32, b32) && b32.get1("id", 0) == 4821 && b32.gets("saved_by") == "2026-10-15b",
                    "it still decodes after 32-bit rounding (ids and text intact)");
    int refused = 0, cases = 0;
    auto bad = [&](std::vector<double> x) { StateDoc t; cases++; if (!decodeState(x, t) && t.empty()) refused++; };
    bad({});
    bad({0});
    bad({1, 2, 3, 4});
    std::vector<double> w = v; w[0] = 1234; bad(w);
    w = v; w[1] = 9; bad(w);
    w = v; w.pop_back(); bad(w);
    w = v; w.push_back(7); bad(w);
    w = v; w[2] = 2.5; bad(w);
    w = v; w[2] = 1e9; bad(w);
    w = v; w[4] = 300; bad(w);
    w = v; w[2] = NAN; bad(w);
    std::printf("    %d of %d malformed lists refused\n", refused, cases);
    fails += expect(refused == cases, "malformed lists are refused whole");
}

static std::vector<Controller::Atom> atomsOf(const std::vector<double>& v) {
    std::vector<Controller::Atom> a;
    for (double d : v) { Controller::Atom x; x.d = d; a.push_back(x); }
    return a;
}
static std::vector<double> savedList(uint32_t id, const std::string& by = VERSION, int fmt = 1) {
    StateDoc d;
    d.num["v"] = {(double)fmt};
    d.num["id"] = {(double)id};
    d.str["saved_by"] = by;
    return encodeState(d);
}

static void testState() {
    std::printf("saved state: the device id\n");
    Rig a;
    a.init();
    a.idle(4.0);
    fails += expect(a.host.saves == 0 && a.ctl->isRestored() && a.host.logged("new device, id"), "new device decided at 1.5 s, nothing saved in the first 4 s");
    a.idle(1.0);
    const uint32_t id = a.ctl->id();
    StateDoc pd;
    fails += expect(a.host.requests == 3, "it asks the set three times (0, 300, 800 ms)");
    fails += expect(a.host.saves == 1 && TestHost::doc(a.host.pattr, pd) && pd.get1("id", 0) == id, "then saved once");
    a.msg("amount", {50});
    a.idle(2.0);
    fails += expect(a.host.saves == 1, "moving Amount does not write the engine's state (Live saves it)");

    Rig b;
    b.host.pattr = a.host.pattr;
    b.init();
    b.idle(6.0);
    fails += expect(b.ctl->id() == id && b.host.logged("state restored, id " + std::to_string(id)) && b.host.saves == 0,
                    "reopening: same id, nothing re-saved");

    // Live restores late (2.5 s, after "new device"): the set's state wins; the new id is never written
    Rig e;
    e.init();
    e.idle(2.5);
    e.ctl->message("statelist", atomsOf(savedList(id)));
    e.idle(6.0);
    bool onlySetId = true;
    for (uint32_t s : e.host.savedIds) if (s != id) onlySetId = false;
    fails += expect(e.ctl->id() == id && e.host.logged("arrived late") && onlySetId, "a restore at 2.5 s replaces the new id");
    Rig g;
    g.init();
    g.idle(11.0);
    g.ctl->message("statelist", atomsOf(savedList(id)));
    fails += expect(g.ctl->id() != id, "a restore after 10 s is ignored (limit)");

    // a 15a set (a dict name in the pattr) and a 15e set (format 1): handled
    Rig h;
    Controller::Atom w1; w1.num = false; w1.s = "dictionary";
    Controller::Atom w2; w2.num = false; w2.s = "002lgstate";
    h.host.pattr = {w1, w2};
    h.init();
    h.idle(6.0);
    fails += expect(h.host.logged("new device") && h.host.saves == 1, "a 15a set: new device, saved properly from then on");
    Rig k;
    k.host.pattr = atomsOf(savedList(id, "2026-10-15e", 1));
    k.init();
    k.idle(3.0);
    StateDoc kd;
    fails += expect(k.ctl->id() == id && k.host.saves == 1 && TestHost::doc(k.host.pattr, kd) && kd.gets("saved_by") == VERSION &&
                    (int)kd.get1("v", 0) == STATE_FORMAT, "a 15e set: restored and re-saved by this build");

    int same = 0;
    for (int i = 0; i < 20; i++) { Rig x, y; x.init(); y.init(); x.idle(1.6); y.idle(1.6); if (x.ctl->id() == y.ctl->id()) same++; }
    fails += expect(same <= 1, "two new devices get different ids");
}

// Listen → reading → plan, saved and reopened: the same plan plays, exactly
static void testListenFlow() {
    std::printf("Listen → reading → plan → save → reopen\n");
    Rig a;
    a.init();
    a.idle(2.0);
    fails += expect(a.host.uiLast["facts"].size() > 0 && a.host.uiLast["facts"][0] == 0, "before Listen the view is told: nothing read yet");
    a.listen(sig::lead(5));
    const Profile& p = a.ctl->profile();
    fails += expect(a.ctl->takeCount() == 1 && p.valid, "Arm + playing: one take, read");
    fails += expect(std::fabs(p.f0min - 220) < 5 && std::fabs(p.f0max - 659) < 15, "the reading has the notes played (A3 … E5)");
    a.idle(3.0);   // the solve and the five variations
    const Plan& pl = a.ctl->playing();
    const Solution& s = a.ctl->lastSolve();
    fails += expect(s.ok && s.reach > 0.6, "solved for the Lead stage (reach > 60 %)");
    fails += expect(pl[P_BITE_DB] > 0.2, "the Lead's presence ask is played (a presence lift)");
    std::vector<double> mv = a.host.uiLast["moves"];
    fails += expect(mv.size() >= 5 && mv.size() % 5 == 0, "the Moves reach the view (as numbers)");
    std::vector<double> vr = a.host.uiLast["vars"];
    bool allReach = vr.size() == 6;
    for (int v = 0; v < 5 && allReach; v++) allReach = vr[(size_t)v] >= 0;
    fails += expect(allReach, "all five variations were solved in the background (their reach is shown)");
    // a knob: the fast path answers at once, the slow path refines
    const double before = a.ctl->playing()[P_BITE_DB];
    a.msg("bite", {6});
    a.host.holdJobs = true;
    a.idle(0.05);
    const double fast = a.ctl->playing()[P_BITE_DB];
    fails += expect(fast > before + 0.5, "Bite +6: more presence at once (fast path)");
    a.host.holdJobs = false;
    a.idle(3.0);
    fails += expect(a.ctl->lastSolve().asked[Q_BITE] > 6 && std::fabs(a.ctl->lastSolve().reached[Q_BITE] - a.ctl->lastSolve().asked[Q_BITE]) < 1.5,
                    "then re-solved on the takes: Bite reached within 1.5 steps of what was asked");
    a.idle(2.0);
    const Plan saved = a.ctl->playing();

    // reopen: params arrive first (Live), then the state
    Rig b;
    b.host.pattr = a.host.pattr;
    b.msg("bite", {6});
    b.init();
    b.idle(1.0);
    fails += expect(b.ctl->profile().valid && b.ctl->takeCount() == 0, "reopened: the reading is back (the takes are not saved)");
    bool same = true;
    for (int i = 0; i < NP; i++) if (!bitEqual(b.ctl->playing()[i], saved[i])) same = false;
    fails += expect(same, "reopened: exactly the saved plan plays");
    Stereo in = fromAudio(sig::lead(6)), outA, outB;
    // the audio is the same too (both processors at the same plan). The live level match measures afresh after a
    // reopen (~0.4 s of sound), so compare once both have settled: same level, same shape.
    a.run(in, 0, in.size(), outA);
    b.run(in, 0, in.size(), outB);
    const size_t f0 = in.size() - (size_t)(2 * 48000);
    auto lvl = [&](const std::vector<double>& x) { double e = 0; for (size_t i = f0; i < x.size(); i++) e += x[i] * x[i]; return powToDb(e / (double)(x.size() - f0) + 1e-30); };
    const double lvA = lvl(outA.l), lvB = lvl(outB.l), gab = dbToLin(lvA - lvB);
    double diff = 0;
    for (size_t i = f0; i < in.size(); i++) diff = std::max(diff, std::fabs(outA.l[i] - gab * outB.l[i]));
    std::printf("    reopened vs before, once settled: level %+.3f dB, largest difference after that %.2g\n", lvB - lvA, diff);
    fails += expect(std::fabs(lvA - lvB) < 0.1 && diff < 1e-3, "and it sounds the same (level within 0.1 dB once settled; shape under -60 dB)");
    // after reopening, knobs still work (saved sensitivities)
    b.msg("air", {-8});
    b.idle(0.1);
    fails += expect(b.ctl->playing()[P_AIR_DB] < saved[P_AIR_DB] - 0.5, "after reopening, a knob still acts (Air -8: the top comes down)");
    // removing the only take → back to Listen first (clean pass-through)
    a.msg("removetake", {0});
    a.idle(1.0);   // the compressor's release and the glides finish
    Stereo n = noiseSig(0.2, 48000), on;
    a.msg("amount", {100});
    a.run(n, 0, n.size(), on);
    bool clean = true;
    double worstDiff = 0;
    for (size_t i = (size_t)(0.15 * 48000); i < n.size(); i++) { worstDiff = std::max(worstDiff, std::fabs(on.l[i] - n.l[i])); if (std::fabs(on.l[i] - n.l[i]) > 1e-4) clean = false; }
    if (!clean) std::printf("    after removing the take: largest difference %.3g\n", worstDiff);
    fails += expect(!a.ctl->profile().valid && clean, "removing the last take: back to a clean pass-through (within -80 dB once the release tails end)");
}

static void testMessages() {
    std::printf("messages\n");
    Rig r;
    r.msg("amount", {250});
    Stereo one; one.l.assign(4800, 0.25); one.r = one.l;
    Stereo o; r.run(one, 0, one.size(), o);
    fails += expect(bitEqual(o.l.back(), 0.25), "Amount above 100 is held at 100");
    r.msg("bogus", {1});
    fails += expect(r.host.logged("unknown message"), "an unknown message is reported in the Max window, not ignored silently");
    std::vector<Controller::Atom> word(1);
    word[0].num = false; word[0].s = "bang";
    r.ctl->message("amount", word);
    r.ctl->message("bite", word);
    r.ctl->message("pickrole", word);
    fails += expect(true, "a word where a number belongs does not crash (counts as the default)");
    // the role buttons set the stage parameters (Live's parameters follow)
    r.msg("pickrole", {ROLE_BED});
    fails += expect(r.host.params["role"] == ROLE_BED && std::fabs(r.host.params["depth"] - ROLE_PRESETS[ROLE_BED].depth) < 1e-9,
                    "a role button sets the role and drops the stage into its starting place");
}

static void testCpu() {
    std::printf("CPU\n");
    const double sr = 48000;
    Rig rig(sr);
    setPlan(rig, busyPlan());
    rig.msg("amount", {60});
    Stereo in = noiseSig(10.0, sr), out;
    out.l.resize(in.size()); out.r.resize(in.size());
    // the best of three runs per mode, alternating (a shared machine's noise only ever adds time)
    auto run = [&]() {
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t d = 0; d + 128 <= in.size(); d += 128)
            rig.pr->process(in.l.data() + d, in.r.data() + d, out.l.data() + d, out.r.data() + d, 128);
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };
    double dyn = 1e9, avg = 1e9;
    for (int k = 0; k < 3; k++) {
        rig.msg("dynamics", {0}); run(); dyn = std::min(dyn, run());
        rig.msg("dynamics", {1}); run(); avg = std::min(avg, run());
    }
    std::printf("    every module working, 10 s of stereo at 48 kHz: %.3f %% of one core (Dynamic), %.3f %% (Average)\n", dyn * 10, avg * 10);
    fails += expect(dyn * 10 < 5.0, "under the 5 % budget (this machine; about 10 % on a 2015 Intel Mac, checked in Live)");
    fails += expect(avg < dyn, "Average costs less than Dynamic");
}

// values that arrive before audio starts (Live restoring a set) apply at once: no fade-in from the default
static void testLoad() {
    std::printf("load\n");
    Rig rig(48000);
    setPlan(rig, gainPlan(-6));
    rig.msg("amount", {0});
    Stereo in = noiseSig(0.1, 48000), out;
    rig.run(in, 0, in.size(), out);
    bool exact = true;
    for (size_t i = 0; i < in.size(); i++) if (!bitEqual(in.l[i], out.l[i]) || !bitEqual(in.r[i], out.r[i])) exact = false;
    fails += expect(exact, "a set saved at 0 % plays dry from the very first sample");
    rig.msg("amount", {100});
    rig.pr->prepare(48000);
    Stereo o2;
    rig.run(in, 0, in.size(), o2);
    fails += expect(std::fabs(o2.l[0] - in.l[0] * dbToGain(-6)) < 1e-12, "after audio restarts, the current values apply from the first sample");
    // a plan that arrives before audio starts plays from the first sample, no glide
    Rig r2(48000);
    setPlan(r2, gainPlan(-6));
    Stereo o3;
    r2.run(in, 0, in.size(), o3);
    fails += expect(std::fabs(o3.l[0] - in.l[0] * dbToGain(-6)) < 1e-12, "a plan that arrives before audio starts applies at once");
}

#include "module_tests.h"

// 16d: the live level match and the user's Gain, through the device's own path (controller + audio thread)
static void testLevelAndGain() {
    std::printf("level match and Gain (16d)\n");
    auto lufsOf = [](const Stereo& x, size_t from, size_t to, double sr) {
        Audio t; t.sr = sr;
        for (size_t i = from; i < to; i++) { t.l.push_back((float)x.l[i]); t.r.push_back((float)x.r[i]); }
        std::vector<double> b; loudnessBlocks(t, b); return integratedLoudness(b);
    };
    {
        // a character at 100 % played on the fast path only (no re-solve, as after reopening a set): the output still
        // ends up as loud as the input (the stage's own level aside)
        Rig a;
        a.init();
        a.listen(sig::drums(5));
        a.idle(3.0);
        a.host.holdJobs = true;
        a.msg("vare", {100});
        a.idle(0.05);
        const Stereo in = fromAudio(sig::drums(10));
        Stereo out;
        a.run(in, 0, in.size(), out);
        const double stage = a.ctl->playing()[P_STAGE_DB];
        const size_t n = in.size();
        const double early = lufsOf(out, 0, n / 10, a.sr) - lufsOf(in, 0, n / 10, a.sr) - stage;
        const double late = lufsOf(out, n / 2, n, a.sr) - lufsOf(in, n / 2, n, a.sr) - stage;
        std::printf("    E Driven 100 %%, fast path, drums: first second %+.1f LU, after 5 s %+.1f LU vs the input\n", early, late);
        fails += expect(std::fabs(late) < 1.0, "the live level match: as loud as the input once settled (fast path, a strong character)");
        a.host.holdJobs = false;
        // Gain: exactly x1 at 0 dB, +6 dB is +6 dB, and moving it doesn't click
        Rig g;
        g.init();
        const Stereo t = fromAudio(sig::lead(2));
        Stereo o0, o6;
        g.msg("amount", {0});
        g.run(t, 0, t.size(), o0);
        bool exact = true;
        for (size_t i = 0; i < t.size(); i++) if (!bitEqual(o0.l[i], t.l[i]) || !bitEqual(o0.r[i], t.r[i])) exact = false;
        fails += expect(exact, "Gain 0 dB with Amount 0 %: exactly the input");
        g.msg("gain", {6});
        g.idle(0.1);
        g.run(t, 0, t.size(), o6);
        double e0 = 0, e6 = 0;
        for (size_t i = 0; i < t.size(); i++) { e0 += t.l[i] * t.l[i]; e6 += o6.l[i] * o6.l[i]; }
        const double gd = powToDb(e6 / e0);
        std::printf("    Gain +6 dB: %+.3f dB\n", gd);
        fails += expect(std::fabs(gd - 6) < 0.01, "Gain +6 dB raises the output by 6 dB");
        Stereo tone, ot;
        tone.l.resize((size_t)(2 * g.sr)); tone.r.resize(tone.l.size());
        for (size_t i = 0; i < tone.size(); i++) tone.l[i] = tone.r[i] = 0.4 * std::sin(2 * M_PI * 80 * i / g.sr);
        for (int k = 0; k < 8; k++) {
            g.msg("gain", {k % 2 ? -12.0 : 12.0});
            g.run(tone, (size_t)k * tone.size() / 8, tone.size() / 8, ot);
        }
        const ClickStats cs = findClicks(ot.l);
        std::printf("    Gain swung -12 <-> +12 dB on a low tone: clicks %ld\n", cs.count);
        fails += expect(cs.count == 0, "Gain moves without clicks");
    }
}

int main(int argc, char** argv) {
    std::printf("lifegiver engine %s — offline tests\n", VERSION);
    if (argc > 1 && std::string(argv[1]) == "modules") { testModules(); std::printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails); return fails ? 1 : 0; }
    if (argc > 1 && std::string(argv[1]) == "level") { testLevelAndGain(); std::printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails); return fails ? 1 : 0; }
    if (argc > 1 && std::string(argv[1]) == "brain") { testBrain(); std::printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails); return fails ? 1 : 0; }
    testLoad();
    testPassThrough();
    testFade();
    testAutomation();
    testRealtimeSafety();
    testCodec();
    testState();
    testMessages();
    testChain();
    testMeasures();
    testCapture();
    testReading();
    testBrain();
    testModules();
    testListenFlow();
    testLevelAndGain();
    testCpu();
    std::printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
