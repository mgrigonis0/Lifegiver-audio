// Lifegiver: the processing chain (DESIGN §11, fixed order) and the plan that sets it.
//
//   low cut → EQ (Weight shelf, Warm bell, Bite bell, Air shelf) → tilt → saturation (pre) → compressor/expander
//   → transient shaper → high cut → stereo stage (width: lows never widened; true pan: lows stay put) → make-up + stage level
//
// The same class runs on the audio thread (the device) and on the worker (closed-loop renders of the takes), so what
// the solver measures is exactly what plays. All filters are IIR, minimum phase: zero latency.
// A plan is a flat array of numbers (saved in the set as is). Every plan value glides (two one-pole stages,
// S-shaped, 15 ms; coefficients follow every 16 samples) so a new plan never clicks. process() never allocates.
#pragma once
#include <cmath>
#include <cstring>

#include "dsp.h"
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
    NP
};

struct Plan {
    double v[NP];
    Plan() { neutral(); }
    void neutral() {
        for (double& x : v) x = 0;
        v[P_LOWCUT_HZ] = 20; v[P_WEIGHT_HZ] = 120; v[P_WARM_HZ] = 300; v[P_BITE_HZ] = 3000; v[P_AIR_HZ] = 10000;
        v[P_SAT_EVEN] = 0.3; v[P_SAT_REF] = 0.3;
        v[P_COMP_THR] = -20; v[P_COMP_ATT] = 10; v[P_COMP_REL] = 150; v[P_COMP_RATIO] = 4; v[P_COMP_MIX] = 1; v[P_EXP_THR] = -50;
        v[P_HIGHCUT_HZ] = 20000; v[P_WIDTH] = 1; v[P_MONO_HZ] = 120;
    }
    double& operator[](int i) { return v[i]; }
    double operator[](int i) const { return v[i]; }
};

constexpr int CTRL_BLOCK = 16;
constexpr double PLAN_TAU = 0.015;   // per smoothing stage

class Chain {
public:
    // Before the first block (or for an offline render): everything jumps to `p`, filter memories cleared.
    void reset(const Plan& p, double sampleRate) {
        sr = sampleRate > 0 ? sampleRate : 48000;
        for (int i = 0; i < NP; i++) { sm[i].setTime(PLAN_TAU, sr / CTRL_BLOCK); sm[i].reset(p[i]); cur[i] = p[i]; }
        gainSm.setTime(0.004, sr);
        gainSm.reset(dbToLin(p[P_MAKEUP_DB] + p[P_STAGE_DB]));
        for (int c = 0; c < 2; c++) {
            lowcut[c].reset(); highcut[c].reset();
            for (int b = 0; b < 4; b++) eq[c][b].reset();
            tiltLo[c].reset(); tiltHi[c].reset();
            sat[c].reset(sr);
            for (int k = 0; k < 2; k++) { xoLo[c][k].reset(); xoHi[c][k].reset(); }
        }
        detHp.reset(); detHpR.reset(); tsHp.reset(); tsLp.reset();
        stageMix.setTime(0.01, sr);
        stageMix.reset((p[P_WIDTH] != 1.0 || p[P_PAN] != 0.0) ? 1.0 : 0.0);
        compY1 = compYL = 0; expY1 = expYL = 0;
        tsFast.reset(); tsSlow.reset(); detEnv.reset(); tsGain = 0;
        coefsReady = false;
        ctr = 0;
        updateCoefs();
    }
    void setTarget(const Plan& p) { for (int i = 0; i < NP; i++) sm[i].set(p[i]); }
    const double* current() const { return cur; }

