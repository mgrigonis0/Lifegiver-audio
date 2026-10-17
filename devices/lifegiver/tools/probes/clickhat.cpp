// clickhat <wav> [Role] [A..E: a character at 100 %]. Does the chain add a click at hat onsets? Per onset: peak (first 2 ms) and HF energy >8 kHz (first 3 ms)
// of output vs input, after matching overall level; reported as the worst/median over onsets. One module at a time.
// "rel" (17c): the onset's HF change minus the change in the body of the same hit (5–30 ms after it): a static EQ
// reads ~0 there (it changes the whole hit alike), a click (the onset sharpened against its own body) reads > 0.
#include <cstdio>
#include <string>
#include <algorithm>
#include "brain.h"
#include "capture.h"
#include "../wav.h"
using namespace lg;
static Audio listen(const Audio& src) {
    static Capture cap; cap.cancel(); cap.release(); cap.arm();
    std::vector<double> L(128), R(128);
    for (size_t i = 0; i < src.size() && cap.state.load() != Capture::DONE; i += 128) {
        const long n = (long)std::min<size_t>(128, src.size() - i);
        for (long k = 0; k < n; k++) { L[(size_t)k] = src.l[i + (size_t)k]; R[(size_t)k] = src.r[i + (size_t)k]; }
        cap.process(L.data(), R.data(), n, src.sr);
    }
    Audio t; t.sr = src.sr; const long n = cap.frames();
    t.l.assign(cap.left(), cap.left() + n); t.r.assign(cap.right(), cap.right() + n); cap.cancel(); cap.release(); return t;
}
static std::vector<double> run(const Audio& a, const Plan& p) {
    Chain ch; ch.reset(p, a.sr);
    std::vector<double> L(a.l.begin(), a.l.end()), R(a.r.begin(), a.r.end());
    for (long i = 0; i < (long)a.size(); i += 128) { const long n = std::min<long>(128, (long)a.size() - i); ch.process(L.data() + i, R.data() + i, L.data() + i, R.data() + i, n); }
    return L;
}
int main(int argc, char** argv) {
    Audio src; std::string err;
    if (!wav::read(argv[1], src, &err)) { std::printf("%s\n", err.c_str()); return 1; }
    const std::string role = argc > 2 ? argv[2] : "Rhythm";
    auto ts = makeTakeSet({listen(src)});
    Profile p = analyze(*ts);
    Inputs in; int r = ROLE_LEAD;
    for (int k = 0; k < NROLES; k++) if (role == ROLE_NAMES[k]) r = k;
    in.role = r; const RolePreset rp = rolePresetFor(r, p); in.depth = rp.depth; in.width = 0.5; in.lo = rp.lo; in.hi = rp.hi;
    if (argc > 3 && argv[3][0] >= 'A' && argv[3][0] <= 'E') in.vars[argv[3][0] - 'A'] = 1.0;   // a character at 100 %
    Solution s = solve(*ts, p, in, -1);
    // onsets from the input: envelope rises
    std::vector<double> x(src.l.begin(), src.l.end());
    const double sr = src.sr;
    std::vector<long> on;
    double env = 0, slow = 0; long last = -100000;
    for (long i = 0; i < (long)x.size(); i++) {
        env = std::max(std::fabs(x[(size_t)i]), env * 0.999); slow += 0.0005 * (env - slow);
        if (env > 2.5 * slow && env > 0.02 && i - last > (long)(0.05 * sr)) { on.push_back(i); last = i; }
    }
    auto metric = [&](const std::vector<double>& y, double& pkW, double& hfW, double& relMed, double& relW) {
        Svf h; h.set(Svf::HP, 8000, 0.7071, 0, sr); Svf hx; hx.set(Svf::HP, 8000, 0.7071, 0, sr);
        std::vector<double> yh(y.size()), xh(x.size());
        for (size_t i = 0; i < y.size(); i++) { yh[i] = h.tick(y[i]); xh[i] = hx.tick(x[i]); }
        double ey = 0, ex = 0; for (size_t i = 0; i < y.size(); i++) { ey += y[i] * y[i]; ex += x[i] * x[i]; }
        const double g = std::sqrt(ex / (ey + 1e-30));
        std::vector<double> pk, hf, rel;
        for (long o : on) {
            {
                const long a1 = std::max(0L, o - (long)(0.001 * sr)), b1 = std::min((long)x.size(), o + (long)(0.003 * sr));
                const long a2 = std::min((long)x.size(), o + (long)(0.005 * sr)), b2x = std::min((long)x.size(), o + (long)(0.030 * sr));
                double yo = 0, xo = 0, yb = 0, xb = 0;
                for (long i = a1; i < b1; i++) { yo += yh[(size_t)i] * yh[(size_t)i]; xo += xh[(size_t)i] * xh[(size_t)i]; }
                for (long i = a2; i < b2x; i++) { yb += yh[(size_t)i] * yh[(size_t)i]; xb += xh[(size_t)i] * xh[(size_t)i]; }
                rel.push_back(10 * std::log10((yo + 1e-20) / (xo + 1e-20)) - 10 * std::log10((yb + 1e-20) / (xb + 1e-20)));
            }
            const long a = std::max(0L, o - (long)(0.001 * sr)), b2 = std::min((long)x.size(), o + (long)(0.002 * sr)), b3 = std::min((long)x.size(), o + (long)(0.003 * sr));
            double py = 0, px = 0, hy = 0, hxv = 0;
            for (long i = a; i < b2; i++) { py = std::max(py, std::fabs(y[(size_t)i]) * g); px = std::max(px, std::fabs(x[(size_t)i])); }
            for (long i = a; i < b3; i++) { hy += yh[(size_t)i] * yh[(size_t)i] * g * g; hxv += xh[(size_t)i] * xh[(size_t)i]; }
            pk.push_back(20 * std::log10((py + 1e-9) / (px + 1e-9))); hf.push_back(10 * std::log10((hy + 1e-20) / (hxv + 1e-20)));
        }
        std::sort(pk.begin(), pk.end()); std::sort(hf.begin(), hf.end()); std::sort(rel.begin(), rel.end());
        pkW = pk.back(); hfW = hf.back(); relMed = rel[rel.size() / 2]; relW = rel.back();
        return std::make_pair(pk[pk.size() / 2], hf[hf.size() / 2]);
    };
    std::printf("%s, %s, %zu onsets — onset peak / >8 kHz energy vs input (level-matched): median, worst\n", argv[1], role.c_str(), on.size());
    struct Cfg { const char* name; Plan pl; };
    std::vector<Cfg> cfgs;
    Plan full = s.plan; cfgs.push_back({"whole plan", full});
    auto only = [&](const char* nm, std::initializer_list<int> keep) {
        Plan q; // neutral
        for (int k : keep) q[k] = full[k];
        cfgs.push_back({nm, q});
    };
    only("EQ+tilt", {P_WEIGHT_DB, P_WEIGHT_HZ, P_WARM_DB, P_WARM_HZ, P_BITE_DB, P_BITE_HZ, P_AIR_DB, P_AIR_HZ, P_TILT_DB});
    only("res tamer", {P_RES_DEPTH, P_RES_HZ1, P_RES_HZ2, P_RES_HZ3, P_RES_HZ4});
    only("excess tamer", {P_EXC_DEPTH, P_EXC_HZ, P_EXC_Q, P_EXC_THR, P_EXC_FLOOR});
    only("comp/exp", {P_COMP_AMT, P_COMP_THR, P_COMP_ATT, P_COMP_REL, P_COMP_RATIO, P_COMP_MIX, P_EXP_AMT, P_EXP_THR});
    only("transient", {P_TS_ATTACK, P_TS_SUSTAIN});
    only("sat", {P_SAT_DRIVE, P_SAT_EVEN, P_SAT_REF, P_SAT2_DRIVE});
    only("dyn EQ", {P_DWARM_AMT, P_DBITE_AMT, P_DWARM_THR, P_DBITE_THR, P_WARM_HZ, P_BITE_HZ});
    only("smooth", {P_SMOOTH_AMT});
    only("multiband", {P_MB_LO, P_MB_MID, P_MB_HI, P_MB_F1, P_MB_F2, P_MB_THR1, P_MB_THR2, P_MB_THR3, P_MB_PERIOD});
    only("lowcut", {P_LOWCUT_HZ, P_LOWCUT_ON});
    for (auto& c : cfgs) {
        double pw, hw, rm, rw; auto m = metric(run(src, c.pl), pw, hw, rm, rw);
        std::printf("  %-13s peak %+5.2f / %+5.2f dB   HF %+5.2f / %+5.2f dB   rel %+5.2f / %+5.2f dB\n", c.name, m.first, pw, m.second, hw, rm, rw);
    }
    std::printf("  plan: TS attack %.2f sustain %.2f · comp %.2f att %.1f ms · res depth %.2f · exc depth %.2f @ %.0f Hz · smooth %.2f · dBite %.2f\n",
                full[P_TS_ATTACK], full[P_TS_SUSTAIN], full[P_COMP_AMT], full[P_COMP_ATT], full[P_RES_DEPTH], full[P_EXC_DEPTH], full[P_EXC_HZ], full[P_SMOOTH_AMT], full[P_DBITE_AMT]);
}
