// Lifegiver offline bench (BUILD_PLAN Phase 2): runs real audio through the same engine code the device uses.
//
//   render bench <stems dir> <out dir>     every stem: Listen (simulated Arm, up to 4 takes) → reading → solve for its
//                                          role → Moves → renders (processed + loudness-matched original) as WAV
//   render file <in.wav> <out.wav> [role=Lead] [knob.Bite=3] [depth=0.8] [vars=70,0,30,0,0] ...
//
// Build: g++ -std=c++17 -O2 -o render render.cpp -lpthread
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"   // a GCC false positive inside std::vector (inlined)
#endif
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "../source/engine/brain.h"
#include "../source/engine/capture.h"
#include "wav.h"

using namespace lg;

static const char* const WORDS[NQ][5] = {
    {"—", "thin", "light", "full", "heavy"},
    {"—", "lean", "even", "warm", "thick"},
    {"—", "soft", "smooth", "clear", "edgy"},
    {"—", "dull", "soft top", "open", "airy"},
    {"—", "smooth", "fine", "grainy", "rough"},
    {"—", "soft", "firm", "punchy", "hard"},
    {"—", "short", "medium", "long", "held"},
    {"—", "jumpy", "lively", "even", "steady"},
};
static const char* const MOVE_TEXT[] = {
    "", "Lifting the lows", "Easing the low end", "Warming the low-mids", "Clearing low-mid mud", "Lifting presence",
    "Softening presence", "Lifting the air", "Darkening the top", "Tilting brighter", "Tilting darker",
    "Adding warm harmonics", "Adding edgy harmonics", "Steadying level", "Freeing the dynamics", "Sharpening attacks",
    "Softening attacks", "Lengthening sustain", "Shortening tails", "Clearing lows below", "Easing highs above",
    "Widening, lows kept mono", "Narrowing", "Stereo pan", "Level matched to the input", "Set back"};

// Listen, as the device does it: Arm, feed the audio in blocks, take what the capture keeps
static Audio listen(const Audio& src, size_t from, double sr) {
    static Capture cap;   // 8 MB: once
    cap.cancel();
    cap.release();
    cap.arm();
    const long B = 128;
    std::vector<double> L(B), R(B);
    for (size_t i = from; i < src.size() && cap.state.load() != Capture::DONE; i += B) {
        const long n = (long)std::min<size_t>(B, src.size() - i);
        for (long k = 0; k < n; k++) { L[(size_t)k] = src.l[i + (size_t)k]; R[(size_t)k] = src.r[i + (size_t)k]; }
        cap.process(L.data(), R.data(), n, sr);
    }
    Audio t;
    t.sr = sr;
    if (cap.state.load() == Capture::DONE || cap.state.load() == Capture::CAPTURING) {
        const long n = cap.frames();
        t.l.assign(cap.left(), cap.left() + n);
        t.r.assign(cap.right(), cap.right() + n);
    }
    cap.cancel();
    cap.release();
    return t;
}

static Audio processWhole(const Audio& a, const Plan& plan) {
    Chain ch;
    ch.reset(plan, a.sr);
    Audio o = a;
    std::vector<double> L(a.size()), R(a.size());
    for (size_t i = 0; i < a.size(); i++) { L[i] = a.l[i]; R[i] = a.r[i]; }
    ch.process(L.data(), R.data(), L.data(), R.data(), (long)a.size());
    for (size_t i = 0; i < a.size(); i++) { o.l[i] = (float)L[i]; o.r[i] = (float)R[i]; }
    return o;
}

static void printProfile(const Profile& p) {
    std::printf("  heard: %d take(s), %.1f s of sound, %d onsets · ", p.takes, p.activeSec, p.onsets);
    if (p.f0min > 0) std::printf("pitch %.0f–%.0f Hz · ", p.f0min, p.f0max); else std::printf("no stable pitch · ");
    std::printf("%s · %s · stereo corr %.2f · %.1f LUFS · lowest %.0f Hz\n",
                p.harmonic > 0.6 ? "harmonic" : (p.harmonic > 0.3 ? "partly pitched" : "noisy"),
                p.sustained > 0.66 ? "sustained" : (p.sustained > 0.33 ? "medium decay" : "plucked/hit"), p.corr, p.loudness, p.lowestHz);
    std::printf("  reads:");
    for (int q = 0; q < NQ; q++) {
        const int w = readingWord(q, p.reading[q], p.readValid[q]);
        std::printf(" %s %s(%.1f%s)", QUALITY_NAMES[q], WORDS[q][w], p.reading[q], p.certainty[q] >= 0.8 ? "●●●" : (p.certainty[q] >= 0.5 ? "●●○" : "●○○"));
    }
    std::printf("\n");
}