    // in place allowed (each sample is read before it is written)
    void process(const double* inL, const double* inR, double* outL, double* outR, long n) {
        for (long i = 0; i < n; i++) {
            if (ctr == 0) controlTick();
            ctr = (ctr + 1) % CTRL_BLOCK;
            double L = inL[i], R = inR[i];

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
                    const double T = cur[P_COMP_THR] - 6 * clampd(cur[P_COMP_AMT], 0, 1), W = 6.0, ov = xdb - T;
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
            // transient shaper: attack and sustain gains from a fast/slow envelope difference (level independent)
            if (tsOn) {
                const double b = tsLp.tick(tsHp.tick(0.5 * (L + R)));
                const double a = std::fabs(b);
                const double f = tsFast.tick(a), s = tsSlow.tick(a);
                const double diff = linToDb(f + 1e-9) - linToDb(s + 1e-9);
                double gdb = 0;
                if (diff > 0) gdb = tsA * std::min(diff, 24.0);
                else gdb = tsS * std::min(-diff, 24.0);
                gdb = clampd(gdb, -24.0, 18.0);
                tsGain += tsGainC * (gdb - tsGain);
                const double g = dbToLin(tsGain);
                L *= g; R *= g;
            }
            // high cut
            if (cur[P_HIGHCUT_ON] > 0) {
                const double on = cur[P_HIGHCUT_ON];
                const double ll = highcut[0].tick(L), lr = highcut[1].tick(R);
                L += on * (ll - L); R += on * (lr - R);
            }
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
            const double g = gainSm.next();
            outL[i] = L * g;
            outR[i] = R * g;
        }
    }

private:
    double sr = 48000;
    Smoother sm[NP];
    double cur[NP] = {0};
    Smoother gainSm;
    Svf lowcut[2], highcut[2], eq[2][4], tiltLo[2], tiltHi[2];
    Sat sat[2];
    OnePole detHp, detHpR;
    Svf xoLo[2][2], xoHi[2][2];   // per channel: Linkwitz-Riley 4th order (two Butterworth stages each)
    Smoother stageMix;            // 0 = the stereo stage is bypassed (exactly the input)
    Svf tsHp, tsLp;
    Env tsFast, tsSlow, detEnv;
    double tsGain = 0, tsGainC = 1, tsA = 0, tsS = 0;
    double compY1 = 0, compYL = 0, expY1 = 0, expYL = 0;
    double compAttC = 0, compRelC = 0, expAttC = 0, expRelC = 0, compRatio = 1, expRatio = 1, compMix = 1;
    double satG = 1, satB = 0, satMix = 0;
    double panP = 0, panC = 1, panS = 0;
    bool eqActive = false, eqOn[4] = {false, false, false, false}, tiltOn = false, dynOn = false, tsOn = false;
    bool coefsReady = false;
    int ctr = 0;

    void controlTick() {
        bool moving = false;
        for (int i = 0; i < NP; i++) {
            if (!sm[i].settled()) { cur[i] = sm[i].next(); moving = true; }
        }
        if (moving || !coefsReady) updateCoefs();
        // a module whose amounts reached exactly 0 switches off once its own glide (gain memory) has died out
        if (tsOn && tsA == 0.0 && tsS == 0.0 && std::fabs(tsGain) < 1e-4) { tsOn = false; tsGain = 0; }   // < 0.0001 dB: inaudible
        if (dynOn && compRatio <= 1.0001 && expRatio <= 1.0001 && compYL < 1e-4 && expYL < 1e-4) {
            dynOn = false; compY1 = compYL = expY1 = expYL = 0;
        }
    }

