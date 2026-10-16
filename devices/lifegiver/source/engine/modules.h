// Lifegiver: the dynamic modules of the chain (16c). Each one is zero latency (IIR, minimum phase), never allocates
// after construction, and is exactly the input when its amount is 0 (its filters are bypassed or sum to the input).
//
// Every dynamic module computes a "unit" gain from its detector: the gain it would apply at amount 1 (dB, ≤ 0).
// The applied gain is amount × unit, so the effect is linear in the amount (Scale and the solver rely on it), and the
// average of the unit over a sound is what the Average mode holds (no detector running: a static gain).
//
// - DynBell: a bell that cuts when its band rises above its usual share of the sound (dynamic EQ: mud, harshness),
//   or, with a narrow detector inside a wide one, when a resonance rings out (the resonance tamer).
// - HiSmooth: rounds off the spikes of the highs (grit, crackle, clicks) against the highs' running level.
// - Multiband: three bands (split by subtraction: they sum to exactly the input), a compressor on each.
// - Ambience: early reflections (6 taps per side + a small diffuser) and a short dense tail (8-line feedback delay
//   network), for distance. Placement, not shaping.
#pragma once
#include <cmath>
#include <cstring>
#include <vector>

#include "dsp.h"

namespace lg {

// mean-square follower; read in dB at control rate
struct MsFollow {
    double y = 0, a = 1, r = 1;
    void set(double attMs, double relMs, double sr) { a = coefFor(attMs, sr); r = coefFor(relMs, sr); }
    void reset() { y = 0; }
    inline void tick(double x2) { y += (x2 > y ? a : r) * (x2 - y); if (y < 1e-30) y = 0; }
    double db() const { return powToDb(y + 1e-20); }
};

// a gain in dB gliding at control rate: toward more cut at the attack speed, back at the release speed
struct GainGlide {
    double g = 0, a = 1, r = 1;
    void set(double attMs, double relMs, double ctrlRate) { a = coefFor(attMs, ctrlRate); r = coefFor(relMs, ctrlRate); }
    void reset() { g = 0; }
    inline double step(double target) {
        g += (target < g ? a : r) * (target - g);
        if (std::fabs(g - target) < 1e-6) g = target;
        return g;
    }
};

// running average of a module's unit gain over the sound (for the Average mode); only while there is sound
struct UnitStat {
    double sum = 0;
    long n = 0;
    void add(double u) { sum += u; n++; }
    double mean() const { return n ? sum / (double)n : 0.0; }
    void clear() { sum = 0; n = 0; }
};

// ------------------------------------------------------------------------------------------------- dynamic bell
struct DynBell {
    Svf bell[2], det, ref;
    MsFollow nb, wb;
    GainGlide gl;
    double f = 1000, bellQ = 1, sr = 48000;
    double unit = 0;       // dB at amount 1, from the detector (≤ 0)
    double applied = 0;    // dB now in the bell
    bool on = false, wide = false;
    UnitStat stat;