static void printSolution(const Solution& s) {
    std::printf("    reach %3.0f %% ·", s.reach * 100);
    for (int q = 0; q < NQ; q++)
        if (std::fabs(s.asked[q]) >= 0.25) std::printf(" %s %+.1f of %+.1f", QUALITY_NAMES[q], s.reached[q], s.asked[q]);
    std::printf("\n");
    for (const Move& m : movesOf(s)) {
        std::printf("      · %-28s", MOVE_TEXT[m.kind]);
        if (m.kind == MV_LOWCUT || m.kind == MV_HIGHCUT) std::printf(" %.0f Hz", m.value);
        else if (m.kind == MV_MAKEUP || m.kind == MV_STAGE_LEVEL) std::printf(" %+.1f dB", m.value);
        else if (m.kind == MV_WIDER || m.kind == MV_NARROWER) std::printf(" side ×%.2f", m.value);
        else if (m.kind == MV_PAN) std::printf(" %+.2f", m.value);
        else if (m.kind <= MV_TILT_DOWN) std::printf(" %+.1f dB", m.value);
        else if (m.kind == MV_COMP) std::printf(" release %.0f ms", m.value);
        else std::printf(" %.2f", m.value);
        if (m.q >= 0) std::printf("   (%s %+.1f of %+.1f)", QUALITY_NAMES[m.q], m.reached, m.asked);
        std::printf("\n");
    }
}

static void applyRole(Inputs& in, int role, const Profile& p = Profile()) {
    in.role = role;
    const RolePreset r = rolePresetFor(role, p);
    in.pan = r.pan; in.depth = r.depth; in.width = r.width; in.edge = r.edge; in.lo = r.lo; in.hi = r.hi;
}

static int roleByName(const std::string& n) {
    for (int r = 0; r < NROLES; r++) if (n == ROLE_NAMES[r]) return r;
    return ROLE_LEAD;
}

// a one-shot becomes a short pattern (a drum hit is played as a part, not once)
static Audio patternOf(const Audio& hit, double every, double sec) {
    Audio a;
    a.sr = hit.sr;
    const size_t n = (size_t)(sec * hit.sr), step = (size_t)(every * hit.sr);
    a.l.assign(n, 0.f); a.r.assign(n, 0.f);
    for (size_t s = 0; s < n; s += step)
        for (size_t i = 0; i < hit.size() && s + i < n; i++) { a.l[s + i] += hit.l[i]; a.r[s + i] += hit.r[i]; }
    return a;
}

