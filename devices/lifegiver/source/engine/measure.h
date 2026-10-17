// Lifegiver: the measures (offline: worker thread and tests). Each of the 8 qualities is a measured quantity in dB,
// relative to the sound itself (DESIGN §4): the knobs move these numbers, the solver checks them on the takes.
//
//   Weight   power in [range lo, 150 Hz] vs the rest of the range                       dB
//   Warm     power in [150, 600 Hz] vs the rest                                         dB
//   Bite     power in [1.8, 5 kHz] vs the rest (presence)                               dB
//   Air      power in [8 kHz, range hi] vs the rest                                     dB
//   Texture  spectral flatness 500 Hz – 8 kHz relative to the 1/3-octave envelope (grain: what fills the gaps
//            between partials; tone colour doesn't move it), mean over frames                                dB
//   Punch    transient over body in 64 Hz – 1 kHz: peak in the first 30 ms after each onset vs 60–120 ms  dB
//   Tail     level 150–400 ms after each onset vs its peak                              dB (negative: decays)
//   Steady   minus the spread of the 50 ms level (P90 − P10, as a standard deviation)  dB
// Plus integrated loudness (ITU-R BS.1770, K-weighted, gated), used for the static make-up gain.
//
// Comparability: which frames count, where the onsets are and which windows are active are decided once, on the
// input take (`Ref`), and reused for every processed render of it. A change in a number is then a change made by
// the processing, never by a different selection.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "dsp.h"
#include "fft.h"

