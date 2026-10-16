// Lifegiver: the processing chain (DESIGN §11, fixed order) and the plan that sets it.
//
//   low cut → EQ (Weight shelf, Warm bell, Bite bell, Air shelf) → tilt → resonance tamer (4 dynamic notches)
//   → dynamic EQ (Warm, Bite: cut when the band builds up) → saturation (pre) → compressor/expander → multiband
//   → transient shaper → high smoothing → saturation (post) → high cut → ambience → stereo stage (width: lows never
//   widened; true pan: lows stay put) → make-up + stage level → (Scale > 100 %) level match + safety ceiling
//
// The same class runs on the audio thread (the device) and on the worker (closed-loop renders of the takes), so what
// the solver measures is exactly what plays. All filters are IIR, minimum phase: zero latency.
// A plan is a flat array of numbers (saved in the set as is; new values are appended, so an older plan reads as a
// prefix). Every plan value glides (two one-pole stages, S-shaped, 15 ms; coefficients follow every 16 samples) so a
// new plan never clicks. process() never allocates.
//
// Scale (0 … 4, 1 = the plan): multiplies the shaping (EQ, tilt, saturation, dynamics, transients, dynamic EQ,
// smoothing, multiband), not the placement (cuts, Clean, stage, ambience). Above 1 the level is matched to the input
// (slowly) and a soft ceiling keeps the peaks under 0 dBFS; both fade in between 100 % and 200 %.
// Average mode: the dynamic modules (dynamic EQ, tamer, smoothing, multiband) stop following the sound and hold their
// average gain (measured when the plan was solved, saved in the plan): no detectors run.
#pragma once
#include <cmath>
#include <cstring>

#include "dsp.h"
#include "modules.h"
#include "smooth.h"

