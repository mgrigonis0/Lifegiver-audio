// Lifegiver: click-free parameter smoothing for the audio thread.
#pragma once
#include <cmath>

namespace lg {

// Two one-pole stages in a row (critically damped). A step in the target becomes an S-shaped glide: it starts with
// zero slope and settles without overshoot (≈ 99 % after 6.6 × tau). Unlike a smoothstep ramp, a new target in the
// middle of a glide never puts a corner in the level, so automation and fast knob moves stay click-free too.
// Once within `snapEps` of the target it lands exactly on it (exact end values, no denormals).
struct Smoother {
    double target = 0, y1 = 0, y2 = 0, k = 1;
    static constexpr double snapEps = 1e-7;

    void setTime(double tauSec, double sr) { k = (tauSec <= 0 || sr <= 0) ? 1.0 : 1.0 - std::exp(-1.0 / (tauSec * sr)); }
    void reset(double v) { target = y1 = y2 = v; }
    void set(double v) { target = v; }
    bool settled() const { return y2 == target && y1 == target; }
    double next() {
        if (settled()) return y2;
        y1 += k * (target - y1);
        y2 += k * (y1 - y2);
        if (std::fabs(target - y1) < snapEps && std::fabs(target - y2) < snapEps) y1 = y2 = target;
        return y2;
    }
};

inline double dbToGain(double db) { return std::pow(10.0, db / 20.0); }

}  // namespace lg
