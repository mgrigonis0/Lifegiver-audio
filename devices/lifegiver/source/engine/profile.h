// Lifegiver: the reading (DESIGN §3, §7.1). What the sound is, from up to 4 takes: the 8 qualities as heard, the
// facts (pitch range, harmonic or noisy, plucked or sustained, stereo), levels for gain staging, and how sure each
// value is. Worker thread only. Saved in the set (not the takes: too big), so a reopened set keeps its zero points.
#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "measure.h"

namespace lg {

struct Profile {
    bool valid = false;
    int takes = 0;
    double activeSec = 0;
    int onsets = 0;
    double reading[NQ];        // the qualities as read, full open range (20 Hz – 20 kHz)
    bool readValid[NQ];
    double certainty[NQ];      // 0..1
    double f0min = 0, f0max = 0;   // Hz; 0 = no stable pitch
    double harmonic = 0;       // 0 noisy .. 1 clearly pitched
    double sustained = 0.5;    // 0 plucked/hit .. 1 sustained
    double corr = 1;           // L/R correlation
    double sideDb = -100;
    double loudness = -70;     // LUFS of the takes
    double lowestHz = 20;      // below this there is (almost) nothing
    double peakRef = 0.3;      // typical peak level (linear): gain staging into saturation
    double levelHi = -20, levelLo = -40;   // detector levels (dBFS): compressor / expander thresholds
    std::vector<double> oct6;  // power in 1/6-octave bands from 20 Hz (for re-reading a range without the takes)

    Profile() { for (int i = 0; i < NQ; i++) { reading[i] = 0; readValid[i] = false; certainty[i] = 0; } }

    static constexpr int OCT6_N = 60;
    static double oct6Lo(int i) { return 20.0 * std::pow(2.0, i / 6.0); }

    // the band-ratio qualities for a range, from the 1/6-octave summary (approximate; used when the takes are gone)
    void readRange(double lo, double hi, double* q, bool* ok) const {
        const BandEdges be = bandEdges(lo, hi);
        auto pw = [&](double a, double b) {
            double s = 0;
            for (int i = 0; i < OCT6_N && i < (int)oct6.size(); i++) {
                const double l = oct6Lo(i), h = oct6Lo(i + 1);
                const double ov = std::min(h, b) - std::max(l, a);
                if (ov > 0) s += oct6[(size_t)i] * ov / (h - l);
            }
            return s;
        };
        const double total = pw(lo, hi);
        for (int i = 0; i < 4; i++) {
            ok[i] = bandUsable(be.lo[i], be.hi[i]) && total > 1e-20;
            if (!ok[i]) { q[i] = 0; continue; }
            const double band = pw(be.lo[i], be.hi[i]);
            q[i] = powToDb(band / std::max(total - band, total * 1e-6) + 1e-12);
        }
        for (int i = 4; i < NQ; i++) { q[i] = reading[i]; ok[i] = readValid[i]; }
    }
};

// ------------------------------------------------------------------------------------------------- pitch (YIN)
struct PitchTrack { std::vector<double> f0; std::vector<double> aperiodic; std::vector<uint8_t> active; };

inline PitchTrack yinTrack(const Audio& a) {
    PitchTrack t;
    std::vector<double> m;
    monoOf(a, m);
    const int D = std::max(1, (int)std::lround(a.sr / 12000.0));
    const double sd = a.sr / D;
    Svf aa1, aa2;
    aa1.set(Svf::LP, 0.4 * sd, 0.5412, 0, a.sr);
    aa2.set(Svf::LP, 0.4 * sd, 1.3066, 0, a.sr);
    std::vector<double> x;
    x.reserve(m.size() / (size_t)D + 1);
    for (size_t i = 0; i < m.size(); i++) { const double v = aa2.tick(aa1.tick(m[i])); if (i % (size_t)D == 0) x.push_back(v); }
    const int W = 512, hop = 256, tauMin = std::max(2, (int)(sd / 1500)), tauMax = (int)(sd / 35);
    if ((int)x.size() < W + tauMax + 2) return t;
    const size_t nf = (x.size() - (size_t)(W + tauMax + 2)) / (size_t)hop + 1;
    double mx = 0;
    std::vector<double> en(nf);
    for (size_t f = 0; f < nf; f++) {
        double s = 0;
        for (int j = 0; j < W; j++) { const double v = x[f * hop + (size_t)j]; s += v * v; }
        en[f] = s / W;
        mx = std::max(mx, en[f]);
    }
    std::vector<double> d((size_t)tauMax + 2);
    for (size_t f = 0; f < nf; f++) {
        const double* p = x.data() + f * hop;
        const bool act = en[f] > mx * 1e-4 && en[f] > 1e-8;
        t.active.push_back(act ? 1 : 0);
        if (!act) { t.f0.push_back(0); t.aperiodic.push_back(1); continue; }
        d[0] = 1;
        double run = 0;
        int best = -1;
        double bestv = 1e9;
        for (int tau = 1; tau <= tauMax + 1; tau++) {
            double s = 0;
            for (int j = 0; j < W; j++) { const double df = p[j] - p[j + tau]; s += df * df; }
            run += s;
            d[(size_t)tau] = run > 0 ? s * tau / run : 1;
        }
        for (int tau = tauMin; tau <= tauMax; tau++) {
            if (d[(size_t)tau] < 0.15) {
                while (tau + 1 <= tauMax && d[(size_t)tau + 1] < d[(size_t)tau]) tau++;
                best = tau; bestv = d[(size_t)tau];
                break;
            }
        }
        if (best < 0) {   // no dip under the threshold: take the global minimum, reported as aperiodic
            for (int tau = tauMin; tau <= tauMax; tau++) if (d[(size_t)tau] < bestv) { bestv = d[(size_t)tau]; best = tau; }
        }
        double tau = best;
        if (best > tauMin && best < tauMax) {
            const double y0 = d[(size_t)best - 1], y1 = d[(size_t)best], y2 = d[(size_t)best + 1];
            const double den = y0 - 2 * y1 + y2;
            if (std::fabs(den) > 1e-12) tau = best + 0.5 * (y0 - y2) / den;
        }
        t.f0.push_back(bestv < 0.25 ? sd / tau : 0);
        t.aperiodic.push_back(bestv);
    }
    return t;
}

inline double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const size_t i = (size_t)std::min<double>((double)v.size() - 1, std::max(0.0, p * (double)(v.size() - 1)));
    return v[i];
}

