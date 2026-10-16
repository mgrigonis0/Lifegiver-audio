// Synthetic test sounds with known answers (BUILD_PLAN Phase 2): pitched notes, a pad, drums, a bass line, noise
// swells, plucks. Deterministic (fixed seeds), stereo, as lg::Audio. Used by the tests and tools/render.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "../../source/engine/measure.h"

namespace sig {
using lg::Audio;

inline uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
inline double rnd(uint32_t& s) { return (lcg(s) >> 8) / double(1 << 24) * 2 - 1; }
inline double mtof(double m) { return 440.0 * std::pow(2.0, (m - 69) / 12); }

// band-limited harmonic tone: amplitude of harmonic k = 1/k^slope (slope 1 ≈ saw), up to 0.45 sr
inline double harm(double phase, double f0, double sr, double slope, double oddOnly = 0) {
    double s = 0;
    for (int k = 1; k * f0 < 0.45 * sr && k < 200; k++) {
        if (oddOnly > 0 && k % 2 == 0) { s += (1 - oddOnly) * std::sin(k * phase) / std::pow(k, slope); continue; }
        s += std::sin(k * phase) / std::pow(k, slope);
    }
    return s;
}

inline Audio empty(double sec, double sr) { Audio a; a.sr = sr; a.l.assign((size_t)(sec * sr), 0.f); a.r = a.l; return a; }

// a synth lead: notes every 0.5 s (A3 … E5), quick attack, sustain, small release, a touch of stereo
inline Audio lead(double sec = 4, double sr = 48000, double level = 0.25) {
    Audio a = empty(sec, sr);
    const int notes[] = {57, 60, 64, 67, 69, 72, 76, 64};
    const size_t n = a.size(), noteLen = (size_t)(0.5 * sr);
    double ph = 0;
    for (size_t i = 0; i < n; i++) {
        const size_t k = i / noteLen, j = i % noteLen;
        const double f = mtof(notes[k % 8]);
        ph += 2 * lg::kPi * f / sr;
        if (ph > 2 * lg::kPi * 1000) ph = std::fmod(ph, 2 * lg::kPi);
        const double t = j / sr;
        double env = std::min(1.0, t / 0.008) * (0.8 + 0.2 * std::exp(-t / 0.08));
        if (j > noteLen - (size_t)(0.03 * sr)) env *= (noteLen - j) / (0.03 * sr);
        const double v = level * env * harm(ph, f, sr, 1.0) * 0.6;
        a.l[i] = (float)v; a.r[i] = (float)(v * 0.92);
    }
    return a;
}

// a pad: a sustained chord with slow swells, wide (detuned left/right)
inline Audio pad(double sec = 4, double sr = 48000, double level = 0.15) {
    Audio a = empty(sec, sr);
    const double fs[] = {mtof(48), mtof(55), mtof(60), mtof(64)};
    double phL[4] = {0, 0, 0, 0}, phR[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < a.size(); i++) {
        const double t = i / sr;
        const double env = std::min(1.0, t / 0.6) * (0.85 + 0.15 * std::sin(2 * lg::kPi * 0.3 * t));
        double L = 0, R = 0;
        for (int v = 0; v < 4; v++) {
            phL[v] += 2 * lg::kPi * fs[v] * 1.002 / sr; phR[v] += 2 * lg::kPi * fs[v] * 0.998 / sr;
            L += harm(phL[v], fs[v], sr, 2.0); R += harm(phR[v], fs[v], sr, 2.0);
        }
        a.l[i] = (float)(level * env * L * 0.4); a.r[i] = (float)(level * env * R * 0.4);
    }
    return a;
}

// drums: kick on 1 and 3, snare on 2 and 4 at 120 bpm, mono-ish
inline Audio drums(double sec = 4, double sr = 48000, double level = 0.6, uint32_t seed = 3) {
    Audio a = empty(sec, sr);
    const size_t beat = (size_t)(0.5 * sr);
    double ph = 0;
    for (size_t i = 0; i < a.size(); i++) {
        const size_t b = i / beat, j = i % beat;
        const double t = j / sr;
        double v;
        if (b % 2 == 0) {   // kick: a falling sine
            const double f = 50 + 120 * std::exp(-t / 0.03);
            ph += 2 * lg::kPi * f / sr;
            v = std::sin(ph) * std::exp(-t / 0.18) * std::min(1.0, t / 0.001);
        } else {            // snare: noise burst + 190 Hz body
            ph += 2 * lg::kPi * 190 / sr;
            v = (0.7 * rnd(seed) * std::exp(-t / 0.07) + 0.5 * std::sin(ph) * std::exp(-t / 0.05)) * std::min(1.0, t / 0.001);
        }
        a.l[i] = (float)(level * v); a.r[i] = (float)(level * v * 0.98);
    }
    return a;
}

// a bass line: low notes (E1 … A1), fat, short gaps
inline Audio bass(double sec = 4, double sr = 48000, double level = 0.35) {
    Audio a = empty(sec, sr);
    const int notes[] = {28, 28, 31, 33, 28, 35, 33, 31};
    const size_t noteLen = (size_t)(0.5 * sr);
    double ph = 0;
    for (size_t i = 0; i < a.size(); i++) {
        const size_t k = i / noteLen, j = i % noteLen;
        const double f = mtof(notes[k % 8]);
        ph += 2 * lg::kPi * f / sr;
        if (ph > 2 * lg::kPi * 1000) ph = std::fmod(ph, 2 * lg::kPi);
        const double t = j / sr;
        double env = std::min(1.0, t / 0.005) * (0.6 + 0.4 * std::exp(-t / 0.15));
        if (j > noteLen - (size_t)(0.04 * sr)) env *= (noteLen - j) / (0.04 * sr);
        const double v = level * env * harm(ph, f, sr, 1.6) * 0.7;
        a.l[i] = (float)v; a.r[i] = (float)v;
    }
    return a;
}

// noise swells: band-passed noise rising and falling (noisy, no pitch)
inline Audio noise(double sec = 4, double sr = 48000, double level = 0.3, uint32_t seed = 9) {
    Audio a = empty(sec, sr);
    lg::Svf bl, br;
    bl.set(lg::Svf::BP, 2500, 0.7, 0, sr); br.set(lg::Svf::BP, 2500, 0.7, 0, sr);
    for (size_t i = 0; i < a.size(); i++) {
        const double t = i / sr;
        const double env = 0.55 + 0.45 * std::sin(2 * lg::kPi * 0.5 * t);
        a.l[i] = (float)(level * env * bl.tick(rnd(seed)));
        a.r[i] = (float)(level * env * br.tick(rnd(seed)));
    }
    return a;
}

// plucks: decaying harmonic notes every 0.375 s
inline Audio pluck(double sec = 4, double sr = 48000, double level = 0.4) {
    Audio a = empty(sec, sr);
    const int notes[] = {64, 67, 71, 72, 71, 67, 64, 62};
    const size_t noteLen = (size_t)(0.375 * sr);
    double ph = 0;
    for (size_t i = 0; i < a.size(); i++) {
        const size_t k = i / noteLen, j = i % noteLen;
        const double f = mtof(notes[k % 8]);
        ph += 2 * lg::kPi * f / sr;
        if (ph > 2 * lg::kPi * 1000) ph = std::fmod(ph, 2 * lg::kPi);
        const double t = j / sr;
        const double env = std::min(1.0, t / 0.002) * std::exp(-t / 0.12);
        const double v = level * env * harm(ph, f, sr, 1.3) * 0.6;
        a.l[i] = (float)v; a.r[i] = (float)(v * 0.9);
    }
    return a;
}

inline Audio byName(const std::string& n, double sec = 4, double sr = 48000) {
    if (n == "pad") return pad(sec, sr);
    if (n == "drums") return drums(sec, sr);
    if (n == "bass") return bass(sec, sr);
    if (n == "noise") return noise(sec, sr);
    if (n == "pluck") return pluck(sec, sr);
    return lead(sec, sr);
}
static const char* const NAMES[] = {"lead", "pad", "drums", "bass", "noise", "pluck"};

}  // namespace sig