namespace lg {

enum PlanIdx {
    P_LOWCUT_HZ, P_LOWCUT_ON,
    P_WEIGHT_DB, P_WEIGHT_HZ, P_WARM_DB, P_WARM_HZ, P_BITE_DB, P_BITE_HZ, P_AIR_DB, P_AIR_HZ,
    P_TILT_DB,
    P_SAT_DRIVE, P_SAT_EVEN, P_SAT_REF,
    P_COMP_AMT, P_COMP_THR, P_COMP_ATT, P_COMP_REL, P_COMP_RATIO, P_COMP_MIX, P_EXP_AMT, P_EXP_THR,
    P_TS_ATTACK, P_TS_SUSTAIN,
    P_HIGHCUT_HZ, P_HIGHCUT_ON,
    P_WIDTH, P_MONO_HZ, P_PAN,
    P_MAKEUP_DB, P_STAGE_DB,
    // 16c
    P_SAT2_DRIVE,
    P_DWARM_AMT, P_DBITE_AMT, P_DWARM_THR, P_DBITE_THR,
    P_SMOOTH_AMT,
    P_MB_LO, P_MB_MID, P_MB_HI, P_MB_F1, P_MB_F2, P_MB_THR1, P_MB_THR2, P_MB_THR3, P_MB_PERIOD,
    P_RES_DEPTH, P_RES_HZ1, P_RES_HZ2, P_RES_HZ3, P_RES_HZ4,
    P_AMB_MIX, P_AMB_SIZE,
    P_AVG_DWARM, P_AVG_DBITE, P_AVG_SMOOTH, P_AVG_MB1, P_AVG_MB2, P_AVG_MB3, P_AVG_RES1, P_AVG_RES2, P_AVG_RES3, P_AVG_RES4,
    NP
};
constexpr int NP_16B = P_STAGE_DB + 1;   // plans saved by 16a/16b
constexpr int NAVG = 10;                 // P_AVG_DWARM … P_AVG_RES4

struct Plan {
    double v[NP];
    Plan() { neutral(); }
    void neutral() {
        for (double& x : v) x = 0;
        v[P_LOWCUT_HZ] = 20; v[P_WEIGHT_HZ] = 120; v[P_WARM_HZ] = 300; v[P_BITE_HZ] = 3000; v[P_AIR_HZ] = 10000;
        v[P_SAT_EVEN] = 0.3; v[P_SAT_REF] = 0.3;
        v[P_COMP_THR] = -20; v[P_COMP_ATT] = 10; v[P_COMP_REL] = 150; v[P_COMP_RATIO] = 4; v[P_COMP_MIX] = 1; v[P_EXP_THR] = -50;
        v[P_HIGHCUT_HZ] = 20000; v[P_WIDTH] = 1; v[P_MONO_HZ] = 120;
        v[P_DWARM_THR] = -6; v[P_DBITE_THR] = -14;
        v[P_MB_F1] = 200; v[P_MB_F2] = 2500; v[P_MB_THR1] = -20; v[P_MB_THR2] = -26; v[P_MB_THR3] = -32; v[P_MB_PERIOD] = 25;
        v[P_AMB_SIZE] = 0.7;
    }
    double& operator[](int i) { return v[i]; }
    double operator[](int i) const { return v[i]; }
};

constexpr int CTRL_BLOCK = 16;
constexpr double PLAN_TAU = 0.015;   // per smoothing stage
constexpr double SCALE_MAX = 4.0;

// how far Scale may take each shaping value (its own limits at 100 % are the solver's)
struct ScaleRange { int idx; double lo, hi; };
static const ScaleRange SCALED[] = {
    {P_WEIGHT_DB, -24, 24}, {P_WARM_DB, -24, 24}, {P_BITE_DB, -24, 24}, {P_AIR_DB, -24, 24}, {P_TILT_DB, -12, 12},
    {P_SAT_DRIVE, 0, 2}, {P_COMP_AMT, 0, 4}, {P_EXP_AMT, 0, 3}, {P_TS_ATTACK, -4, 4}, {P_TS_SUSTAIN, -4, 4},
    {P_SAT2_DRIVE, 0, 2}, {P_DWARM_AMT, 0, 4}, {P_DBITE_AMT, 0, 4}, {P_SMOOTH_AMT, 0, 4},
    {P_MB_LO, 0, 4}, {P_MB_MID, 0, 4}, {P_MB_HI, 0, 4},
};

// what the chain measured about its dynamic modules (their unit gains, averaged over the sound)
struct ChainStats { double avg[NAVG]; };

class Chain {
public:
    // Before the first block (or for an offline render): everything jumps to `p`, filter memories cleared.
    void reset(const Plan& p, double sampleRate) {
        sr = sampleRate > 0 ? sampleRate : 48000;
        const double cr = sr / CTRL_BLOCK;
        for (int i = 0; i < NP; i++) { sm[i].setTime(PLAN_TAU, cr); sm[i].reset(p[i]); cur[i] = p[i]; }
        scaleSm.setTime(0.02, cr); scaleSm.reset(scaleTarget); scale = scaleTarget;
        avgSm.setTime(0.02, cr); avgSm.reset(avgTarget); avgBlend = avgTarget;
        gainSm.setTime(0.004, sr);
        for (int c = 0; c < 2; c++) {
            lowcut[c].reset(); highcut[c].reset();
            for (int b = 0; b < 4; b++) eq[c][b].reset();
            tiltLo[c].reset(); tiltHi[c].reset();
            sat[c].reset(sr); sat2[c].reset(sr);
            for (int k = 0; k < 2; k++) { xoLo[c][k].reset(); xoHi[c][k].reset(); }
        }
        detHp.reset(); detHpR.reset(); tsHp.reset(); tsLp.reset();
        stageMix.setTime(0.01, sr);
        stageMix.reset((p[P_WIDTH] != 1.0 || p[P_PAN] != 0.0) ? 1.0 : 0.0);
        compY1 = compYL = 0; expY1 = expYL = 0;
        tsFast.reset(); tsSlow.reset(); detEnv.reset(); tsGain = 0;
        for (auto& b : res) { b.reset(); b.stat.clear(); }
        for (auto& b : dyn) { b.reset(); b.stat.clear(); }
        smooth.reset(); smooth.stat.clear(); smooth.setup(sr, cr);
        mb.reset(); for (auto& s : mb.stat) s.clear();
        amb.reset(); amb.setup(sr);
        lvIn.set(1500, 1500, sr); lvOut.set(1500, 1500, sr); lvIn.reset(); lvOut.reset();
        levelMatch = 0;
        coefsReady = false;
        ctr = 0;
        updateCoefs();
        gainSm.reset(gainTarget());
    }
    void setTarget(const Plan& p) { for (int i = 0; i < NP; i++) sm[i].set(p[i]); }
    void setScale(double s) { scaleTarget = clampd(s, 0, SCALE_MAX); scaleSm.set(scaleTarget); }
    void setAverage(bool on) { avgTarget = on ? 1.0 : 0.0; avgSm.set(avgTarget); }
    const double* current() const { return cur; }
    ChainStats stats() const {
        ChainStats s;
        s.avg[0] = dyn[0].stat.mean(); s.avg[1] = dyn[1].stat.mean(); s.avg[2] = smooth.stat.mean();
        for (int b = 0; b < 3; b++) s.avg[3 + b] = mb.stat[b].mean();
        for (int k = 0; k < 4; k++) s.avg[6 + k] = res[k].stat.mean();
        return s;
    }