// ------------------------------------------------------------------------------------------------- reading
struct TakeSet {
    std::vector<Audio> takes;
    std::vector<Ref> refs;
};

inline std::shared_ptr<TakeSet> makeTakeSet(const std::vector<Audio>& takes) {
    auto ts = std::make_shared<TakeSet>();
    ts->takes = takes;
    for (auto& a : ts->takes) ts->refs.push_back(makeRef(a));
    return ts;
}

// The takes the solver renders: at most ~6 s in all (the closed loop renders them a few dozen times), each take's
// share from its start.
inline std::shared_ptr<TakeSet> solveSetOf(const std::vector<Audio>& takes, double totalSec = 6.0) {
    std::vector<Audio> cut;
    for (const Audio& a : takes) {
        const size_t n = std::min(a.size(), (size_t)(totalSec / std::max<size_t>(1, takes.size()) * a.sr));
        Audio c;
        c.sr = a.sr;
        c.l.assign(a.l.begin(), a.l.begin() + (long)n);
        c.r.assign(a.r.begin(), a.r.begin() + (long)n);
        cut.push_back(c);
    }
    return makeTakeSet(cut);
}

inline Profile analyze(const TakeSet& ts) {
    Profile p;
    if (ts.takes.empty()) return p;
    MeasureAcc acc;
    std::vector<double> f0s, voicedFrac;
    std::vector<double> peaks, hiLevels;
    long activeFrames = 0, voicedFrames = 0;
    for (size_t k = 0; k < ts.takes.size(); k++) {
        const Audio& a = ts.takes[k];
        const Ref& r = ts.refs[k];
        acc.add(a, r);
        p.activeSec += r.activeSec;
        p.onsets += (int)r.onsets.size();
        PitchTrack pt = yinTrack(a);
        for (size_t f = 0; f < pt.f0.size(); f++) {
            if (!pt.active[f]) continue;
            activeFrames++;
            if (pt.f0[f] > 0) { voicedFrames++; f0s.push_back(pt.f0[f]); }
        }
        // peak levels per 50 ms window (gain staging, compressor thresholds): detector as the chain's (HP 60 Hz)
        OnePole hl, hr;
        hl.setLP(60, a.sr); hr.setLP(60, a.sr);
        const int wl = (int)(0.05 * a.sr);
        double pk = 0, dpk = 0;
        for (size_t i = 0; i < a.size(); i++) {
            pk = std::max(pk, (double)std::max(std::fabs(a.l[i]), std::fabs(a.r[i])));
            dpk = std::max(dpk, std::max(std::fabs(hl.hp(a.l[i])), std::fabs(hr.hp(a.r[i]))));
            if ((long)(i + 1) % wl == 0) {
                const size_t w = (i + 1) / (size_t)wl - 1;
                if (w < r.winActive.size() && r.winActive[w]) { peaks.push_back(pk); hiLevels.push_back(linToDb(dpk + 1e-9)); }
                pk = 0; dpk = 0;
            }
        }
    }
    const Measured m = acc.finish(20, 20000);
    for (int i = 0; i < NQ; i++) { p.reading[i] = m.q[i]; p.readValid[i] = m.valid[i]; }
    p.loudness = m.loudness;
    p.corr = m.corr;
    p.sideDb = m.sideDb;
    p.takes = (int)ts.takes.size();
    // pitch: the 5th–95th percentile of the voiced frames, if enough of the sound is pitched
    p.harmonic = activeFrames ? (double)voicedFrames / (double)activeFrames : 0;
    if (f0s.size() >= 8 && p.harmonic > 0.3) { p.f0min = percentile(f0s, 0.05); p.f0max = percentile(f0s, 0.95); }
    // plucked ↔ sustained from the Tail reading (decay 150–400 ms after onsets); no onsets = sustained
    if (m.valid[Q_TAIL]) p.sustained = clampd((m.q[Q_TAIL] + 24) / 20, 0, 1);
    else p.sustained = 1;
    // lowest real content: 0.3 % of the power lies below it
    {
        const double total = acc.bandPower(20, 20000);
        double cum = 0;
        p.lowestHz = 20;
        for (int g = (int)(20 / MeasureAcc::GRID_HZ); g < MeasureAcc::GRID_N; g++) {
            cum += acc.spec[(size_t)g];
            if (cum > 0.003 * total) { p.lowestHz = g * MeasureAcc::GRID_HZ; break; }
        }
        if (p.f0min > 0) p.lowestHz = std::min(p.lowestHz, p.f0min * 0.9);
    }
    p.peakRef = std::max(1e-3, percentile(peaks, 0.9));
    p.levelHi = percentile(hiLevels, 0.6) - 3;
    p.levelLo = percentile(hiLevels, 0.25) - 6;
    p.oct6.assign(Profile::OCT6_N, 0.0);
    for (int i = 0; i < Profile::OCT6_N; i++) p.oct6[(size_t)i] = acc.bandPower(Profile::oct6Lo(i), Profile::oct6Lo(i + 1));
    // how sure: spectral values from the seconds of sound, onset values from the number of onsets, more takes help
    const double specC = clampd(p.activeSec / 3.0, 0.2, 1.0) * (p.takes >= 2 ? 1.0 : 0.9);
    const double onC = p.onsets >= 8 ? 1.0 : (p.onsets >= 3 ? 0.66 : (p.onsets >= 1 ? 0.33 : 0.0));
    for (int i = 0; i < NQ; i++) p.certainty[i] = specC;
    p.certainty[Q_PUNCH] = m.valid[Q_PUNCH] ? onC : 0;
    p.certainty[Q_TAIL] = m.valid[Q_TAIL] ? onC : 0;
    p.valid = true;
    return p;
}

// ------------------------------------------------------------------------------------------------- words
// The reading in words (the knob's sub-label). Thresholds in each quality's own dB; first drafts, tuned on the stems.
inline int readingWord(int q, double v, bool ok) {
    if (!ok) return 0;   // 0 = "—"
    static const double cut[NQ][3] = {
        {-25, -12, 0},     // Weight: thin | light | full | heavy
        {-15, -6, 2},      // Warm:   lean | even | warm | thick
        {-24, -15, -7},    // Bite:   soft | smooth | clear | edgy
        {-36, -24, -14},   // Air:    dull | soft top | open | airy
        {-30, -18, -9},    // Texture: smooth | fine | grainy | rough
        {3, 7, 12},        // Punch:  soft | firm | punchy | hard
        {-22, -12, -5},    // Tail:   short | medium | long | held
        {-6, -3.5, -2},    // Steady: jumpy | lively | even | steady
    };
    int w = 1;
    for (int k = 0; k < 3; k++) if (v > cut[q][k]) w = k + 2;
    return w;   // 1..4
}

}  // namespace lg