    void updateCoefs() {
        coefsReady = true;
        if (cur[P_LOWCUT_ON] > 0) for (int c = 0; c < 2; c++) lowcut[c].set(Svf::HP, cur[P_LOWCUT_HZ], 0.7071, 0, sr);
        if (cur[P_HIGHCUT_ON] > 0) for (int c = 0; c < 2; c++) highcut[c].set(Svf::LP, cur[P_HIGHCUT_HZ], 0.7071, 0, sr);
        static const int dbIdx[4] = {P_WEIGHT_DB, P_WARM_DB, P_BITE_DB, P_AIR_DB};
        static const int hzIdx[4] = {P_WEIGHT_HZ, P_WARM_HZ, P_BITE_HZ, P_AIR_HZ};
        static const Svf::Mode modes[4] = {Svf::LOWSHELF, Svf::BELL, Svf::BELL, Svf::HIGHSHELF};
        static const double qs[4] = {0.7071, 0.8, 0.9, 0.7071};
        eqActive = false;
        for (int b = 0; b < 4; b++) {
            eqOn[b] = cur[dbIdx[b]] != 0.0;
            // a band that was on keeps running its filter until it is exactly 0 dB (then it is exactly the input)
            if (eqOn[b]) { for (int c = 0; c < 2; c++) eq[c][b].set(modes[b], cur[hzIdx[b]], qs[b], cur[dbIdx[b]], sr); eqActive = true; }
            else for (int c = 0; c < 2; c++) eq[c][b].reset();
        }
        tiltOn = cur[P_TILT_DB] != 0.0;
        if (tiltOn) {
            for (int c = 0; c < 2; c++) {
                tiltLo[c].set(Svf::LOWSHELF, 1000, 0.5, -0.5 * cur[P_TILT_DB], sr);
                tiltHi[c].set(Svf::HIGHSHELF, 1000, 0.5, 0.5 * cur[P_TILT_DB], sr);
            }
            eqActive = true;
        } else for (int c = 0; c < 2; c++) { tiltLo[c].reset(); tiltHi[c].reset(); }
        // saturation: drive 0..1 → gain into tanh relative to the measured signal level (gain staging, DESIGN §12.6)
        const double d = clampd(cur[P_SAT_DRIVE], 0, 1);
        satMix = d <= 0 ? 0.0 : (d >= 0.1 ? 1.0 : (3 - 2 * (d / 0.1)) * (d / 0.1) * (d / 0.1));
        satG = dbToLin(-6 + 16 * d) / std::max(cur[P_SAT_REF], 1e-4);   // up to +10 dB over the typical peak
        satB = 0.6 * clampd(cur[P_SAT_EVEN], 0, 1);
        // dynamics
        compRatio = 1 + (std::max(cur[P_COMP_RATIO], 1.0) - 1) * clampd(cur[P_COMP_AMT], 0, 1);
        expRatio = 1 + 2 * clampd(cur[P_EXP_AMT], 0, 1);
        compMix = clampd(cur[P_COMP_MIX], 0, 1);
        compAttC = std::exp(-1.0 / (std::max(cur[P_COMP_ATT], 0.1) * 0.001 * sr));
        compRelC = std::exp(-1.0 / (std::max(cur[P_COMP_REL], 1.0) * 0.001 * sr));
        expAttC = std::exp(-1.0 / (0.002 * sr));
        expRelC = std::exp(-1.0 / (std::max(cur[P_COMP_REL], 1.0) * 0.5 * 0.001 * sr));
        dynOn = compRatio > 1.0001 || expRatio > 1.0001 || compYL > 1e-9 || expYL > 1e-9;
        detHp.setLP(60, sr); detHpR.setLP(60, sr);
        detEnv.set(0.1, 30, sr);
        // transient shaper
        tsA = clampd(cur[P_TS_ATTACK], -1, 1);
        tsS = clampd(cur[P_TS_SUSTAIN], -1, 1);
        tsOn = tsA != 0.0 || tsS != 0.0 || std::fabs(tsGain) > 1e-9;
        tsHp.set(Svf::HP, 50, 0.7071, 0, sr);
        tsLp.set(Svf::LP, 1500, 0.7071, 0, sr);
        tsFast.set(0.5, 20, sr);
        tsSlow.set(30, 150, sr);
        tsGainC = coefFor(1.0, sr);
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
        gainSm.set(dbToLin(cur[P_MAKEUP_DB] + cur[P_STAGE_DB]));
    }
};

}  // namespace lg
