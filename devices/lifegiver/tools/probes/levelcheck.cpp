// Scratch harness: loudness of the device's output vs its input, per character at 100 %, per Scale, for the fast path
// (what plays right after a knob move, and after reopening a set) and for the full re-solve.
//   levelcheck <stems dir> [depth]
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "brain.h"
#include "capture.h"
#include "../wav.h"

using namespace lg;

static Audio listen(const Audio& src, size_t from, double sr) {
    static Capture cap;
    cap.cancel(); cap.release(); cap.arm();
    const long B = 128;
    std::vector<double> L(B), R(B);
    for (size_t i = from; i < src.size() && cap.state.load() != Capture::DONE; i += B) {
        const long n = (long)std::min<size_t>(B, src.size() - i);
        for (long k = 0; k < n; k++) { L[(size_t)k] = src.l[i + (size_t)k]; R[(size_t)k] = src.r[i + (size_t)k]; }
        cap.process(L.data(), R.data(), n, sr);
    }
    Audio t; t.sr = sr;
    const long n = cap.frames();
    t.l.assign(cap.left(), cap.left() + n); t.r.assign(cap.right(), cap.right() + n);
    cap.cancel(); cap.release();
    return t;
}

static Audio processWhole(const Audio& a, const Plan& plan, double scale = 1.0) {
    Chain ch;
    ch.reset(plan, a.sr);
    ch.setScale(scale);
    Audio o = a;
    std::vector<double> L(a.size()), R(a.size());
    for (size_t i = 0; i < a.size(); i++) { L[i] = a.l[i]; R[i] = a.r[i]; }
    const long B = 128;
    for (long i = 0; i < (long)a.size(); i += B) {
        const long n = std::min<long>(B, (long)a.size() - i);
        ch.process(L.data() + i, R.data() + i, L.data() + i, R.data() + i, n);
    }
    for (size_t i = 0; i < a.size(); i++) { o.l[i] = (float)L[i]; o.r[i] = (float)R[i]; }
    return o;
}

static double lufs(const Audio& a) { std::vector<double> b; loudnessBlocks(a, b); return integratedLoudness(b); }
static Audio slice(const Audio& a, double t0, double t1) {
    Audio o; o.sr = a.sr;
    const size_t i0 = (size_t)(t0 * a.sr), i1 = std::min(a.size(), (size_t)(t1 * a.sr));
    o.l.assign(a.l.begin() + (long)i0, a.l.begin() + (long)i1); o.r.assign(a.r.begin() + (long)i0, a.r.begin() + (long)i1);
    return o;
}
// the device: plays plan A for 6 s, then switches to plan B while playing; live level match on
// returns: settled error (last 8 s, dB) and the worst 400 ms deviation from the input within 3 s of the switch
static bool MATCH = true;
static void live(const Audio& src, const Plan& a, const Plan& b, double& settled, double& worst, double& stageDb) {
    Chain ch; ch.setLiveMatch(MATCH); ch.reset(a, src.sr);
    Audio o = src;
    std::vector<double> L(src.size()), R(src.size());
    for (size_t i = 0; i < src.size(); i++) { L[i] = src.l[i]; R[i] = src.r[i]; }
    const long B = 128, sw = (long)(6 * src.sr);
    for (long i = 0; i < (long)src.size(); i += B) {
        if (i <= sw && i + B > sw) ch.setTarget(b);
        const long n = std::min<long>(B, (long)src.size() - i);
        ch.process(L.data() + i, R.data() + i, L.data() + i, R.data() + i, n);
    }
    for (size_t i = 0; i < src.size(); i++) { o.l[i] = (float)L[i]; o.r[i] = (float)R[i]; }
    stageDb = b[P_STAGE_DB];
    const double T = src.size() / src.sr;
    settled = lufs(slice(o, T - 8, T)) - lufs(slice(src, T - 8, T)) - stageDb;
    worst = 0;
    for (double t = 6; t < 9; t += 0.1) {
        const double li = lufs(slice(src, t, t + 0.4));
        if (li < -50) continue;
        const double d = lufs(slice(o, t, t + 0.4)) - li - stageDb;
        if (std::fabs(d) > std::fabs(worst)) worst = d;
    }
}

int main(int argc, char** argv) {
    const std::string dir = argv[1];
    const double depth = -1; MATCH = argc > 2 ? std::atoi(argv[2]) != 0 : true;
    const char* names[] = {"drumbass", "jazzmix", "strings", "trumpet", "voice", "snare", "sax", "clarinet", "trumpet2", "piano", "synthstrings"};
    const char* roles[] = {"Rhythm", "Support", "Bed", "Lead", "Lead", "Rhythm", "Lead", "Hook", "Lead", "Support", "Bed"};
    std::printf("%-12s | E..A at 100 %%, fast path, live match on: settled dB / worst 400 ms in the 3 s after the switch\n", "stem");
    const int only = getenv("ONLY") ? std::atoi(getenv("ONLY")) : -1;
    for (int si = 0; si < 11; si++) {
        if (only >= 0 && si != only) continue;
        Audio src; std::string err;
        if (!wav::read(dir + "/" + names[si] + ".wav", src, &err)) { std::printf("%s: %s\n", names[si], err.c_str()); continue; }
        auto ts = makeTakeSet({listen(src, 0, src.sr)});
        Profile p = analyze(*ts);
        Inputs in;
        int role = ROLE_LEAD;
        for (int r = 0; r < NROLES; r++) if (std::string(roles[si]) == ROLE_NAMES[r]) role = r;
        in.role = role;
        const RolePreset rp = rolePresetFor(role, p);
        in.depth = depth >= 0 ? depth : rp.depth; in.width = rp.width; in.edge = rp.edge; in.lo = rp.lo; in.hi = rp.hi;
        if (src.size() < (size_t)(15 * src.sr)) continue;
        Solution s = solve(*ts, p, in, -1);
        double st0, wo0, sd0;
        live(src, s.plan, s.plan, st0, wo0, sd0);
        std::printf("%-12s | none %+4.1f/%+4.1f |", names[si], st0, wo0);
        for (int v = 0; v < NVAR; v++) {
            if (getenv("VAR") && v != std::atoi(getenv("VAR"))) continue;
            Inputs iv = in; iv.vars[v] = 1;
            double st, wo, sd;
            live(src, s.plan, fastStep(s, p, iv, -1).plan, st, wo, sd);
            std::printf("  %c %+4.1f/%+4.1f", 'A' + v, st, wo);
        }
        std::printf("\n");
        std::fflush(stdout);
    }
}
