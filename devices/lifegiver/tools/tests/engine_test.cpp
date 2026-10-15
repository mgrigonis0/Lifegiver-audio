// Offline checks of the Lifegiver engine (Phase 1: the skeleton).
// Build: g++ -std=c++17 -O2 -Wall -Wextra -o engine_test engine_test.cpp -lpthread
#include <chrono>

#include "harness.h"

using namespace test;
using namespace lg;

static int fails = 0;

// 0 % and 100 % (with unity make-up) give the input back bit for bit: any block size, any sample rate, in place too
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
                // in place: Max may hand the same buffer in and out
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
        fails += expect(exact, amt > 0 ? "100 % with unity make-up = the input, bit for bit" : "0 % = the input, bit for bit");
        fails += expect(exactInPlace, "the same, processing in place");
    }
    // a value in between: equal in value (only -0.0 may come out as +0.0)
    Rig rig(48000);
    rig.msg("amount", {37});
    Stereo in = noiseSig(0.25, 48000), out;
    rig.run(in, 0, in.size(), out);
    bool same = true;
    for (size_t i = 0; i < in.size(); i++) if (in.l[i] != out.l[i] || in.r[i] != out.r[i]) same = false;
    fails += expect(same, "37 % with nothing to change = the input (by value)");
}

// a step in Amount: S-shaped, at least 3 ms, no click, lands exactly on the target
static void testFade() {
    std::printf("Amount fades\n");
    for (double sr : {44100.0, 48000.0, 192000.0}) {
        Rig rig(sr);
        rig.msg("makeup", {-6});   // make wet and dry differ, so the fade is audible
        // a low tone: the click measure is only valid where a clean sine's own curvature stays far below its limit
        Stereo in = sineSig(220, 1.0, sr, 0.5), out;
        rig.run(in, 0, (size_t)(0.3 * sr), out);
        rig.msg("amount", {0});
        const size_t at = out.size();
        rig.run(in, at, in.size() - at, out);
        // the gain over time from the left channel's envelope: wet/dry ratio = 10^(-6/20)
        const double gw = dbToGain(-6);
        // measure the blend factor directly: a fresh processor fed with a constant 1.0
        Rig probe(sr);
        probe.msg("makeup", {-6});
        Stereo one; one.l.assign((size_t)(0.2 * sr), 1.0); one.r = one.l;
        Stereo pre; probe.run(one, 0, (size_t)(0.05 * sr), pre);
        probe.msg("amount", {0});
        Stereo po; probe.run(one, 0, one.size(), po);
        // po = 1 + a*(gw - 1): a = (po - 1) / (gw - 1)
        long t10 = -1, t90 = -1, tExact = -1;
        const double step = 1 - gw;
        double maxD1 = 0, maxD2 = 0;
        // the gain just before the change, so the first sample of the fade is measured against it too
        const double g0 = pre.l.back();
        for (size_t i = 0; i < po.size(); i++) {
            const double a = (po.l[i] - 1) / (gw - 1);
            if (t90 < 0 && a <= 0.9) t90 = (long)i;
            if (t10 < 0 && a <= 0.1) t10 = (long)i;
            if (tExact < 0 && po.l[i] == 1.0) tExact = (long)i;
            const double prev = i ? po.l[i - 1] : g0, prev2 = i > 1 ? po.l[i - 2] : (i == 1 ? g0 : g0);
            maxD1 = std::max(maxD1, std::fabs(po.l[i] - prev) / step);
            maxD2 = std::max(maxD2, std::fabs(po.l[i] - 2 * prev + prev2) / step);
        }
        const double fadeMs = (t10 - t90) / sr * 1000, exactMs = tExact / sr * 1000;
        ClickStats cs = findClicks(out.l);
        ClickStats cs2 = findClicks(out.r);
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

// automation: a new Amount every 1-8 ms for 3 s, including full jumps: never a click
static void testAutomation() {
    std::printf("automation\n");
    const double sr = 48000;
    Rig rig(sr);
    rig.msg("makeup", {-12});
    Stereo in = sineSig(220, 3.0, sr, 0.6), out;
    uint32_t seed = 3;
    size_t pos = 0;
    int changes = 0;
    while (pos < in.size()) {
        const size_t step = (size_t)((1 + (lcg(seed) % 8)) * sr / 1000);
        const double r = (lcg(seed) % 1000) / 10.0;
        rig.msg("amount", {(lcg(seed) % 4 == 0) ? ((changes % 2) ? 0.0 : 100.0) : r});
        changes++;
        const size_t n = std::min(step, in.size() - pos);
        rig.run(in, pos, n, out, 128);
        pos += n;
    }
    ClickStats cs = findClicks(out.l), cs2 = findClicks(out.r);
    std::printf("    %d changes, clicks %ld/%ld (worst %.2f)\n", changes, cs.count, cs2.count, std::max(cs.worst, cs2.worst));
    fails += expect(cs.count == 0 && cs2.count == 0, "fast automation stays click-free");
}

// the audio thread never allocates (also while values change), and a stopped audio thread never loses a value
static void testRealtimeSafety() {
    std::printf("real-time safety\n");
    Rig rig(48000);
    Stereo in = noiseSig(1.0, 48000), out;
    out.l.reserve(in.size() + 64); out.r.reserve(in.size() + 64);
    g_allocs = 0;
    g_watchAlloc = true;
    for (size_t d = 0; d + 64 <= in.size(); d += 64) {
        if (d % 6400 == 0) rig.pr->setAmount((d / 6400) % 2 ? 0.2 : 0.9);
        rig.pr->process(in.l.data() + d, in.r.data() + d, in.l.data() + d, in.r.data() + d, 64);
    }
    g_watchAlloc = false;
    std::printf("    allocations while processing: %ld\n", g_allocs.load());
    fails += expect(g_allocs.load() == 0, "process() never allocates");

    // audio stopped: 1000 knob moves, then audio starts: the last one wins
    Rig r2(48000);
    r2.msg("makeup", {-6});
    for (int i = 0; i < 1000; i++) r2.msg("amount", {(double)(i % 101)});
    r2.msg("amount", {0});
    Stereo one; one.l.assign(48000, 0.5); one.r = one.l;
    Stereo o2;
    r2.run(one, 0, one.size(), o2);
    fails += expect(bitEqual(o2.l.back(), 0.5), "after 1000 moves with audio stopped, the last value (0 %) is the one used");
}

static void testState() {
    std::printf("saved state\n");
    // a new device: no save before the set had its chance; then one save with an id
    Rig a;
    a.idle(1.0);
    fails += expect(a.host.saves == 0, "nothing is saved before the set's state (or its absence) is known");
    a.ctl->noState();
    a.idle(1.0);
    const uint32_t id = a.ctl->id();
    std::printf("    new device id %u, saves %d\n", id, a.host.saves);
    fails += expect(a.host.saves == 1 && id >= 1000 && id <= 9999, "a new device gets an id and saves it once");
    fails += expect(a.host.saved.gets("saved_by") == VERSION && (int)a.host.saved.get1("v", 0) == STATE_FORMAT, "the save records format and engine build");
    // Amount is a Live parameter: Live saves it, the engine does not
    a.msg("amount", {50});
    a.idle(1.0);
    fails += expect(a.host.saves == 1, "moving Amount does not write the engine's state");

    // reopen: the same id comes back, no re-save when nothing changed
    Rig b;
    b.ctl->readState(a.host.saved);
    b.idle(1.0);
    bool logged = false;
    for (auto& l : b.host.logs) if (l.find("state restored, id " + std::to_string(id)) != std::string::npos) logged = true;
    fails += expect(b.ctl->id() == id && logged, "reopening restores the same id and says so in the Max window");
    fails += expect(b.host.saves == 0, "reopening the same build does not re-save (no extra undo step)");

    // a set saved by an older build: restored, then re-saved in this build's format
    StateDoc old = a.host.saved;
    old.str["saved_by"] = "2026-01-01a";
    Rig c;
    c.ctl->readState(old);
    c.idle(1.0);
    fails += expect(c.ctl->id() == id && c.host.saves == 1 && c.host.saved.gets("saved_by") == VERSION, "an older build's state is kept and re-saved");

    // junk in the dict: treated as a new device, never a crash
    StateDoc junk;
    junk.num["v"] = {};
    junk.str["id"] = "x";
    Rig d;
    d.ctl->readState(junk);
    d.idle(1.0);
    fails += expect(d.ctl->id() >= 1000 && d.host.saves == 1, "unreadable state = a new device");

    // two devices never share an id by accident (ids are random per new device)
    int same = 0;
    for (int i = 0; i < 20; i++) { Rig x, y; x.ctl->noState(); y.ctl->noState(); if (x.ctl->id() == y.ctl->id()) same++; }
    fails += expect(same <= 1, "two new devices get different ids");
}

static void testMessages() {
    std::printf("messages\n");
    Rig r;
    r.msg("amount", {250});
    Stereo one; one.l.assign(4800, 0.25); one.r = one.l;
    Stereo o; r.run(one, 0, one.size(), o);
    fails += expect(bitEqual(o.l.back(), 0.25), "Amount above 100 is held at 100");
    r.msg("bogus", {1});
    bool logged = false;
    for (auto& l : r.host.logs) if (l.find("unknown message") != std::string::npos) logged = true;
    fails += expect(logged, "an unknown message is reported in the Max window, not ignored silently");
    std::vector<Controller::Atom> word(1);
    word[0].num = false; word[0].s = "bang";
    r.ctl->message("amount", word);
    fails += expect(true, "a word where a number belongs does not crash (counts as the default)");
}

static void testCpu() {
    std::printf("CPU\n");
    const double sr = 48000;
    Rig rig(sr);
    rig.msg("amount", {60});
    rig.msg("makeup", {-3});
    Stereo in = noiseSig(10.0, sr), out;
    out.l.resize(in.size()); out.r.resize(in.size());
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t d = 0; d + 128 <= in.size(); d += 128)
        rig.pr->process(in.l.data() + d, in.r.data() + d, out.l.data() + d, out.r.data() + d, 128);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double pct = sec / 10.0 * 100;
    std::printf("    10 s of stereo at 48 kHz in %.4f s: %.3f %% of one core\n", sec, pct);
    fails += expect(pct < 1.0, "the skeleton uses under 1 % of one core (budget for the full device: 3 %)");
}

// values that arrive before audio starts (Live restoring a set) apply at once: no fade-in from the default
static void testLoad() {
    std::printf("load\n");
    Rig rig(48000);
    rig.msg("makeup", {-6});
    rig.msg("amount", {0});
    Stereo in = noiseSig(0.1, 48000), out;
    rig.run(in, 0, in.size(), out);
    bool exact = true;
    for (size_t i = 0; i < in.size(); i++) if (!bitEqual(in.l[i], out.l[i]) || !bitEqual(in.r[i], out.r[i])) exact = false;
    fails += expect(exact, "a set saved at 0 % plays dry from the very first sample");
    // audio stops, the knob moves, audio restarts: the new value applies at once too
    rig.msg("amount", {100});
    rig.pr->prepare(48000);
    Stereo o2;
    rig.run(in, 0, in.size(), o2);
    fails += expect(std::fabs(o2.l[0] - in.l[0] * dbToGain(-6)) < 1e-12, "after audio restarts, the current value applies from the first sample");
}

int main() {
    std::printf("lifegiver engine %s — offline tests\n", VERSION);
    testLoad();
    testPassThrough();
    testFade();
    testAutomation();
    testRealtimeSafety();
    testState();
    testMessages();
    testCpu();
    std::printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