    // in place allowed (each sample is read before it is written)
    void process(const double* inL, const double* inR, double* outL, double* outR, long n) {
        for (long i = 0; i < n; i++) {
            if (ctr == 0) controlTick();
            ctr = (ctr + 1) % CTRL_BLOCK;
            double L = inL[i], R = inR[i];
            { const double m = 0.5 * (L + R); lvIn.tick(m * m); }

            // low cut (dry/wet: at "off" the input passes untouched)
            if (cur[P_LOWCUT_ON] > 0) {
                const double on = cur[P_LOWCUT_ON];
                const double hl = lowcut[0].tick(L), hr = lowcut[1].tick(R);
                L += on * (hl - L); R += on * (hr - R);
            }
            // EQ + tilt (each section is exactly the input at 0 dB)
            if (eqActive) {
                for (int b = 0; b < 4; b++) if (eqOn[b]) { L = eq[0][b].tick(L); R = eq[1][b].tick(R); }
                if (tiltOn) { L = tiltHi[0].tick(tiltLo[0].tick(L)); R = tiltHi[1].tick(tiltLo[1].tick(R)); }
            }
            // resonance tamer: detectors on the tamer's input, then the notches
            if (resActive) {
                if (detecting) { const double m = 0.5 * (L + R); for (int k = 0; k < 4; k++) if (resUsed[k]) res[k].detect(m); }
                for (int k = 0; k < 4; k++) res[k].process(L, R);
            }
            // dynamic EQ (Warm, Bite)
            if (dynActive) {
                if (detecting) { const double m = 0.5 * (L + R); for (int k = 0; k < 2; k++) if (dynUsed[k]) dyn[k].detect(m); }
                dyn[0].process(L, R); dyn[1].process(L, R);
            }
            // saturation: the nonlinear residual only
            if (satMix > 0) {
                L += satMix * sat[0].residual(L, satG, satB);
                R += satMix * sat[1].residual(R, satG, satB);
            }
            // compressor (downward, above threshold) + expander (downward, below its threshold): one gain, dB domain
            if (dynOn) {
                // level: a peak envelope of the low-cut detector signal (instant attack, release about one low
                // period), so the expander never sees the zero crossings as silence
                const double d = detEnv.tick(std::max(std::fabs(detHp.hp(L)), std::fabs(detHpR.hp(R))));
                const double xdb = linToDb(d + 1e-9);
                double cgr = 0;
                if (compRatio > 1.0001) {
                    const double T = compThr, W = 6.0, ov = xdb - T;
                    if (2 * ov > W) cgr = (1 / compRatio - 1) * ov;
                    else if (2 * ov > -W) cgr = (1 / compRatio - 1) * (ov + W / 2) * (ov + W / 2) / (2 * W);
                }
                // smooth decoupled peak detector on the gain reduction (Giannoulis et al.)
                const double c = -cgr;
                compY1 = std::max(c, compRelC * compY1 + (1 - compRelC) * c);
                compYL = compAttC * compYL + (1 - compAttC) * compY1;
                double egr = 0;
                if (expRatio > 1.0001) {
                    const double ov = xdb - cur[P_EXP_THR];
                    if (ov < 0) egr = std::max(ov * (expRatio - 1), -18.0);
                }
                const double e = -egr;
                expY1 = std::max(e, expRelC * expY1 + (1 - expRelC) * e);
                expYL = expAttC * expYL + (1 - expAttC) * expY1;
                const double g = dbToLin(-(compYL + expYL));
                const double k = 1 + compMix * (g - 1);
                L *= k; R *= k;
            }
            // multiband: three bands that sum to the input, a compressor each. Held (Average): two shelves instead
            // of the split; while switching both run and cross over
            if (mbOn) {
                if (mbShelf >= 1.0) mb.shelves(L, R);
                else {
                    double b0[3], b1[3];
                    mb.split(0, L, b0[0], b0[1], b0[2]);
                    mb.split(1, R, b1[0], b1[1], b1[2]);
                    double sL = 0, sR = 0;
                    for (int b = 0; b < 3; b++) {
                        if (detecting) mb.det[b].tick(std::max(std::fabs(b0[b]), std::fabs(b1[b])));
                        mb.gLin[b] += mb.gC * (mb.gT[b] - mb.gLin[b]);
                        sL += b0[b] * mb.gLin[b]; sR += b1[b] * mb.gLin[b];
                    }
                    if (mbShelf > 0.0) {
                        double hL = L, hR = R;
                        mb.shelves(hL, hR);
                        sL += mbShelf * (hL - sL); sR += mbShelf * (hR - sR);
                    }
                    L = sL; R = sR;
                }
            }
            // transient shaper: attack and sustain gains from a fast/slow envelope difference (level independent)
            if (tsOn) {
                const double b = tsLp.tick(tsHp.tick(0.5 * (L + R)));
                const double a = std::fabs(b);
                const double f = tsFast.tick(a), s = tsSlow.tick(a);
                const double diff = linToDb((f + 1e-9) / (s + 1e-9));
                double gdb = 0;
                if (diff > 0) gdb = tsA * std::min(diff, 24.0);
                else gdb = tsS * std::min(-diff, 24.0);
                gdb = clampd(gdb, -30.0, 24.0);
                tsGain += tsGainC * (gdb - tsGain);
                const double g = dbToLin(tsGain);
                L *= g; R *= g;
            }
            // high smoothing (the spikes of the highs)
            if (smoothActive) {
                if (detecting) smooth.detect(0.5 * (L + R));
                smooth.process(L, R);
            }
            // saturation after the dynamics
            if (sat2Mix > 0) {
                L += sat2Mix * sat2[0].residual(L, sat2G, satB);
                R += sat2Mix * sat2[1].residual(R, sat2G, satB);
            }
            // high cut
            if (cur[P_HIGHCUT_ON] > 0) {
                const double on = cur[P_HIGHCUT_ON];
                const double ll = highcut[0].tick(L), lr = highcut[1].tick(R);
                L += on * (ll - L); R += on * (lr - R);
            }
            // ambience: the line always runs (cheap), the reflections are added only when asked
            amb.write(0.5 * (L + R));
            if (ambMix > 0) {
                double wl, wr;
                amb.read(wl, wr);
                L += ambMix * wl; R += ambMix * wr;
            }
            amb.advance();
            // stereo stage: a Linkwitz-Riley split at P_MONO_HZ (the bands sum to an all-pass: no colour). Width acts
            // on mid/side (the lows are never widened); the true pan moves the highs of both channels (the lows
            // stay where they are). The section fades in and out (S-shaped) when it starts or stops being needed,
            // so a neutral stage is exactly the input and switching never clicks.
            const double lL = xoLo[0][1].tick(xoLo[0][0].tick(L)), lR = xoLo[1][1].tick(xoLo[1][0].tick(R));
            const double hL = xoHi[0][1].tick(xoHi[0][0].tick(L)), hR = xoHi[1][1].tick(xoHi[1][0].tick(R));
            const double st = stageMix.next();
            if (st > 0.0) {
                const double w = cur[P_WIDTH];
                const double wl = w < 1 ? w : 1.0;
                const double ml = 0.5 * (lL + lR), sl = 0.5 * (lL - lR) * wl;
                const double mh = 0.5 * (hL + hR), sh = 0.5 * (hL - hR) * w;
                double oLh = mh + sh, oRh = mh - sh;
                if (panP > 0) { const double x = oLh; oLh = x * panC; oRh += x * panS; }
                else if (panP < 0) { const double x = oRh; oRh = x * panC; oLh += x * panS; }
                const double sL = ml + sl + oLh, sR = ml - sl + oRh;
                if (st >= 1.0) { L = sL; R = sR; }
                else { L += st * (sL - L); R += st * (sR - R); }
            }
            { const double m = 0.5 * (L + R); lvOut.tick(m * m); }
            const double g = gainSm.next();
            L *= g; R *= g;
            // safety ceiling (only when Scale is past 100 %): soft above −1 dBFS, never above 0 dBFS
            if (ceilB > 0) { L += ceilB * (ceiling(L) - L); R += ceilB * (ceiling(R) - R); }
            outL[i] = L;
            outR[i] = R;
        }
    }

private:
    double sr = 48000;
    Smoother sm[NP];
    double cur[NP] = {0};
    double eff[NP] = {0};          // cur with Scale applied to the shaping values
    Smoother scaleSm, avgSm;
    double scaleTarget = 1, scale = 1, avgTarget = 0, avgBlend = 0;
    Smoother gainSm;
    Svf lowcut[2], highcut[2], eq[2][4], tiltLo[2], tiltHi[2];
    Sat sat[2], sat2[2];
    OnePole detHp, detHpR;
    Svf xoLo[2][2], xoHi[2][2];   // per channel: Linkwitz-Riley 4th order (two Butterworth stages each)
    Smoother stageMix;            // 0 = the stereo stage is bypassed (exactly the input)
    Svf tsHp, tsLp;
    Env tsFast, tsSlow, detEnv;
    DynBell res[4], dyn[2];
    HiSmooth smooth;
    Multiband mb;
    Ambience amb;
    MsFollow lvIn, lvOut;
    double levelMatch = 0, ceilB = 0;
    double tsGain = 0, tsGainC = 1, tsA = 0, tsS = 0;
    double compY1 = 0, compYL = 0, expY1 = 0, expYL = 0;
    double compAttC = 0, compRelC = 0, expAttC = 0, expRelC = 0, compRatio = 1, expRatio = 1, compMix = 1, compThr = -20;
    double satG = 1, satB = 0, satMix = 0, sat2G = 1, sat2Mix = 0, ambMix = 0;
    double panP = 0, panC = 1, panS = 0;
    bool eqActive = false, eqOn[4] = {false, false, false, false}, tiltOn = false, dynOn = false, tsOn = false;
    bool resActive = false, resUsed[4] = {false, false, false, false}, dynActive = false, dynUsed[2] = {false, false};
    bool smoothActive = false, mbOn = false, detecting = true;
    double mbShelf = 0;            // 1 = the multiband runs as held shelves (Average, settled)
    bool coefsReady = false;
    int ctr = 0;

