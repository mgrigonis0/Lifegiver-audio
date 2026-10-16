// Lifegiver: DSP building blocks shared by the real-time chain and the offline renders (same code in both, so what
// the solver measures on the takes is what the device plays).
//
// - Svf: Simper/Cytomic trapezoidal state-variable filter. Minimum phase, zero latency, stays smooth when its
//   frequency or gain moves (coefficients may change every few samples). At 0 dB the bell and shelves are exactly
//   the input (their mix coefficients are exactly 0 and 1).
// - OnePole: first-order low/high pass, also the exact complementary split used for "lows stay mono".
// - Env: peak follower with separate attack/release (one-pole in the linear domain).
// - Sat: soft saturation (tanh, optional bias for even harmonics), antialiased with first-order ADAA. Only the
//   nonlinear residual is added back to the untouched input, so at mix 0 the output is the input exactly, the linear
//   part is never delayed or filtered, and no latency is added (DESIGN §11.6; the residual's half-sample offset only
//   touches the added harmonics).
#pragma once
#include <cmath>

namespace lg {

constexpr double PI = 3.14159265358979323846;

inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
inline double linToDb(double g) { return 20.0 * std::log10(g > 1e-30 ? g : 1e-30); }
inline double powToDb(double p) { return 10.0 * std::log10(p > 1e-30 ? p : 1e-30); }

// ------------------------------------------------------------------------------------------------- SVF
struct Svf {
    enum Mode { LP, HP, BP, BELL, LOWSHELF, HIGHSHELF };
    double a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0, k = 1;
    double ic1 = 0, ic2 = 0;

    void reset() { ic1 = ic2 = 0; }
    // f in Hz (kept below 0.49 sr), q > 0, gainDb for bell/shelves
    void set(Mode mode, double f, double q, double gainDb, double sr) {
        f = clampd(f, 5.0, 0.49 * sr);
        q = q > 0.05 ? q : 0.05;
        double g = std::tan(PI * f / sr);
        const double A = std::pow(10.0, gainDb / 40.0);
        k = 1.0 / q;
        switch (mode) {
            case LP: m0 = 0; m1 = 0; m2 = 1; break;
            case HP: m0 = 1; m1 = -k; m2 = -1; break;
            case BP: m0 = 0; m1 = 1; m2 = 0; break;
            case BELL: k = 1.0 / (q * A); m0 = 1; m1 = k * (A * A - 1); m2 = 0; break;
            case LOWSHELF: g /= std::sqrt(A); m0 = 1; m1 = k * (A - 1); m2 = A * A - 1; break;
            case HIGHSHELF: g *= std::sqrt(A); m0 = A * A; m1 = k * (1 - A) * A; m2 = 1 - A * A; break;
        }
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline double tick(double v0) {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        if (std::fabs(ic1) < 1e-25) ic1 = 0;   // no denormals in long silences
        if (std::fabs(ic2) < 1e-25) ic2 = 0;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }
    // Magnitude response at f. The trapezoidal SVF is the bilinear transform of the analog prototype
    // (cHP s^2 + cBP s + cLP) / (s^2 + k s + 1), s normalised by g. With out = m0 v0 + m1 v1 + m2 v2 and
    // v0 = HP + k BP + LP: cHP = m0, cBP = m0 k + m1, cLP = m0 + m2.
    double mag(double f, double sr) const {
        const double g = a2 / a1;
        const double w = std::tan(PI * clampd(f, 0.0, 0.4999 * sr) / sr) / (g > 0 ? g : 1e-12);
        const double cHP = m0, cBP = m0 * k + m1, cLP = m0 + m2;
        const double rn = cLP - cHP * w * w, in = cBP * w;
        const double rd = 1 - w * w, id = k * w;
        return std::sqrt((rn * rn + in * in) / (rd * rd + id * id));
    }
};

// ------------------------------------------------------------------------------------------------- one-pole
struct OnePole {
    double b = 0, z = 0;
    void reset() { z = 0; }
    void setLP(double f, double sr) { b = 1.0 - std::exp(-2 * PI * clampd(f, 0.1, 0.45 * sr) / sr); }
    inline double lp(double x) { z += b * (x - z); if (std::fabs(z) < 1e-25) z = 0; return z; }
    inline double hp(double x) { return x - lp(x); }
};

// ------------------------------------------------------------------------------------------------- envelopes
inline double coefFor(double ms, double sr) { return ms <= 0 ? 1.0 : 1.0 - std::exp(-1.0 / (ms * 0.001 * sr)); }

struct Env {
    double att = 1, rel = 1, y = 0;
    void set(double attMs, double relMs, double sr) { att = coefFor(attMs, sr); rel = coefFor(relMs, sr); }
    void reset(double v = 0) { y = v; }
    inline double tick(double x) {   // x >= 0
        y += (x > y ? att : rel) * (x - y);
        if (y < 1e-30) y = 0;
        return y;
    }
};

// ------------------------------------------------------------------------------------------------- saturation
// log(cosh(u)) without overflow
inline double logcosh(double u) {
    const double a = std::fabs(u);
    return a + std::log1p(std::exp(-2 * a)) - 0.69314718055994530942;
}

// One channel of the ADAA saturator. y = (tanh(G x + b) - tanh b) / (G sech^2 b): unity gain for small signals, so
// the residual y - x is only the distortion. Evaluated with first-order ADAA on u = G x + b.
struct Sat {
    double uPrev = 0, xPrev = 0;
    OnePole dc;          // the residual's DC (from the bias) is removed
    void reset(double sr) { uPrev = 0; xPrev = 0; dc.reset(); dc.setLP(8.0, sr); }
    // returns the residual (nonlinear part only), in the units of x
    inline double residual(double x, double G, double b) {
        const double u = G * x + b;
        const double du = u - uPrev;
        double f;
        if (std::fabs(du) > 1e-6) f = (logcosh(u) - logcosh(uPrev)) / du;
        else f = std::tanh(0.5 * (u + uPrev));
        const double tb = std::tanh(b), s2 = 1 - tb * tb;
        const double y = (f - tb) / (G * s2);
        const double lin = 0.5 * (x + xPrev);   // ADAA of the linear part: the same half-sample average
        uPrev = u; xPrev = x;
        const double r = y - lin;
        return r - dc.lp(r);
    }
};

}  // namespace lg