    // detQ: the detector's band; wideQ > 0: the reference is a wider band at the same frequency (a resonance), else
    // the whole sound (a band's share)
    void setup(double freq, double detQ, double wideQ, double bq, double attMs, double relMs, double rate, double ctrlRate) {
        sr = rate; f = freq; bellQ = bq; wide = wideQ > 0;
        det.set(Svf::BPN, f, detQ, 0, sr);
        if (wide) ref.set(Svf::BPN, f, wideQ, 0, sr);
        nb.set(2, 60, sr); wb.set(2, 60, sr);
        gl.set(attMs, relMs, ctrlRate);
    }
    void reset() {
        for (auto& b : bell) b.reset();
        det.reset(); ref.reset(); nb.reset(); wb.reset(); gl.reset();
        unit = 0; applied = 0; on = false;
    }
    inline void detect(double mid) {
        const double b = det.tick(mid);
        nb.tick(b * b);
        const double w = wide ? ref.tick(mid) : mid;
        wb.tick(w * w);
    }
    // the unit cut from the band's excess over its usual share `thr` (dB), full cut `range` dB above it; quiet = none
    void control(double thr, double range, double maxCut, bool measure) {
        const double level = wb.db();
        if (level < -80) { unit = 0; return; }
        const double exc = nb.db() - level;
        unit = -maxCut * clampd((exc - thr) / range, 0, 1);
        if (measure && level > -60) stat.add(unit);
    }
    // move toward `targetDb` (amount × unit, or the held average) and set the bell
    void apply(double targetDb) {
        const double g = gl.step(targetDb);
        if (g == 0.0 && targetDb == 0.0) {
            if (on) { on = false; bell[0].reset(); bell[1].reset(); }
            applied = 0;
            return;
        }
        on = true;
        if (std::fabs(g - applied) > 1e-4 || applied == 0.0) {
            applied = g;
            bell[0].set(Svf::BELL, f, bellQ, g, sr);
            bell[1].copyCoefs(bell[0]);
        }
    }
    inline void process(double& L, double& R) {
        if (!on) return;
        L = bell[0].tick(L);
        R = bell[1].tick(R);
    }
};

// ------------------------------------------------------------------------------------------------- high smoothing
// The highs (above ~1.2 kHz) split off (x = lo + hi exactly); their spikes over the highs' usual peak level are
// rounded off at once (untouched up to the clip level, then a soft knee to 1.4 ×; no look-ahead needed): grit, crackle, scratch, clicks. The usual peak level is
// a running 80th percentile of the 1/3 ms peaks, so a note's own (recurring) peaks set it and rare spikes don't:
// steady partials pass under the clip. More amount = a lower clip. The unit (for Average) is the energy the clip
// takes out of the highs, in dB.
struct HiSmooth {
    Svf hp[2], dhp;
    double typ = 0, bmax = 0, upC = 0.02, dnC = 0.005;
    double T = 1, k = 2.0;
    double aT = 0, gT = 1, aCur = 0, gCur = 1, c = 1;
    double eIn = 0, eOut = 0, unit = 0;
    bool on = false;
    UnitStat stat;
    void setup(double sr, double ctrlRate) {
        for (auto& h : hp) h.set(Svf::HP, 1200, 0.7071, 0, sr);
        dhp.set(Svf::HP, 1200, 0.7071, 0, sr);
        // per control block: ~15 ms to rise, ~60 ms to fall (at 48 kHz / 16)
        upC = coefFor(15, ctrlRate) * 4; dnC = coefFor(60, ctrlRate);
        c = coefFor(1.0, sr);
    }
    void reset() { for (auto& h : hp) h.reset(); dhp.reset(); typ = bmax = 0; T = 1; aT = aCur = 0; gT = gCur = 1; eIn = eOut = unit = 0; on = false; }
    static inline double clip(double h, double t) {
        const double a = std::fabs(h);
        if (a <= t) return h;
        const double y = t + 0.4 * t * std::tanh((a - t) / (0.4 * t));
        return h < 0 ? -y : y;
    }
    inline void detect(double mid) {
        const double h = dhp.tick(mid);
        const double a = std::fabs(h);
        if (a > bmax) bmax = a;
        const double y = clip(h, T);
        eIn += h * h; eOut += y * y;
    }
    void control(bool measure) {
        if (eIn > 1e-24) {
            unit = std::min(0.0, powToDb(eOut / eIn));
            if (measure && eIn > 1e-12) stat.add(unit);
        }
        eIn = eOut = 0;
        typ += (bmax > typ ? std::min(upC, 1.0) : dnC) * (bmax - typ);
        bmax = 0;
        T = 1.5 * typ + 1e-9;
    }
    // amount (0 … 4) of the clip while it follows the sound, and the held average (dB) while it doesn't
    void set(double amtDyn, double avgDb) {
        aT = std::min(amtDyn, 1.0);
        k = 1.0 / std::max(1.0, amtDyn);
        gT = dbToLin(avgDb);
        if (aT == 0.0 && gT == 1.0 && std::fabs(aCur) < 1e-9 && std::fabs(gCur - 1) < 1e-9) {
            if (on) { on = false; hp[0].reset(); hp[1].reset(); }
            aCur = 0; gCur = 1;
            return;
        }
        on = true;
    }
    inline void process(double& L, double& R) {
        if (!on) return;
        aCur += c * (aT - aCur);
        gCur += c * (gT - gCur);
        const double t = T * k;
        const double hl = hp[0].tick(L), hr = hp[1].tick(R);
        L += aCur * (clip(hl, t) - hl) + (gCur - 1) * hl;
        R += aCur * (clip(hr, t) - hr) + (gCur - 1) * hr;
    }
};

// ------------------------------------------------------------------------------------------------- multiband
// lo | mid | hi at f1 and f2, split by subtraction: lo = a 4th-order low pass, mid = the low pass of the rest, hi =
// what remains. The three bands always sum to exactly the input (no phase shift, no colour), so a band gain near
// 1 is near the input and the amount is continuous from 0 (an all-pass crossover would jump at the first touch).
// A compressor per band (peak detector, soft knee, ratio 3), gain = amount × its unit.
struct Multiband {
    Svf lp1[2][2], lp2[2][2];
    Svf shLo[2], shHi[2];   // Average mode: the held band gains as two shelves (cheaper than the split)
    double f1 = 200, f2 = 2500, sr = 48000, shG[3] = {0, 0, 0}, shGain = 1;
    Env det[3];
    GainGlide gl[3];
    double unit[3] = {0, 0, 0}, applied[3] = {0, 0, 0}, gLin[3] = {1, 1, 1}, gT[3] = {1, 1, 1}, gC = 1;
    double thr[3] = {-20, -26, -32};
    UnitStat stat[3];
    void setup(double fa, double fb, double minPeriodMs, double rate, double ctrlRate) {
        f1 = fa; f2 = fb; sr = rate;
        for (int c = 0; c < 2; c++)
            for (int k = 0; k < 2; k++) { lp1[c][k].set(Svf::LP, f1, 0.7071, 0, sr); lp2[c][k].set(Svf::LP, f2, 0.7071, 0, sr); }
        det[0].set(1, std::max(120.0, 5 * minPeriodMs), sr);
        det[1].set(0.5, 80, sr);
        det[2].set(0.2, 50, sr);
        gl[0].set(std::max(20.0, 0.5 * minPeriodMs), 250, ctrlRate);
        gl[1].set(10, 150, ctrlRate);
        gl[2].set(5, 100, ctrlRate);
        gC = coefFor(0.5, sr);
        setShelves(shG);
    }
    void reset() {
        for (int c = 0; c < 2; c++) for (int k = 0; k < 2; k++) { lp1[c][k].reset(); lp2[c][k].reset(); }
        for (int b = 0; b < 3; b++) { det[b].reset(); gl[b].reset(); unit[b] = applied[b] = 0; gLin[b] = gT[b] = 1; shG[b] = 0; }
        for (int c = 0; c < 2; c++) { shLo[c].reset(); shHi[c].reset(); }
        shGain = 1;
        setShelves(shG);
    }
    // the held gains (dB per band) as shelves: low shelf (lo vs mid) at f1, high shelf (hi vs mid) at f2, mid as a gain
    void setShelves(const double* g) {
        for (int b = 0; b < 3; b++) shG[b] = g[b];
        shLo[0].set(Svf::LOWSHELF, f1, 0.7071, g[0] - g[1], sr); shLo[1].copyCoefs(shLo[0]);
        shHi[0].set(Svf::HIGHSHELF, f2, 0.7071, g[2] - g[1], sr); shHi[1].copyCoefs(shHi[0]);
        shGain = dbToLin(g[1]);
    }
    inline void shelves(double& L, double& R) {
        L = shGain * shHi[0].tick(shLo[0].tick(L));
        R = shGain * shHi[1].tick(shLo[1].tick(R));
    }
    // split one channel (lo + mid + hi = x exactly)
    inline void split(int c, double x, double& lo, double& mid, double& hi) {
        lo = lp1[c][1].tick(lp1[c][0].tick(x));
        const double r = x - lo;
        mid = lp2[c][1].tick(lp2[c][0].tick(r));
        hi = r - mid;
    }
    void control(bool detectOn, bool measure) {
        for (int b = 0; b < 3; b++) {
            if (!detectOn) continue;
            const double x = linToDb(det[b].y + 1e-9), ov = x - thr[b], W = 6.0, s = 1.0 / 3.0 - 1.0;
            double gr = 0;
            if (2 * ov > W) gr = s * ov;
            else if (2 * ov > -W) gr = s * (ov + W / 2) * (ov + W / 2) / (2 * W);
            unit[b] = std::max(gr, -24.0);
            if (measure && x > thr[b] - 40) stat[b].add(unit[b]);
        }
    }
    void apply(const double* targetDb) {
        for (int b = 0; b < 3; b++) { applied[b] = gl[b].step(targetDb[b]); gT[b] = dbToLin(applied[b]); }
    }
    bool idle() const {
        for (int b = 0; b < 3; b++) if (applied[b] != 0.0 || std::fabs(gLin[b] - 1) > 1e-9) return false;
        return true;
    }
};

// ------------------------------------------------------------------------------------------------- ambience
// A small room for distance (16d). The sound's mid (lows kept out) feeds two parts, added to the dry signal by `mix`
// (0 = exactly the input):
// - early reflections: 6 taps per side from a plain delay line (no feedback), softened, then a small all-pass diffuser;
// - a short dense tail: 16 delay lines mixed through a 16x16 Hadamard matrix (a feedback delay network). A dense,
//   orthogonal mix spreads the energy over thousands of modes, so no pitch builds up; each line's loss filter makes
//   the highs die faster than the lows. (16c's tail was one line feeding itself: a comb that rang at fixed pitches
//   for over a second.)
// The delay lengths are fixed (mutually prime, chosen at setup): size only changes the decay time and the darkness, so
// moving the stage never re-tunes a line (no zipper, no click). All lines are allocated once (at most ~85 ms at
// 192 kHz; the 16 tail lines ~2 MB in all), never on the audio thread.
struct Ambience {
    static constexpr int N = 16384, D = 2048, NL = 16, LN = 16384;
    std::vector<double> buf, dif[2][2], line[NL];
    int w = 0, dw = 0, lw = 0;
    double sr = 48000;
    Svf hp;
    OnePole damp[2];
    int tapL[6] = {0}, tapR[6] = {0};
    int difLen[2][2] = {{1, 1}, {1, 1}};
    int len[NL] = {0};
    double gLo[NL] = {0}, pole[NL] = {0}, lz[NL] = {0}, fbOut[NL] = {0};
    double size = -1, t60 = 0.3;
    static constexpr double TL[6] = {7.1, 11.3, 17.9, 23.3, 31.7, 41.1};
    static constexpr double TR[6] = {8.9, 13.7, 19.1, 27.4, 35.3, 44.6};
    static constexpr double G[6] = {0.62, -0.5, 0.42, -0.34, 0.27, -0.21};
    static constexpr double LMS[NL] = {19.1, 20.9, 23.3, 25.1, 27.7, 29.9, 32.3, 34.7,
                                       37.3, 39.7, 42.1, 44.9, 47.3, 49.9, 52.7, 55.3};   // tail lines, ms
    static constexpr double ER_SIZE = 0.85;     // the reflections' spacing (fixed)
    static constexpr double TAIL_GAIN = 0.35;   // the tail against the reflections
    // the room's level: at the very back (mix 0.4) about −13 to −15 LU under a sustained sound, −16 to −18 LU under
    // drums; less further forward. Mixes with reverb at −9 LU or more were all judged too wet, ~−14 LU was preferred
    // (RESEARCH, De Man 2017): only the furthest sound comes near that.
    static constexpr double OUT_GAIN = 0.5;
    Ambience() : buf(N, 0.0) {
        for (auto& a : dif) for (auto& b : a) b.assign(D, 0.0);
        for (auto& l : line) l.assign(LN, 0.0);
    }
    static bool prime(int n) { if (n < 2) return false; for (int d = 2; d * d <= n; d++) if (n % d == 0) return false; return true; }
    void setup(double rate) {
        sr = rate;
        hp.set(Svf::HP, 180, 0.7071, 0, sr);
        for (int k = 0; k < 6; k++) {
            tapL[k] = std::min(N - 1, std::max(1, (int)(TL[k] * ER_SIZE * 0.001 * sr)));
            tapR[k] = std::min(N - 1, std::max(1, (int)(TR[k] * ER_SIZE * 0.001 * sr)));
        }
        const double dl[2][2] = {{3.1, 4.3}, {3.7, 5.1}};
        for (int c = 0; c < 2; c++) for (int k = 0; k < 2; k++) difLen[c][k] = std::min(D - 1, std::max(1, (int)(dl[c][k] * 0.001 * sr)));
        for (int i = 0; i < NL; i++) {
            int n = std::min(LN - 1, std::max(2, (int)(LMS[i] * 0.001 * sr)));
            while (!prime(n) && n < LN - 1) n++;
            for (int j = 0; j < i; j++) if (len[j] == n) { n++; while (!prime(n) && n < LN - 1) n++; }
            len[i] = n;
        }
        size = -1;
        setSize(0.7, 5000);
    }
    // size 0.4 … 1.2 (bigger = further back): the tail's decay, 0.2 s … 0.6 s, and how fast the highs die (dampHz)
    void setSize(double s, double dampHz) {
        damp[0].setLP(dampHz, sr); damp[1].setLP(dampHz, sr);
        if (std::fabs(s - size) < 1e-3) return;
        size = s;
        t60 = 0.2 + 0.5 * clampd(s - 0.4, 0, 0.8);
        const double t60hi = t60 * clampd(dampHz / 12000.0, 0.25, 0.6);
        for (int i = 0; i < NL; i++) {
            const double gl = std::pow(10.0, -3.0 * len[i] / (t60 * sr));
            const double gh = std::pow(10.0, -3.0 * len[i] / (t60hi * sr));
            const double r = gh / gl;
            gLo[i] = gl;
            pole[i] = (1 - r) / (1 + r);   // DC gain gl, Nyquist gain gh
        }
    }
    void reset() {
        std::memset(buf.data(), 0, sizeof(double) * N);
        for (auto& a : dif) for (auto& b : a) std::memset(b.data(), 0, sizeof(double) * D);
        for (auto& l : line) std::memset(l.data(), 0, sizeof(double) * LN);
        for (int i = 0; i < NL; i++) { lz[i] = 0; fbOut[i] = 0; }
        hp.reset(); damp[0].reset(); damp[1].reset();
        w = dw = lw = 0;
    }
    // the input at this sample: the reflections' line and the tail (the tail always runs, so turning the room on
    // never starts from silence)
    inline void write(double mid) {
        const double x = hp.tick(mid);
        buf[(size_t)w] = x;
        // the lines' outputs, through their loss filters
        double v[NL];
        for (int i = 0; i < NL; i++) {
            const double y = line[i][(size_t)((lw - len[i]) & (LN - 1))];
            lz[i] = gLo[i] * (1 - pole[i]) * y + pole[i] * lz[i];
            if (std::fabs(lz[i]) < 1e-25) lz[i] = 0;
            v[i] = lz[i];
            fbOut[i] = v[i];
        }
        // 16x16 Hadamard (fast Walsh-Hadamard), scaled to stay lossless: the losses live in the filters
        for (int h = 1; h < NL; h <<= 1)
            for (int i = 0; i < NL; i += h << 1)
                for (int j = i; j < i + h; j++) { const double a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
        const double sc = 1.0 / std::sqrt((double)NL);
        static constexpr double IN[NL] = {1, -1, 1, 1, -1, 1, -1, -1, -1, 1, 1, -1, 1, 1, -1, 1};
        for (int i = 0; i < NL; i++) line[i][(size_t)lw] = v[i] * sc + 0.5 * IN[i] * x;
    }
    inline double allpass(std::vector<double>& b, int dlen, double x) {
        const double y = b[(size_t)((dw - dlen) & (D - 1))];
        const double v = x + 0.5 * y;
        b[(size_t)dw] = v;
        return y - 0.5 * v;
    }
    // the room at this sample (call after write); advances the lines
    inline void read(double& wl, double& wr) {
        double l = 0, r = 0;
        for (int k = 0; k < 6; k++) {
            l += G[k] * buf[(size_t)((w - tapL[k]) & (N - 1))];
            r += G[k] * buf[(size_t)((w - tapR[k]) & (N - 1))];
        }
        static constexpr double OL[NL] = {1, 1, -1, 1, -1, -1, 1, -1, 1, -1, -1, 1, 1, 1, -1, -1},
                                OR[NL] = {1, -1, 1, 1, 1, -1, -1, -1, -1, 1, -1, 1, 1, -1, 1, -1};
        double tl = 0, tr = 0;
        for (int i = 0; i < NL; i++) { tl += OL[i] * fbOut[i]; tr += OR[i] * fbOut[i]; }
        // the diffuser smears the reflections only; the tail is already dense (diffusing it again adds a short echo)
        l = allpass(dif[0][1], difLen[0][1], allpass(dif[0][0], difLen[0][0], l));
        r = allpass(dif[1][1], difLen[1][1], allpass(dif[1][0], difLen[1][0], r));
        l = damp[0].lp(l + TAIL_GAIN * tl); r = damp[1].lp(r + TAIL_GAIN * tr);
        wl = OUT_GAIN * l; wr = OUT_GAIN * r;
    }
    inline void advance() { w = (w + 1) & (N - 1); dw = (dw + 1) & (D - 1); lw = (lw + 1) & (LN - 1); }
};

}  // namespace lg