    static inline double ceiling(double x) {
        const double a = std::fabs(x), T = 0.89;
        if (a <= T) return x;
        const double y = T + (1 - T) * std::tanh((a - T) / (1 - T));
        return x < 0 ? -y : y;
    }
    double gainTarget() const {
        const double mk = cur[P_MAKEUP_DB] * std::min(scale, 1.0);
        const double b = clampd(scale - 1, 0, 1);
        return dbToLin(cur[P_STAGE_DB] + (1 - b) * mk + b * levelMatch);
    }

    void controlTick() {
        bool moving = false;
        for (int i = 0; i < NP; i++) {
            if (!sm[i].settled()) { cur[i] = sm[i].next(); moving = true; }
        }
        if (!scaleSm.settled()) { scale = scaleSm.next(); moving = true; }
        if (!avgSm.settled()) avgBlend = avgSm.next();
        if (moving || !coefsReady) updateCoefs();
        dynamicTick();
        // the level match (Scale above 100 %): the input's level over the output's, while there is sound
        const double b = clampd(scale - 1, 0, 1);
        if (b > 0 && lvIn.y > 1e-7 && lvOut.y > 1e-12) levelMatch = clampd(powToDb(lvIn.y / lvOut.y), -36, 36);
        ceilB = b;
        if (b > 0 || moving) gainSm.set(gainTarget());
        // a module whose amounts reached exactly 0 switches off once its own glide (gain memory) has died out
        if (tsOn && tsA == 0.0 && tsS == 0.0 && std::fabs(tsGain) < 1e-4) { tsOn = false; tsGain = 0; }   // < 0.0001 dB: inaudible
        if (dynOn && compRatio <= 1.0001 && expRatio <= 1.0001 && compYL < 1e-4 && expYL < 1e-4) {
            dynOn = false; compY1 = compYL = expY1 = expYL = 0;
        }
    }