namespace lg {

enum Quality { Q_WEIGHT, Q_WARM, Q_BITE, Q_AIR, Q_TEXTURE, Q_PUNCH, Q_TAIL, Q_STEADY, NQ };
static const char* const QUALITY_NAMES[NQ] = {"Weight", "Warm", "Bite", "Air", "Texture", "Punch", "Tail", "Steady"};

// A captured take: stereo float (4 takes stay in memory; float halves it), at its own sample rate.
struct Audio {
    std::vector<float> l, r;
    double sr = 48000;
    size_t size() const { return l.size(); }
};

// ------------------------------------------------------------------------------------------------- K-weighting
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double tick(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// BS.1770 K-weighting at any sample rate (the reference's analog-matched design, as pyloudnorm)
inline void kWeighting(double sr, Biquad& shelf, Biquad& hp) {
    {
        const double G = 3.999843853973347, Q = 0.7071752369554196, fc = 1681.974450955533;
        const double K = std::tan(kPi * fc / sr), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1 + K / Q + K * K;
        shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelf.b1 = 2 * (K * K - Vh) / a0;
        shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf.a1 = 2 * (K * K - 1) / a0;
        shelf.a2 = (1 - K / Q + K * K) / a0;
    }
    {
        const double Q = 0.5003270373238773, fc = 38.13547087602444;
        const double K = std::tan(kPi * fc / sr);
        const double a0 = 1 + K / Q + K * K;
        hp.b0 = 1; hp.b1 = -2; hp.b2 = 1;
        hp.a1 = 2 * (K * K - 1) / a0;
        hp.a2 = (1 - K / Q + K * K) / a0;
    }
}

// mean-square blocks of K-weighted stereo (400 ms, 100 ms step), summed over channels: the BS.1770 block powers
inline void loudnessBlocks(const Audio& a, std::vector<double>& blocks) {
    Biquad s1, h1, s2, h2;
    kWeighting(a.sr, s1, h1);
    kWeighting(a.sr, s2, h2);
    const size_t n = a.size(), step = (size_t)(0.1 * a.sr), len = 4 * step;
    std::vector<double> sq(n);
    for (size_t i = 0; i < n; i++) {
        const double yl = h1.tick(s1.tick(a.l[i])), yr = h2.tick(s2.tick(a.r[i]));
        sq[i] = yl * yl + yr * yr;
    }
    if (n < len) { if (n > 0) { double s = 0; for (double v : sq) s += v; blocks.push_back(s / (double)n); } return; }
    double acc = 0;
    for (size_t i = 0; i < len; i++) acc += sq[i];
    for (size_t start = 0;; start += step) {
        blocks.push_back(acc / (double)len);
        if (start + step + len > n) break;
        for (size_t i = start; i < start + step; i++) acc -= sq[i];
        for (size_t i = start + len; i < start + len + step; i++) acc += sq[i];
    }
}

// integrated loudness (LUFS) of pooled block powers; -70 if silent
inline double integratedLoudness(const std::vector<double>& blocks) {
    double s = 0; long c = 0;
    for (double z : blocks) if (-0.691 + powToDb(z) > -70) { s += z; c++; }
    if (!c) return -70;
    const double gate = -0.691 + powToDb(s / c) - 10;
    s = 0; c = 0;
    for (double z : blocks) { const double l = -0.691 + powToDb(z); if (l > -70 && l > gate) { s += z; c++; } }
    return c ? -0.691 + powToDb(s / c) : -70;
}

// ------------------------------------------------------------------------------------------------- reference
inline int fftSizeFor(double sr) { return sr > 120000 ? 16384 : (sr > 60000 ? 8192 : 4096); }

struct Ref {
    double sr = 48000;
    int fftN = 4096, hop = 1024;
    std::vector<uint8_t> frameActive;   // STFT frames that count
    std::vector<long> onsets;           // sample positions of note/hit starts
    int winLen = 2400;                  // 50 ms level windows (Steady)
    std::vector<uint8_t> winActive;
    double activeSec = 0;               // how much of the take was sound (certainty)
};

inline void monoOf(const Audio& a, std::vector<double>& m) {
    m.resize(a.size());
    for (size_t i = 0; i < a.size(); i++) m[i] = 0.5 * ((double)a.l[i] + (double)a.r[i]);
}

// decide frames, onsets and windows on the input take
inline Ref makeRef(const Audio& a) {
    Ref r;
    r.sr = a.sr;
    r.fftN = fftSizeFor(a.sr);
    r.hop = r.fftN / 4;
    r.winLen = (int)(0.05 * a.sr);
    std::vector<double> m;
    monoOf(a, m);
    const size_t n = m.size();
    // STFT frames
    if (n >= (size_t)r.fftN) {
        const size_t nf = (n - (size_t)r.fftN) / (size_t)r.hop + 1;
        std::vector<double> ms(nf);
        double mx = 0;
        for (size_t f = 0; f < nf; f++) {
            double s = 0;
            for (int i = 0; i < r.fftN; i++) { const double v = m[f * (size_t)r.hop + (size_t)i]; s += v * v; }
            ms[f] = s / r.fftN;
            mx = std::max(mx, ms[f]);
        }
        r.frameActive.resize(nf);
        for (size_t f = 0; f < nf; f++) r.frameActive[f] = (ms[f] > mx * 1e-4 && ms[f] > 1e-7) ? 1 : 0;   // -40 dB, -70 dBFS
    }
    // level windows
    {
        const size_t nw = n / (size_t)r.winLen;
        std::vector<double> ms(nw);
        double mx = 0;
        for (size_t w = 0; w < nw; w++) {
            double s = 0;
            for (int i = 0; i < r.winLen; i++) { const double v = m[w * (size_t)r.winLen + (size_t)i]; s += v * v; }
            ms[w] = s / r.winLen;
            mx = std::max(mx, ms[w]);
        }
        r.winActive.resize(nw);
        long act = 0;
        for (size_t w = 0; w < nw; w++) { r.winActive[w] = (ms[w] > mx * 3.16e-4 && ms[w] > 1e-7) ? 1 : 0; act += r.winActive[w]; }
        r.activeSec = act * 0.05;
    }
    // onsets: in any of 4 broad bands (40–200 Hz, 200 Hz–1 kHz, 1–5 kHz, 5–16 kHz), the band's smoothed energy rises
    // 6 dB above its minimum of the last 60 ms, near the band's loud parts. Broad bands and smoothing longer than the
    // low notes' periods keep beating partials and steady noise from counting as new notes; legato note changes
    // still show as a rise in the band where the new partials land. At least 60 ms apart.
    {
        static const double edges[5] = {40, 200, 1000, 5000, 16000};
        static const double tauMs[4] = {30, 15, 8, 5};
        const int hop = std::max(1, (int)(0.002 * a.sr));
        const int look = (int)(0.06 * a.sr) / hop;
        std::vector<long> cand;
        std::vector<std::vector<double>> lvs(4);
        double gmax = -300;
        for (int b = 0; b < 4; b++) {
            if (edges[b] > 0.45 * a.sr) break;
            Svf hp, lp;
            hp.set(Svf::HP, edges[b], 0.7071, 0, a.sr);
            lp.set(Svf::LP, std::min(edges[b + 1], 0.45 * a.sr), 0.7071, 0, a.sr);
            const double c = coefFor(tauMs[b], a.sr);
            double e = 0;
            std::vector<double>& lv = lvs[(size_t)b];
            lv.reserve(n / (size_t)hop + 1);
            for (size_t i = 0; i < n; i++) {
                const double y = lp.tick(hp.tick(m[i]));
                e += c * (y * y - e);
                if (i % (size_t)hop == 0) lv.push_back(powToDb(e + 1e-20));
            }
            for (double v : lv) gmax = std::max(gmax, v);
        }
        const int ahead = std::max(1, (int)(0.03 * a.sr) / hop);   // longer than the period of a 35 Hz note
        for (int b = 0; b < 4; b++) {
            const std::vector<double>& lv = lvs[(size_t)b];
            if (lv.empty()) continue;
            double mx = -300;
            for (double v : lv) mx = std::max(mx, v);
            if (mx < gmax - 24) continue;   // a band with little of the sound's energy: its wiggles are not notes
            long last = -1000000;
            for (size_t h = (size_t)look; h + (size_t)ahead < lv.size(); h++) {
                if (lv[h] < mx - 35 || lv[h] < -80) continue;
                double mn = 1e9, before = 0, after = 0;
                for (size_t j = h - (size_t)look; j < h; j++) { mn = std::min(mn, lv[j]); before += lv[j]; }
                for (size_t j = h; j < h + (size_t)ahead; j++) after += lv[j];
                before /= look; after /= ahead;
                const long pos = (long)(h * (size_t)hop);
                // a real rise: 6 dB over the recent minimum, and the level stays up (not a pitch-period pulse)
                if (lv[h] - mn > 6 && after - before > 4 && pos - last > (long)(0.06 * a.sr)) {
                    size_t s = h;
                    while (s > h - (size_t)look && lv[s - 1] > mn + 1) s--;
                    cand.push_back((long)(s * (size_t)hop));
                    last = pos;
                }
            }
        }
        std::sort(cand.begin(), cand.end());
        const long gap = (long)(0.06 * a.sr);
        long last = -(long)a.sr;
        for (long c : cand) if (c - last >= gap) { r.onsets.push_back(c); last = c; }
    }
    return r;
}

// ------------------------------------------------------------------------------------------------- measures
struct Measured {
    double q[NQ];
    bool valid[NQ];
    double loudness = -70;    // LUFS
    double sideDb = -100;     // side vs mid power
    double corr = 1;          // L/R correlation
    Measured() { for (int i = 0; i < NQ; i++) { q[i] = 0; valid[i] = false; } }
};

struct BandEdges { double lo[4], hi[4]; };
constexpr double WEIGHT_TOP = 150, WARM_TOP = 600, BITE_LO = 1800, BITE_HI = 5000, AIR_LO = 8000;
// Punch needs its band (64 Hz – 1 kHz) to hold real energy: within 25 dB of the whole sound (17c)
constexpr double PUNCH_MIN_SHARE = 3.1623e-3;

inline BandEdges bandEdges(double rangeLo, double rangeHi) {
    BandEdges b;
    b.lo[0] = rangeLo; b.hi[0] = WEIGHT_TOP;
    b.lo[1] = WEIGHT_TOP; b.hi[1] = WARM_TOP;
    b.lo[2] = BITE_LO; b.hi[2] = BITE_HI;
    b.lo[3] = AIR_LO; b.hi[3] = rangeHi;
    for (int i = 0; i < 4; i++) { b.lo[i] = std::max(b.lo[i], rangeLo); b.hi[i] = std::min(b.hi[i], rangeHi); }
    return b;
}
// a band narrower than 1/3 octave inside the range can't be shaped meaningfully
inline bool bandUsable(double lo, double hi) { return hi > lo * 1.26; }

// Accumulates the measures of several takes (each with its own Ref), then turns them into qualities for a range.
class MeasureAcc {
public:
    std::vector<double> spec;      // summed power spectrum (mono), on a fixed 2 Hz grid up to 24 kHz
    long frames = 0;
    double flatSum = 0; long flatN = 0;
    double punchSum = 0; long punchN = 0;
    double tailSum = 0; long tailN = 0;
    double spreadSum = 0; double spreadW = 0;
    std::vector<double> blocks;    // loudness blocks
    double sumM = 0, sumS = 0, sumLR = 0, sumLL = 0, sumRR = 0;

    static constexpr double GRID_HZ = 2.0;
    static constexpr int GRID_N = 12001;   // 0 .. 24 kHz

    MeasureAcc() : spec(GRID_N, 0.0) {}

    void add(const Audio& a, const Ref& r) {
        std::vector<double> m;
        monoOf(a, m);
        const size_t n = m.size();
        // spectrum + flatness
        if (!r.frameActive.empty()) {
            Fft fft(r.fftN);
            std::vector<double> p, pre;
            const double binHz = a.sr / r.fftN;
            const int b500 = (int)std::ceil(500 / binHz), b8k = std::min((int)(8000 / binHz), r.fftN / 2);
            for (size_t f = 0; f < r.frameActive.size(); f++) {
                if (!r.frameActive[f]) continue;
                fft.power(m.data() + f * (size_t)r.hop, p);
                for (size_t k = 1; k < p.size(); k++) {
                    const int g = (int)std::lround(k * binHz / GRID_HZ);
                    if (g < GRID_N) spec[(size_t)g] += p[k];
                }
                frames++;
                // grain: flatness of the spectrum relative to its own 1/3-octave envelope, so a tone colour (EQ, tilt)
                // doesn't change it: only what fills the gaps between partials does (noise, new harmonics)
                pre.assign(p.size() + 1, 0.0);
                for (size_t k = 0; k < p.size(); k++) pre[k + 1] = pre[k] + p[k];
                double lsum = 0, asum = 0, pk = 0;
                int cnt = 0;
                for (int k = b500; k <= b8k; k++) pk = std::max(pk, p[(size_t)k]);
                for (int k = b500; k <= b8k; k++) {
                    const int lo = std::max(1, (int)(k / 1.1225)), hi = std::min((int)p.size() - 1, (int)(k * 1.1225) + 1);
                    const double env = (pre[(size_t)hi + 1] - pre[(size_t)lo]) / (hi - lo + 1);
                    if (env < pk * 1e-7) continue;   // nothing there (70 dB down): not part of the sound's texture
                    const double v = p[(size_t)k] / env;
                    lsum += std::log(v + 1e-20); asum += v; cnt++;
                }
                if (cnt > 8 && asum > 1e-14) { flatSum += 10 * (lsum / cnt - std::log(asum / cnt)) / std::log(10.0); flatN++; }
            }
        }
        // punch: 64 Hz – 1 kHz, the transient's peak (first 30 ms) over the body right after it (60–120 ms);
        // tail: the full-band level 150–400 ms after each onset vs its peak
        if (!r.onsets.empty()) {
            Svf hp1, lp1;
            hp1.set(Svf::HP, 64, 0.707, 0, a.sr);
            lp1.set(Svf::LP, 1000, 0.707, 0, a.sr);
            Env fast, full;
            fast.set(0.5, 15, a.sr);
            full.set(1, 30, a.sr);
            std::vector<double> band(n), lev(n);
            for (size_t i = 0; i < n; i++) {
                const double b = lp1.tick(hp1.tick(m[i]));
                band[i] = linToDb(fast.tick(std::fabs(b)) + 1e-9);
                lev[i] = linToDb(full.tick(std::fabs(m[i])) + 1e-9);
            }
            const long w30 = (long)(0.03 * a.sr), b60 = (long)(0.06 * a.sr), b120 = (long)(0.12 * a.sr);
            const long w50 = (long)(0.05 * a.sr), t150 = (long)(0.15 * a.sr), t400 = (long)(0.4 * a.sr);
            for (size_t k = 0; k < r.onsets.size(); k++) {
                const long t0 = r.onsets[k];
                const long next = k + 1 < r.onsets.size() ? r.onsets[k + 1] : (long)n;
                const long end = std::min(next, (long)n);
                if (t0 + b120 <= end) {
                    double pk = -300, body = 0;
                    for (long i = t0; i < t0 + w30; i++) pk = std::max(pk, band[(size_t)i]);
                    for (long i = t0 + b60; i < t0 + b120; i++) body += band[(size_t)i];
                    body /= (double)(b120 - b60);
                    if (pk > -150) { punchSum += clampd(pk - body, 0, 30); punchN++; }
                }
                double pk = -300;
                for (long i = t0; i < std::min(t0 + w50, end); i++) pk = std::max(pk, lev[(size_t)i]);
                const long e = std::min(t0 + t400, end - (long)(0.01 * a.sr));
                if (e - (t0 + t150) >= (long)(0.05 * a.sr) && pk > -150) {
                    double s = 0; long c = 0;
                    for (long i = t0 + t150; i < e; i++) { s += lev[(size_t)i]; c++; }
                    tailSum += std::max(s / c - pk, -60.0); tailN++;
                }
            }
        }
        // steadiness: spread of the 50 ms level over active windows
        {
            std::vector<double> lv;
            for (size_t w = 0; w < r.winActive.size(); w++) {
                if (!r.winActive[w]) continue;
                double s = 0;
                for (int i = 0; i < r.winLen; i++) { const double v = m[w * (size_t)r.winLen + (size_t)i]; s += v * v; }
                lv.push_back(powToDb(s / r.winLen + 1e-20));
            }
            if (lv.size() >= 4) {
                std::sort(lv.begin(), lv.end());
                const double p10 = lv[lv.size() / 10], p90 = lv[(lv.size() * 9) / 10];
                spreadSum += (p90 - p10) / 2.56 * (double)lv.size();
                spreadW += (double)lv.size();
            }
        }
        loudnessBlocks(a, blocks);
        for (size_t i = 0; i < n; i++) {
            const double L = a.l[i], R = a.r[i], M = 0.5 * (L + R), S = 0.5 * (L - R);
            sumM += M * M; sumS += S * S; sumLR += L * R; sumLL += L * L; sumRR += R * R;
        }
    }

    double bandPower(double lo, double hi) const {
        const int a = std::max(1, (int)std::ceil(lo / GRID_HZ)), b = std::min(GRID_N - 1, (int)std::floor(hi / GRID_HZ));
        double s = 0;
        for (int g = a; g <= b; g++) s += spec[(size_t)g];
        return s;
    }

    Measured finish(double rangeLo, double rangeHi) const {
        Measured o;
        const BandEdges be = bandEdges(rangeLo, rangeHi);
        const double total = bandPower(rangeLo, rangeHi);
        for (int i = 0; i < 4; i++) {
            if (!bandUsable(be.lo[i], be.hi[i]) || total <= 1e-20) continue;
            const double band = bandPower(be.lo[i], be.hi[i]);
            const double rest = std::max(total - band, total * 1e-6);
            o.q[i] = powToDb(band / rest + 1e-12);
            o.valid[i] = true;
        }
        if (flatN) { o.q[Q_TEXTURE] = flatSum / flatN; o.valid[Q_TEXTURE] = true; }
        // 17c: punch is read in 64 Hz – 1 kHz; a sound with next to nothing there (a hi-hat) has no punch to read or
        // shape: its "transients" there are noise, and shaping them made 17b's hats click
        const double punchShare = bandPower(64, 1000) / std::max(bandPower(20, 24000), 1e-30);
        if (punchN && punchShare >= PUNCH_MIN_SHARE) { o.q[Q_PUNCH] = punchSum / punchN; o.valid[Q_PUNCH] = true; }
        if (tailN) { o.q[Q_TAIL] = tailSum / tailN; o.valid[Q_TAIL] = true; }
        if (spreadW > 0) { o.q[Q_STEADY] = -spreadSum / spreadW; o.valid[Q_STEADY] = true; }
        o.loudness = integratedLoudness(blocks);
        o.sideDb = powToDb((sumS + 1e-20) / (sumM + 1e-20));
        o.corr = (sumLL > 0 && sumRR > 0) ? sumLR / std::sqrt(sumLL * sumRR) : 1.0;
        return o;
    }
};

}  // namespace lg