static int bench(const std::string& dir, const std::string& outDir) {
    struct Stem { const char* name; const char* role; double pattern; std::vector<double> takeAt; };
    const Stem stems[] = {
        {"kick", "Rhythm", 0.5, {0}}, {"snare", "Rhythm", 1.0, {0}}, {"bass", "Bass", 0, {0, 4}},
        {"vocal", "Lead", 0, {35, 60, 95, 130}}, {"lead", "Lead", 0, {0, 4}}, {"leadwide", "Hook", 0, {0, 6}},
        {"pad", "Bed", 0, {0, 7}}, {"guitar", "Support", 0, {0, 4}}, {"pluck", "Hook", 0, {0}}, {"fx", "FX", 0, {0, 5}},
    };
    for (const Stem& st : stems) {
        Audio src;
        std::string err;
        if (!wav::read(dir + "/" + st.name + ".wav", src, &err)) { std::printf("%s: %s\n", st.name, err.c_str()); continue; }
        if (st.pattern > 0) src = patternOf(src, st.pattern, 8);
        std::vector<Audio> takes;
        for (double at : st.takeAt) {
            Audio t = listen(src, (size_t)(at * src.sr), src.sr);
            if (t.size() > (size_t)(0.5 * src.sr)) takes.push_back(t);
        }
        const auto t0 = std::chrono::steady_clock::now();
        auto ts = makeTakeSet(takes);
        Profile p = analyze(*ts);
        const double msA = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("\n%s (%s, %.0f Hz) — analysis %.0f ms\n", st.name, st.role, src.sr, msA);
        printProfile(p);
        Inputs in;
        applyRole(in, roleByName(st.role), p);
        auto ss = solveSetOf(takes);
        double push[NQ];
        stagePush(in, push);
        std::printf("  stage %s asks:", st.role);
        for (int q = 0; q < NQ; q++) if (std::fabs(push[q]) >= 0.25) std::printf(" %s %+.1f", QUALITY_NAMES[q], push[q]);
        std::printf("\n");
        const auto t1 = std::chrono::steady_clock::now();
        Solution s = solve(*ss, p, in, -1);
        const double msS = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
        std::printf("  solve (mix A70 C30) %.0f ms\n", msS);
        printSolution(s);
        for (int v = 0; v < NVAR; v++) {
            const auto t2 = std::chrono::steady_clock::now();
            Solution sv = solve(*ss, p, in, v, &s, 5);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t2).count();
            std::printf("  variation %c: reach %3.0f %% (%.0f ms) tools", 'A' + v, sv.reach * 100, ms);
            for (int k = 0; k < NT; k++) std::printf(" %.2f", sv.tools[k]);
            std::printf("\n");
        }
        if (!outDir.empty()) {
            Audio clip = src;
            const size_t maxN = (size_t)(20 * src.sr);
            if (clip.size() > maxN) {   // the vocal: 20 s from the first take
                const size_t from = (size_t)(st.takeAt[0] * src.sr);
                clip.l.assign(src.l.begin() + (long)from, src.l.begin() + (long)std::min(src.size(), from + maxN));
                clip.r.assign(src.r.begin() + (long)from, src.r.begin() + (long)std::min(src.size(), from + maxN));
            }
            wav::write(outDir + "/" + st.name + "_after.wav", processWhole(clip, s.plan));
            Audio before = clip;
            const double g = dbToLin(s.plan[P_STAGE_DB]);   // Compare: the original at the plan's intended level
            for (size_t i = 0; i < before.size(); i++) { before.l[i] *= (float)g; before.r[i] *= (float)g; }
            wav::write(outDir + "/" + st.name + "_before.wav", before);
        }
    }
    return 0;
}

static int file(int argc, char** argv) {
    Audio src;
    std::string err;
    if (!wav::read(argv[2], src, &err)) { std::printf("%s\n", err.c_str()); return 1; }
    Inputs in;
    applyRole(in, ROLE_LEAD);
    for (int i = 4; i < argc; i++) {
        std::string a = argv[i];
        const size_t eq = a.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr(0, eq), v = a.substr(eq + 1);
        if (k == "role") applyRole(in, roleByName(v));
        else if (k.rfind("knob.", 0) == 0) { for (int q = 0; q < NQ; q++) if (k.substr(5) == QUALITY_NAMES[q]) in.knob[q] = std::atof(v.c_str()); }
        else if (k == "depth") in.depth = std::atof(v.c_str());
        else if (k == "width") in.width = std::atof(v.c_str());
        else if (k == "edge") in.edge = std::atof(v.c_str());
        else if (k == "pan") in.pan = std::atof(v.c_str());
        else if (k == "lo") in.lo = std::atof(v.c_str());
        else if (k == "hi") in.hi = std::atof(v.c_str());
        else if (k == "fit") in.fit = std::atof(v.c_str());
        else if (k == "vars") { double x[5] = {0, 0, 0, 0, 0}; std::sscanf(v.c_str(), "%lf,%lf,%lf,%lf,%lf", &x[0], &x[1], &x[2], &x[3], &x[4]); for (int j = 0; j < 5; j++) in.vars[j] = x[j] / 100; }
    }
    auto ts = makeTakeSet({listen(src, 0, src.sr)});
    Profile p = analyze(*ts);
    printProfile(p);
    Solution s = solve(*ts, p, in, -1);
    printSolution(s);
    wav::write(argv[3], processWhole(src, s.plan));
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "bench") return bench(argv[2], argc >= 4 ? argv[3] : "");
    if (argc >= 4 && std::string(argv[1]) == "file") return file(argc, argv);
    std::printf("usage: render bench <stems dir> [out dir] | render file <in.wav> <out.wav> [key=value ...]\n");
    return 1;
}