    // the dynamic modules, every control block: their units from the detectors (unless Average holds them), then
    // amount × unit (or amount × the saved average), gliding
    void dynamicTick() {
        const double ab = avgBlend;
        detecting = ab < 1.0;
        auto target = [&](double amt, double unit, double avg) { return amt * ((1 - ab) * unit + ab * avg); };
        // tamer
        const double depth = clampd(cur[P_RES_DEPTH], 0, 1);
        resActive = false;
        for (int k = 0; k < 4; k++) {
            resUsed[k] = cur[P_RES_HZ1 + k] > 20;
            if (!resUsed[k]) { if (res[k].on) { res[k].apply(0); resActive = resActive || res[k].on; } continue; }
            if (detecting) res[k].control(-9.0 + 1.5, 4.0, 8.0, true);
            res[k].apply(clampd(target(depth, res[k].unit, cur[P_AVG_RES1 + k]), -12, 0));
            resActive = resActive || res[k].on || depth > 0;
        }
        // dynamic EQ
        dynActive = false;
        const double amts[2] = {eff[P_DWARM_AMT], eff[P_DBITE_AMT]}, thr[2] = {cur[P_DWARM_THR], cur[P_DBITE_THR]};
        for (int k = 0; k < 2; k++) {
            dynUsed[k] = amts[k] > 0 || dyn[k].on;
            if (!dynUsed[k]) continue;
            if (detecting) dyn[k].control(thr[k], 6.0, 12.0, true);
            dyn[k].apply(clampd(target(amts[k], dyn[k].unit, cur[P_AVG_DWARM + k]), -30, 0));
            dynActive = dynActive || dyn[k].on || amts[k] > 0;
        }
        // smoothing
        const double sa = eff[P_SMOOTH_AMT];
        if (sa > 0 || smooth.on) {
            if (detecting) smooth.control(true);
            smooth.set(sa * (1 - ab), clampd(sa * ab * cur[P_AVG_SMOOTH], -30, 0));
            smoothActive = smooth.on;
        } else smoothActive = false;
        // multiband
        const double ma[3] = {eff[P_MB_LO], eff[P_MB_MID], eff[P_MB_HI]};
        const bool want = ma[0] > 0 || ma[1] > 0 || ma[2] > 0;
        if (want && !mbOn) { mb.reset(); setupMb(); mbOn = true; }
        if (mbOn) {
            mb.control(detecting, true);
            double t[3], held[3];
            bool shelfMoved = false;
            for (int b = 0; b < 3; b++) {
                t[b] = clampd(target(ma[b], mb.unit[b], cur[P_AVG_MB1 + b]), -36, 0);
                held[b] = clampd(ma[b] * cur[P_AVG_MB1 + b], -36, 0);
                shelfMoved = shelfMoved || std::fabs(held[b] - mb.shG[b]) > 1e-4;
            }
            mb.apply(t);
            if (shelfMoved) mb.setShelves(held);
            mbShelf = ab;
            // off once every band is back at exactly 0 dB (the bands then sum to the input exactly) and no shelf is held
            if (!want && mb.idle() && mb.shG[0] == 0.0 && mb.shG[1] == 0.0 && mb.shG[2] == 0.0) mbOn = false;
        }
    }

    void setupMb() {
        mb.thr[0] = cur[P_MB_THR1]; mb.thr[1] = cur[P_MB_THR2]; mb.thr[2] = cur[P_MB_THR3];
        mb.setup(clampd(cur[P_MB_F1], 60, 600), clampd(cur[P_MB_F2], 1000, 8000), std::max(cur[P_MB_PERIOD], 1.0), sr, sr / CTRL_BLOCK);
    }

    void updateCoefs() {
        coefsReady = true;
        for (int i = 0; i < NP; i++) eff[i] = cur[i];
        for (const ScaleRange& s : SCALED) eff[s.idx] = clampd(cur[s.idx] * scale, s.lo, s.hi);
        if (cur[P_LOWCUT_ON] > 0) for (int c = 0; c < 2; c++) lowcut[c].set(Svf::HP, cur[P_LOWCUT_HZ], 0.7071, 0, sr);
        if (cur[P_HIGHCUT_ON] > 0) for (int c = 0; c < 2; c++) highcut[c].set(Svf::LP, cur[P_HIGHCUT_HZ], 0.7071, 0, sr);
        static const int dbIdx[4] = {P_WEIGHT_DB, P_WARM_DB, P_BITE_DB, P_AIR_DB};
        static const int hzIdx[4] = {P_WEIGHT_HZ, P_WARM_HZ, P_BITE_HZ, P_AIR_HZ};
        static const Svf::Mode modes[4] = {Svf::LOWSHELF, Svf::BELL, Svf::BELL, Svf::HIGHSHELF};
        static const double qs[4] = {0.7071, 0.8, 0.9, 0.7071};
        eqActive = false;
        for (int b = 0; b < 4; b++) {
            eqOn[b] = eff[dbIdx[b]] != 0.0;
            // a band that was on keeps running its filter until it is exactly 0 dB (then it is exactly the input)
            if (eqOn[b]) { eq[0][b].set(modes[b], cur[hzIdx[b]], qs[b], eff[dbIdx[b]], sr); eq[1][b].copyCoefs(eq[0][b]); eqActive = true; }
            else for (int c = 0; c < 2; c++) eq[c][b].reset();
        }
        tiltOn = eff[P_TILT_DB] != 0.0;
        if (tiltOn) {
            tiltLo[0].set(Svf::LOWSHELF, 1000, 0.5, -0.5 * eff[P_TILT_DB], sr); tiltLo[1].copyCoefs(tiltLo[0]);
            tiltHi[0].set(Svf::HIGHSHELF, 1000, 0.5, 0.5 * eff[P_TILT_DB], sr); tiltHi[1].copyCoefs(tiltHi[0]);
            eqActive = true;
        } else for (int c = 0; c < 2; c++) { tiltLo[c].reset(); tiltHi[c].reset(); }
        // saturation: drive 0..1 → gain into tanh relative to the measured signal level (gain staging, DESIGN §12.6);
        // Scale may take the drive to 2 (+26 dB over the typical peak)
        auto mixOf = [](double d) { return d <= 0 ? 0.0 : (d >= 0.1 ? 1.0 : (3 - 2 * (d / 0.1)) * (d / 0.1) * (d / 0.1)); };
        const double ref = std::max(cur[P_SAT_REF], 1e-4);
        const double d = clampd(eff[P_SAT_DRIVE], 0, 2);
        satMix = mixOf(d);
        satG = dbToLin(-6 + 16 * d) / ref;
        satB = 0.6 * clampd(cur[P_SAT_EVEN], 0, 1);
        const double d2 = clampd(eff[P_SAT2_DRIVE], 0, 2);
        sat2Mix = mixOf(d2);
        sat2G = dbToLin(-6 + 16 * d2) / ref;
        // dynamics
        const double ca = clampd(eff[P_COMP_AMT], 0, 4);
        compRatio = 1 + (std::max(cur[P_COMP_RATIO], 1.0) - 1) * ca;
        compThr = cur[P_COMP_THR] - 6 * ca;
        expRatio = 1 + 2 * clampd(eff[P_EXP_AMT], 0, 3);
        compMix = clampd(cur[P_COMP_MIX], 0, 1);
        compAttC = std::exp(-1.0 / (std::max(cur[P_COMP_ATT], 0.1) * 0.001 * sr));
        compRelC = std::exp(-1.0 / (std::max(cur[P_COMP_REL], 1.0) * 0.001 * sr));
        expAttC = std::exp(-1.0 / (0.002 * sr));
        expRelC = std::exp(-1.0 / (std::max(cur[P_COMP_REL], 1.0) * 0.5 * 0.001 * sr));
        dynOn = compRatio > 1.0001 || expRatio > 1.0001 || compYL > 1e-9 || expYL > 1e-9;
        detHp.setLP(60, sr); detHpR.setLP(60, sr);
        detEnv.set(0.1, 30, sr);
        // transient shaper
        tsA = clampd(eff[P_TS_ATTACK], -4, 4);
        tsS = clampd(eff[P_TS_SUSTAIN], -4, 4);
        tsOn = tsA != 0.0 || tsS != 0.0 || std::fabs(tsGain) > 1e-9;
        tsHp.set(Svf::HP, 50, 0.7071, 0, sr);
        tsLp.set(Svf::LP, 1500, 0.7071, 0, sr);
        tsFast.set(0.5, 20, sr);
        tsSlow.set(30, 150, sr);
        tsGainC = coefFor(1.0, sr);
        // the dynamic modules' frequencies and speeds
        const double cr = sr / CTRL_BLOCK;
        for (int k = 0; k < 4; k++) if (cur[P_RES_HZ1 + k] > 20) res[k].setup(cur[P_RES_HZ1 + k], 8.0, 1.0, 6.0, 3, 120, sr, cr);
        dyn[0].setup(cur[P_WARM_HZ], 1.0, 0, 0.8, 15, 200, sr, cr);
        dyn[1].setup(cur[P_BITE_HZ], 1.0, 0, 0.9, 3, 80, sr, cr);
        if (mbOn) setupMb();
        // ambience: mix 0..0.5, the room's size from distance; darker further back
        ambMix = clampd(cur[P_AMB_MIX], 0, 0.5);
        amb.setSize(clampd(cur[P_AMB_SIZE], 0.4, 1.2), 7000 - 3000 * clampd(cur[P_AMB_SIZE], 0.4, 1.2));
        // stereo
        for (int c = 0; c < 2; c++)
            for (int k = 0; k < 2; k++) {
                xoLo[c][k].set(Svf::LP, cur[P_MONO_HZ], 0.7071, 0, sr);
                xoHi[c][k].set(Svf::HP, cur[P_MONO_HZ], 0.7071, 0, sr);
            }
        panP = clampd(cur[P_PAN], -1, 1);
        panC = std::cos(std::fabs(panP) * kPi / 2);
        panS = std::sin(std::fabs(panP) * kPi / 2);
        stageMix.set((cur[P_WIDTH] != 1.0 || panP != 0.0) ? 1.0 : 0.0);
        gainSm.set(gainTarget());
    }
};

}  // namespace lg
