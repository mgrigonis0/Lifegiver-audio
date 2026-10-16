// Lifegiver: the brain (DESIGN §5–§9). What the sound needs (role + stage + knob turns → a target per quality) and
// how to get there: ONE solver for everything. It looks for tool amounts that minimise
//     distance to the target (measured on the takes) + tool costs (the variation mix's preferences) ,
// within per-sound limits and the Range. A variation is a set of tool costs; a mix of variations is the
// strength-weighted mix of costs, solved again, so it still hits the target. The loop is closed: every candidate plan
// is rendered through the real chain and measured, so the device knows what it reached, and the make-up gain is
// exact and static.
//
// Units: a knob turn is in "steps" (−10 … +10, 0 = as read). A full turn is this sound's safe maximum for that
// quality (in the quality's own dB), smaller when the sound already has a lot of it and gentler when the reading is
// unsure. Cuts get more room than boosts.
// Two speeds: the slow path (solve) runs on the worker; the fast path (one Gauss-Newton step on the saved
// sensitivity table J) answers knob turns, drags and automation at once, also after a reopen without takes.
#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "chain.h"
#include "measure.h"
#include "profile.h"

namespace lg {

// ------------------------------------------------------------------------------------------------- inputs
enum Role { ROLE_LEAD, ROLE_HOOK, ROLE_SUPPORT, ROLE_RHYTHM, ROLE_BASS, ROLE_BED, ROLE_FX, NROLES };
static const char* const ROLE_NAMES[NROLES] = {"Lead", "Hook", "Support", "Rhythm", "Bass", "Bed", "FX"};
constexpr int NVAR = 5;

struct Inputs {
    double knob[NQ];                 // −10 … +10 steps, the user's own extra on top of the stage
    int role = ROLE_LEAD;
    double pan = 0, depth = 0.82, width = 0.5, edge = 0.25;   // the stage (width 0.5 = as the sound is)
    double lo = 20, hi = 20000;      // the range the sound may own
    double fit = 0.5;                // 0 = Identity … 1 = Fit
    double clean = 0.4;              // 0 … 1
    double vars[NVAR] = {0, 0, 0, 0, 0};       // character strengths A..E (0 = none)
    Inputs() { for (double& k : knob) k = 0; }
};

// a role's starting point on the stage (the button drops the shape here) — first draft, tuned by ear
struct RolePreset { double pan, depth, width, edge, lo, hi; };
static const RolePreset ROLE_PRESETS[NROLES] = {
    {0.0, 0.82, 0.35, 0.0, 60, 20000},      // Lead: front, centred, a bit narrow
    {-0.25, 0.66, 0.45, 0.15, 80, 20000},   // Hook
    {0.3, 0.52, 0.55, 0.0, 100, 16000},     // Support: just behind the lead, out of the lows
    {-0.2, 0.5, 0.5, 0.1, 25, 20000},       // Rhythm
    {0.0, 0.7, 0.15, 0.1, 25, 16000},       // Bass: centred, narrow, owns the lows
    {0.0, 0.2, 0.8, 0.0, 120, 16000},       // Bed: back and wide, makes room below
    {0.4, 0.15, 0.95, 0.1, 150, 20000},     // FX: back, wide, out of the lows
};
// The preset as the role button applies it to this sound: roles that keep their lows never cut into the sound's
// own lowest notes (a kick's fundamental is not "rumble").
inline RolePreset rolePresetFor(int role, const Profile& p) {
    RolePreset r = ROLE_PRESETS[(role >= 0 && role < NROLES) ? role : 0];
    const bool ownsLows = role == ROLE_LEAD || role == ROLE_HOOK || role == ROLE_RHYTHM || role == ROLE_BASS;
    if (ownsLows && p.valid) r.lo = std::max(20.0, std::min(r.lo, p.lowestHz * 0.8));
    return r;
}
// Edge (intensity) starts near smooth for every role: saturation is never a default move (DESIGN §16).
// what each role asks of the qualities, in steps (the mockup's first draft)
static const double ROLE_PUSH[NROLES][NQ] = {
    // Weight Warm Bite Air Texture Punch Tail Steady
    {0, 0, 1, 1, 0, 0, 0, 0},       // Lead: clarity, presence
    {0, 0, 2, 0, 0, 1, 0, 0},       // Hook: forward, distinct
    {0, 0, -1, -1, 0, 0, 0, 0},     // Support: gives way
    {0, 0, 0, 0, 0, 2, 0, 0},       // Rhythm: attacks
    {2, 0, 0, 0, 0, 0, 0, 2},       // Bass: lows, steady
    {0, 1, 0, 0, 0, -1, 0, 2},      // Bed: steady, softer attacks
    {0, 0, 0, 1, 1, 0, 0, 0},       // FX: air, colour
};

// the stage's push per quality, in steps (shown as the arc on each knob)
inline void stagePush(const Inputs& in, double* push) {
    const double roleScale = 0.4 + 0.6 * clampd(in.fit, 0, 1);   // Identity end: the role pulls less
    const int r = (in.role >= 0 && in.role < NROLES) ? in.role : 0;
    for (int q = 0; q < NQ; q++) push[q] = ROLE_PUSH[r][q] * roleScale;
    const double f = (clampd(in.depth, 0, 1) - 0.5) * 2;   // −1 back … +1 front
    push[Q_BITE] += 1.5 * f; push[Q_AIR] += 1.5 * f; push[Q_PUNCH] += 1.5 * f; push[Q_TAIL] -= 1.5 * f;
    push[Q_STEADY] += std::fabs(f);
    const double e = clampd(in.edge, 0, 1);
    push[Q_TEXTURE] += 3 * e; push[Q_STEADY] += 1.5 * e;
    for (int q = 0; q < NQ; q++) push[q] = clampd(push[q], -4, 4);
}
inline double stageLevelDb(const Inputs& in) { return -(1 - clampd(in.depth, 0, 1)) * 3.0; }

// the safe maximum (dB of each quality's measure) for a full turn, per direction, for this sound
struct Units { double up[NQ], dn[NQ]; bool ok[NQ]; };
inline Units unitsFor(const Profile& p, const double* zero, const bool* zeroOk) {
    Units u;
    for (int q = 0; q < NQ; q++) { u.up[q] = 0; u.dn[q] = 0; u.ok[q] = zeroOk[q]; }
    const double* R = zero;
    u.up[Q_WEIGHT] = R[Q_WEIGHT] > 3 ? 3 : 6;                                u.dn[Q_WEIGHT] = 9;
    u.up[Q_WARM] = R[Q_WARM] > 3 ? 2.5 : 5;                                  u.dn[Q_WARM] = 8;
    u.up[Q_BITE] = R[Q_BITE] > -8 ? 2.5 : 5;                                 u.dn[Q_BITE] = 8;
    u.up[Q_AIR] = R[Q_AIR] > -14 ? 3 : 6;                                    u.dn[Q_AIR] = 10;
    u.up[Q_TEXTURE] = p.harmonic > 0.5 ? 4 : 2;                              u.dn[Q_TEXTURE] = 3;
    u.up[Q_PUNCH] = 6;                                                       u.dn[Q_PUNCH] = 5;
    u.up[Q_TAIL] = 6;                                                        u.dn[Q_TAIL] = 8;
    u.up[Q_STEADY] = clampd(-R[Q_STEADY] - 1.0, 0, 5);                       u.dn[Q_STEADY] = 4;
    // a weak band (little of the sound's energy there) gets a gentle lift only; an empty one can't be shaped
    for (int q = Q_WEIGHT; q <= Q_AIR; q++) {
        if (R[q] < -60) u.ok[q] = false;
        else if (R[q] < -40) u.up[q] = std::min(u.up[q], 3.0);
    }
    for (int q = 0; q < NQ; q++) {
        const double c = 0.6 + 0.4 * clampd(p.certainty[q], 0, 1);   // unsure → gentler
        u.up[q] *= c; u.dn[q] *= c;
        if (!u.ok[q]) { u.up[q] = 0; u.dn[q] = 0; }
    }
    return u;
}
// steps → dB of the measure, and back
inline double stepsToDb(const Units& u, int q, double s) { return s >= 0 ? s * u.up[q] / 10 : s * u.dn[q] / 10; }
inline double dbToSteps(const Units& u, int q, double db) {
    const double un = db >= 0 ? u.up[q] : u.dn[q];
    return un > 1e-9 ? db * 10 / un : 0;
}

// ------------------------------------------------------------------------------------------------- tools
enum Tool { T_WEIGHT, T_WARM, T_BITE, T_AIR, T_TILT, T_SAT, T_COMP, T_EXP, T_TSA, T_TSS, NT };
static const double TOOL_LO[NT] = {-12, -9, -10, -12, -6, 0, 0, 0, -1, -1};
static const double TOOL_HI[NT] = {9, 6, 8, 9, 6, 1, 1, 1, 1, 1};
static const double TOOL_H[NT] = {3, 3, 3, 3, 2, 0.3, 0.4, 0.4, 0.4, 0.4};   // typical step (cost unit, probe size)

// The characters A..E, from subtle to strong (the user's "rising character"). Each one, at its strength (0..1):
// asks for its own sound (pushes, in steps, added to the stage's), prefers its own tools (costs, lower = preferred)
// and sets its style (compressor attack, release ms, max ratio, parallel mix; saturation evenness). All at 0 = no
// character: only what the stage and the knobs ask, by the cleanest tools. Strengths add (any blend).
static const char* const VAR_NAMES[NVAR] = {"Polish", "Warm", "Bold", "Vintage", "Driven"};
static const double VAR_PUSH[NVAR][NQ] = {
    // Weight Warm Bite Air Texture Punch Tail Steady
    {0, 0, 1, 2, 0, 0, 0, 1},             // A Polish: a little clearer and more open, gentle glue
    {0.5, 2.5, 0, -0.5, 0, 0, 0, 1.5},    // B Warm: body, glue
    {0, 0, 2.5, 0, 0, 3, -1, 0},          // C Bold: forward, sharper attacks
    {0, 2, -1, -3, 0, -1, 0, 3},          // D Vintage: rounded soft top, slow glue
    {1, 0, 2, 0, 0, 1.5, 0, 4},           // E Driven: dense, held level
};
// Each character's colour, always in its sound at its strength: the least amount of its signature tools
// (saturation drive, compressor amount, transient attack). The solver works around it, so the goal still holds.
static const double VAR_FLOOR[NVAR][NT] = {
    // Weight Warm Bite Air Tilt Sat  Comp  Exp TsA  TsS
    {0, 0, 0, 0, 0, 0.0, 0.15, 0, 0.0, 0},   // A Polish: gentle glue only
    {0, 0, 0, 0, 0, 0.3, 0.25, 0, 0.0, 0},   // B Warm: warm (even) harmonics, glue
    {0, 0, 0, 0, 0, 0.3, 0.0, 0, 0.3, 0},    // C Bold: edgy (odd) harmonics, sharper attacks
    {0, 0, 0, 0, 0, 0.45, 0.35, 0, 0.0, 0},  // D Vintage: tape-like harmonics, slow glue
    {0, 0, 0, 0, 0, 0.75, 0.5, 0, 0.0, 0},   // E Driven: dense saturation, held level
};
static const double VAR_COST[NVAR][NT] = {
    // Weight Warm Bite  Air  Tilt  Sat  Comp  Exp  TsA  TsS
    {1.0, 1.0, 1.0, 1.0, 40, 40, 1.0, 3.0, 2.0, 2.0},    // A Polish: EQ and transparent dynamics, no colour
    {1.2, 1.2, 1.2, 1.2, 40, 0.5, 0.7, 3.0, 2.0, 1.2},   // B Warm
    {1.2, 1.2, 0.8, 1.2, 40, 0.8, 2.0, 2.0, 0.5, 1.5},   // C Bold
    {1.5, 1.5, 1.5, 2.5, 0.6, 0.6, 0.7, 3.0, 2.0, 1.2},  // D Vintage: the top comes down by a tilt
    {1.2, 1.2, 1.2, 1.2, 40, 0.3, 0.5, 2.0, 1.0, 1.0},   // E Driven
};
static const double VAR_STYLE[NVAR][5] = {
    {10, 150, 2.5, 1.0, 0.5},
    {25, 250, 2.5, 0.6, 0.85},
    {30, 120, 3.0, 1.0, 0.15},
    {40, 400, 2.5, 0.8, 0.6},
    {5, 100, 5.0, 1.0, 0.4},
};
// no character: the cleanest tools, a transparent compressor
static const double NEUTRAL_COST[NT] = {1.0, 1.0, 1.0, 1.0, 40, 40, 1.0, 2.0, 1.5, 1.5};
static const double NEUTRAL_STYLE[5] = {10, 150, 3.0, 1.0, 0.5};

// strengths (raw, for the pushes) and the share of each (and of "no character") in costs and styles
struct Mix { double w[NVAR]; double share[NVAR + 1]; };
inline Mix mixOf(const Inputs& in, int solo) {
    Mix m;
    double s = 0;
    for (int v = 0; v < NVAR; v++) { m.w[v] = solo >= 0 ? (v == solo ? 1.0 : 0.0) : clampd(in.vars[v], 0, 1); s += m.w[v]; }
    const double none = std::max(0.0, 1.0 - s), tot = s + none;
    for (int v = 0; v < NVAR; v++) m.share[v] = m.w[v] / tot;
    m.share[NVAR] = none / tot;
    return m;
}
inline void toolCosts(const Mix& m, const Profile& p, double* c) {
    for (int t = 0; t < NT; t++) {
        double lc = m.share[NVAR] * std::log(NEUTRAL_COST[t]);
        for (int v = 0; v < NVAR; v++) lc += m.share[v] * std::log(VAR_COST[v][t]);
        c[t] = std::exp(lc);
    }
    c[T_SAT] *= 1 + 30 * (1 - clampd(p.harmonic, 0, 1));   // safety: overtones on noisy/dense sound make intermodulation
}
// what is asked of each quality beyond the user's own turn: the stage, plus the characters
inline void askedPush(const Inputs& in, int solo, double* push) {
    stagePush(in, push);
    const Mix m = mixOf(in, solo);
    for (int q = 0; q < NQ; q++) {
        for (int v = 0; v < NVAR; v++) push[q] += m.w[v] * VAR_PUSH[v][q];
        push[q] = clampd(push[q], -10, 10);
    }
}

// ------------------------------------------------------------------------------------------------- plans
// The parts of the plan that are not solved: cuts, stage, frequencies, styles, thresholds.
inline Plan basePlan(const Inputs& in, const Profile& p, const Mix& m) {
    Plan pl;
    // Clean's floor below the sound's real lows, and the Range (a hard limit)
    const double cleanHz = clampd(p.lowestHz * 0.7, 20, 250) * clampd(in.clean, 0, 1);
    const double lc = std::max(in.lo, cleanHz);
    pl[P_LOWCUT_ON] = lc > 21 ? 1 : 0;
    pl[P_LOWCUT_HZ] = std::max(lc, 20.0);
    pl[P_HIGHCUT_ON] = in.hi < 19000 ? 1 : 0;
    pl[P_HIGHCUT_HZ] = std::min(in.hi, 20000.0);
    // tool frequencies, adapted to the sound
    pl[P_WEIGHT_HZ] = clampd(p.lowestHz * 2.0, 80, 150);
    pl[P_WARM_HZ] = 300;
    pl[P_BITE_HZ] = 3000;
    pl[P_AIR_HZ] = in.hi < 19000 ? std::min(10000.0, in.hi * 0.7) : 10000.0;
    // styles from the variation mix
    double st[5];
    for (int k = 0; k < 5; k++) {
        st[k] = m.share[NVAR] * NEUTRAL_STYLE[k];
        for (int v = 0; v < NVAR; v++) st[k] += m.share[v] * VAR_STYLE[v][k];
    }
    // safety: dynamics slower than the period of the lowest note heard (with margin), or low notes distort
    const double period = p.f0min > 0 ? 1000.0 / p.f0min : 1000.0 / 40;
    pl[P_COMP_ATT] = std::max(st[0], 0.5 * period);
    pl[P_COMP_REL] = std::max(st[1], 5 * period);
    pl[P_COMP_RATIO] = st[2];
    pl[P_COMP_MIX] = st[3];
    pl[P_SAT_EVEN] = st[4];
    pl[P_SAT_REF] = p.peakRef;
    pl[P_COMP_THR] = p.levelHi;
    pl[P_EXP_THR] = p.levelLo;
    // stage: width (0.5 = as the sound is; only as wide as it holds up in mono), true pan, level
    double g = 0.25 + 1.5 * clampd(in.width, 0, 1);
    const double side = std::pow(10.0, p.sideDb / 10);   // side vs mid power
    if (g > 1) {
        const double gmax = side > 1e-6 ? std::max(1.0, 0.9 / std::sqrt(side)) : 4.0;
        g = std::min(g, gmax);
    }
    // Clean's mono safety: a sound already wider than holds up in mono (more than ~1.5 dB lost in the mono sum) is
    // narrowed toward that limit, as far as Clean asks
    if (side > 1e-6) {
        const double gSafe = std::min(1.0, 0.65 / std::sqrt(side));
        g = std::min(g, 1 - clampd(in.clean, 0, 1) * (1 - gSafe));
    }
    pl[P_WIDTH] = std::fabs(g - 1) < 1e-6 ? 1.0 : g;
    pl[P_MONO_HZ] = 120;
    pl[P_PAN] = clampd(in.pan, -1, 1);
    pl[P_STAGE_DB] = stageLevelDb(in);
    return pl;
}

inline void applyTools(Plan& pl, const double* t) {
    pl[P_WEIGHT_DB] = t[T_WEIGHT];
    pl[P_WARM_DB] = t[T_WARM];
    pl[P_BITE_DB] = t[T_BITE];
    pl[P_AIR_DB] = t[T_AIR];
    pl[P_TILT_DB] = t[T_TILT];
    pl[P_SAT_DRIVE] = t[T_SAT];
    pl[P_COMP_AMT] = t[T_COMP];
    pl[P_EXP_AMT] = t[T_EXP];
    pl[P_TS_ATTACK] = t[T_TSA];
    pl[P_TS_SUSTAIN] = t[T_TSS];
}

// render every take through a plan and measure (the closed loop)
inline Measured renderMeasure(const TakeSet& ts, const Plan& pl, double lo, double hi, bool raw = false) {
    MeasureAcc acc;
    Chain ch;
    for (size_t k = 0; k < ts.takes.size(); k++) {
        const Audio& a = ts.takes[k];
        if (raw) { acc.add(a, ts.refs[k]); continue; }
        Plan p = pl;
        p[P_MAKEUP_DB] = 0; p[P_STAGE_DB] = 0;
        ch.reset(p, a.sr);
        Audio o;
        o.sr = a.sr;
        o.l.resize(a.size()); o.r.resize(a.size());
        std::vector<double> L(a.size()), R(a.size());
        for (size_t i = 0; i < a.size(); i++) { L[i] = a.l[i]; R[i] = a.r[i]; }
        ch.process(L.data(), R.data(), L.data(), R.data(), (long)a.size());
        for (size_t i = 0; i < a.size(); i++) { o.l[i] = (float)L[i]; o.r[i] = (float)R[i]; }
        acc.add(o, ts.refs[k]);
    }
    return acc.finish(lo, hi);
}

// ------------------------------------------------------------------------------------------------- solving
constexpr int NM = NQ + 1;   // measured outputs: the qualities and loudness (for the make-up)

struct Solution {
    bool ok = false;
    Plan plan;
    double tools[NT];
    double J[NM][NT];        // d(measure, dB)/d(tool), at the solution
    double M[NM];            // measured at the solution
    double Z[NM];            // the zero point: the take through the base plan and the characters' colour
    double Zraw[NM];         // the take through the base plan alone (cuts, no tools)
    bool zok[NQ];
    double asked[NQ];        // steps asked
    double reached[NQ];      // steps reached (measured)
    double reach = 1;        // 0..1 overall
    double inLoud = -70;     // the input's loudness
    Units units;
    Solution() {
        for (int t = 0; t < NT; t++) { tools[t] = 0; for (int m = 0; m < NM; m++) J[m][t] = 0; }
        for (int m = 0; m < NM; m++) { M[m] = 0; Z[m] = 0; Zraw[m] = 0; }
        for (int q = 0; q < NQ; q++) { zok[q] = false; asked[q] = 0; reached[q] = 0; }
    }
};

inline void toMeasures(const Measured& m, double* out) {
    for (int q = 0; q < NQ; q++) out[q] = m.q[q];
    out[NQ] = m.loudness;
}

// solve a small dense system A x = b (n ≤ NT), partial pivoting; false if singular
inline bool solveLinear(double A[NT][NT], double* b, double* x, int n) {
    for (int c = 0; c < n; c++) {
        int piv = c;
        for (int r = c + 1; r < n; r++) if (std::fabs(A[r][c]) > std::fabs(A[piv][c])) piv = r;
        if (std::fabs(A[piv][c]) < 1e-14) return false;
        if (piv != c) { for (int k = 0; k < n; k++) std::swap(A[c][k], A[piv][k]); std::swap(b[c], b[piv]); }
        for (int r = c + 1; r < n; r++) {
            const double f = A[r][c] / A[c][c];
            for (int k = c; k < n; k++) A[r][k] -= f * A[c][k];
            b[r] -= f * b[c];
        }
    }
    for (int r = n - 1; r >= 0; r--) {
        double s = b[r];
        for (int k = r + 1; k < n; k++) s -= A[r][k] * x[k];
        x[r] = s / A[r][r];
    }
    return true;
}

// Which tools may serve which quality (the solver only moves tools that serve something asked; the variation mix
// then prefers some of them). Keeps each move musical: no "more bite" by cutting everything else, no expander to
// change a tone colour.
static const bool TOOL_FOR[NQ][NT] = {
    // Weight Warm  Bite   Air    Tilt   Sat    Comp   Exp    TsA    TsS
    {true, false, false, false, false, false, false, false, false, false},   // Weight
    {false, true, false, false, false, true, false, false, false, false},    // Warm
    {false, false, true, false, false, true, false, false, false, false},    // Bite
    {false, false, false, true, true, false, false, false, false, false},    // Air
    {false, false, false, false, false, true, false, false, false, false},   // Texture
    {false, false, false, false, false, false, true, false, true, false},    // Punch
    {false, false, false, false, false, false, true, true, false, true},     // Tail
    {false, false, false, false, false, false, true, true, false, false},    // Steady
};

// the characters' colour: the least of each tool, at their strengths
inline void floorsOf(const Mix& m, const Profile& p, double* floor) {
    for (int k = 0; k < NT; k++) {
        double f = 0;
        for (int v = 0; v < NVAR; v++) f += m.w[v] * VAR_FLOOR[v][k];
        if (k == T_SAT) f *= clampd(p.harmonic, 0, 1);   // safety: little or no saturation on noisy, dense sound
        floor[k] = clampd(f, 0, TOOL_HI[k]);
    }
}

struct Problem {
    double T[NQ];            // target (dB of each measure)
    double w[NQ];            // weight per quality (normalised residuals)
    double scale[NQ];        // dB per step (normalisation)
    double cost[NT];         // tool costs
    bool on[NT];             // tools the solver may move
    double floor[NT];        // the characters' colour: the least of each tool (0 = none)
    bool unreachable[NQ] = {false, false, false, false, false, false, false, false};
    double inLoud = -70;     // the input's loudness: a big level change costs (it would only be hidden by make-up)
    double lambda = 0.03;
};
constexpr double LOUD_W = 0.15, LOUD_SCALE = 3.0;

inline Problem makeProblem(const Inputs& in, const Profile& p, const Mix& m, int solo, const double* Z, const bool* zok,
                           const Units& u, double inLoud, double* askedOut) {
    Problem pb;
    double push[NQ];
    askedPush(in, solo, push);
    toolCosts(m, p, pb.cost);
    pb.inLoud = inLoud;
    floorsOf(m, p, pb.floor);
    for (int k = 0; k < NT; k++) pb.on[k] = false;
    const double keepW = 0.15 + 0.35 * (1 - clampd(in.fit, 0, 1));   // Identity end: protect what isn't asked
    for (int q = 0; q < NQ; q++) {
        const double steps = clampd(push[q] + in.knob[q], -10, 10);
        const bool can = u.ok[q] && (steps >= 0 ? u.up[q] : u.dn[q]) > 0.05;
        askedOut[q] = zok[q] && u.ok[q] ? steps : 0;
        pb.T[q] = Z[q] + (zok[q] ? stepsToDb(u, q, steps) : 0);
        pb.scale[q] = std::max(0.25, std::max(u.up[q], u.dn[q]) / 10);
        bool asked = std::fabs(steps) >= 0.25 && can;
        bool tool = false;
        for (int k = 0; k < NT; k++) if (TOOL_FOR[q][k] && pb.cost[k] < 20) { tool = true; if (asked) pb.on[k] = true; }
        // asked, but this variation mix has no tool for it (e.g. grain in "Clean"): it is not reached, and other
        // tools must not be bent to fake it
        if (asked && !tool) asked = false, pb.unreachable[q] = true;
        pb.w[q] = !zok[q] || !u.ok[q] || pb.unreachable[q] ? 0 : (asked ? 1.0 : keepW);
    }
    // a quality nobody asked for, with none of its tools in play: its change is a side effect to show, not a cost
    for (int q = 0; q < NQ; q++) {
        if (pb.w[q] <= 0 || pb.w[q] >= 1.0) continue;
        bool served = false;
        for (int k = 0; k < NT; k++) served = served || (pb.on[k] && TOOL_FOR[q][k]);
        if (!served) pb.w[q] = 0;
    }
    return pb;
}

inline double costOf(const Problem& pb, const double* M, const double* t) {
    double c = 0;
    for (int q = 0; q < NQ; q++) { const double r = (M[q] - pb.T[q]) / pb.scale[q]; c += pb.w[q] * r * r; }
    for (int k = 0; k < NT; k++) { const double a = (t[k] - pb.floor[k]) / TOOL_H[k]; c += pb.lambda * pb.cost[k] * a * a; }
    const double l = (M[NQ] - pb.inLoud) / LOUD_SCALE;
    return c + LOUD_W * l * l;
}

// One regularised Gauss-Newton step from tools t (measured M) using J, over the enabled tools only; mu = Levenberg
// damping; each tool moves at most 2 of its typical steps per iteration (trust region).
// One regularised Gauss-Newton step from tools t (measured M) using J, over the enabled tools only; mu = Levenberg
// damping; each tool moves at most 2 of its typical steps per iteration (trust region). Only the tools that serve
// an asked quality move (TOOL_FOR, in makeProblem); their effects on every quality count. Tools that aren't enabled
// stay at the characters' floor.
inline double toolLo(const Problem& pb, int k) { return pb.floor[k] > 0 ? std::max(TOOL_LO[k], pb.floor[k]) : TOOL_LO[k]; }
inline void gnStep(const Problem& pb, const double J[NM][NT], const double* M, const double* t, double mu, double* tNew) {
    int idx[NT], n = 0;
    for (int k = 0; k < NT; k++) { tNew[k] = pb.on[k] ? clampd(t[k], toolLo(pb, k), TOOL_HI[k]) : pb.floor[k]; if (pb.on[k]) idx[n++] = k; }
    if (!n) return;
    double A[NT][NT], g[NT], d[NT];
    for (int i = 0; i < n; i++) { g[i] = 0; for (int j = 0; j < n; j++) A[i][j] = 0; }
    auto row = [&](const double* Jr, double r, double s2) {
        for (int i = 0; i < n; i++) {
            const double ji = Jr[idx[i]] * TOOL_H[idx[i]];   // in tool steps (better conditioned)
            g[i] += s2 * ji * r;
            for (int j = 0; j < n; j++) {
                A[i][j] += s2 * ji * Jr[idx[j]] * TOOL_H[idx[j]];
            }
        }
    };
    for (int q = 0; q < NQ; q++) {
        if (pb.w[q] <= 0) continue;
        row(J[q], M[q] - pb.T[q], pb.w[q] / (pb.scale[q] * pb.scale[q]));
    }
    row(J[NQ], M[NQ] - pb.inLoud, LOUD_W / (LOUD_SCALE * LOUD_SCALE));
    for (int i = 0; i < n; i++) {
        const int k = idx[i];
        const double reg = pb.lambda * pb.cost[k];
        A[i][i] += reg + mu * (A[i][i] + reg) + 1e-9;
        g[i] = -(g[i] + reg * (t[k] - pb.floor[k]) / TOOL_H[k]);   // the cost is for going beyond the character's colour
    }
    if (!solveLinear(A, g, d, n)) return;
    for (int i = 0; i < n; i++) {
        const int k = idx[i];
        const double step = clampd(d[i], -2, 2) * TOOL_H[k];
        tNew[k] = clampd(t[k] + step, toolLo(pb, k), TOOL_HI[k]);
    }
}

// reach: how much of what was asked arrived, each asked quality weighted by how much was asked of it
inline void finishSolution(Solution& s, const Problem& pb) {
    double num = 0, den = 0;
    for (int q = 0; q < NQ; q++) {
        s.reached[q] = s.zok[q] && s.units.ok[q] ? dbToSteps(s.units, q, s.M[q] - s.Z[q]) : 0;
        s.reached[q] = clampd(s.reached[q], -10, 10);
        if (std::fabs(s.asked[q]) >= 0.25 && (pb.w[q] >= 1.0 || pb.unreachable[q])) {
            const double want = pb.T[q] - s.Z[q], got = s.M[q] - s.Z[q];
            const double frac = std::fabs(want) > 1e-9 ? clampd(got / want, 0, 1) : 1;
            num += std::fabs(s.asked[q]) * frac;
            den += std::fabs(s.asked[q]);
        }
    }
    s.reach = den > 1e-12 ? num / den : 1;
    s.plan[P_MAKEUP_DB] = clampd(s.inLoud - s.M[NQ], -18, 18);
}

// The slow path. `warm` (optional): a previous solution on the same takes to start from (its J and tools).
inline Solution solve(const TakeSet& ts, const Profile& p, const Inputs& in, int solo, const Solution* warm = nullptr,
                      int maxIter = 8) {
    Solution s;
    if (ts.takes.empty() || !p.valid) return s;
    const Mix mix = mixOf(in, solo);
    const Plan base = basePlan(in, p, mix);
    s.inLoud = renderMeasure(ts, base, in.lo, in.hi, true).loudness;
    const Measured zm = renderMeasure(ts, base, in.lo, in.hi);
    toMeasures(zm, s.Zraw);
    for (int q = 0; q < NQ; q++) s.zok[q] = zm.valid[q];
    s.units = unitsFor(p, s.Zraw, s.zok);

    double t[NT], M[NM];
    auto eval = [&](const double* tt, double* out) {
        Plan pl = base;
        applyTools(pl, tt);
        toMeasures(renderMeasure(ts, pl, in.lo, in.hi), out);
    };
    // The characters' colour is the new zero: what it does to the sound is intended, so the asks (stage, knobs,
    // the characters' own pushes) are measured from there, and nothing fights it.
    double fl[NT];
    floorsOf(mix, p, fl);
    bool coloured = false;
    for (int k = 0; k < NT; k++) { t[k] = fl[k]; coloured = coloured || t[k] != 0.0; }
    if (coloured) eval(t, M); else for (int m = 0; m < NM; m++) M[m] = s.Zraw[m];
    for (int m = 0; m < NM; m++) s.Z[m] = M[m];
    const Problem pb = makeProblem(in, p, mix, solo, s.Z, s.zok, s.units, s.inLoud, s.asked);
    // Sensitivities, probed one tool at a time from the current point. A fresh solve probes EVERY tool (not just the
    // ones this ask enables), so the saved table answers any later knob or character on the fast path, also after a
    // reopen without the takes.
    auto probeTool = [&](int k) {
        double tt[NT], out[NM];
        for (int i = 0; i < NT; i++) tt[i] = t[i];
        double h = TOOL_H[k];
        if (tt[k] + h > TOOL_HI[k]) h = -h;
        tt[k] += h;
        eval(tt, out);
        for (int m = 0; m < NM; m++) s.J[m][k] = (out[m] - M[m]) / h;
    };
    auto probe = [&]() { for (int k = 0; k < NT; k++) if (pb.on[k]) probeTool(k); };
    bool any = false;
    for (int k = 0; k < NT; k++) any = any || pb.on[k];
    if (warm && warm->ok) {
        for (int m = 0; m < NM; m++) for (int k = 0; k < NT; k++) s.J[m][k] = warm->J[m][k];
        for (int k = 0; k < NT; k++) {
            bool known = false;
            for (int m = 0; m < NM; m++) known = known || s.J[m][k] != 0.0;
            if (!known) probeTool(k);
        }
    } else {
        for (int k = 0; k < NT; k++) probeTool(k);
    }
    if (any) {
        double cur = costOf(pb, M, t);
        double mu = 0.01;
        bool fresh = !(warm && warm->ok);   // J was just probed here
        for (int it = 0; it < maxIter; it++) {
            double tn[NT], Mn[NM];
            gnStep(pb, s.J, M, t, mu, tn);
            double step = 0;
            for (int k = 0; k < NT; k++) step += std::fabs(tn[k] - t[k]) / TOOL_H[k];
            if (step < 1e-3) {
                if (fresh) break;
                probe(); fresh = true; continue;
            }
            eval(tn, Mn);
            const double c = costOf(pb, Mn, tn);
            // Broyden update of J with what this step actually did
            double dt[NT], dd = 0;
            for (int k = 0; k < NT; k++) { dt[k] = (tn[k] - t[k]); dd += dt[k] * dt[k]; }
            if (dd > 1e-12) {
                for (int m = 0; m < NM; m++) {
                    double pred = 0;
                    for (int k = 0; k < NT; k++) pred += s.J[m][k] * dt[k];
                    const double err = (Mn[m] - M[m]) - pred;
                    for (int k = 0; k < NT; k++) s.J[m][k] += err * dt[k] / dd;
                }
            }
            if (c < cur) {
                for (int k = 0; k < NT; k++) t[k] = tn[k];
                for (int m = 0; m < NM; m++) M[m] = Mn[m];
                const bool big = cur - c > 0.002 * (cur + 1e-9);
                cur = c;
                mu = std::max(mu / 3, 1e-4);
                fresh = false;
                if (!big) break;
            } else if (!fresh) {
                probe(); fresh = true;   // the model was off: measure it again here
            } else {
                mu *= 4;
            }
            double worst = 0;
            for (int q = 0; q < NQ; q++) if (pb.w[q] >= 1.0) worst = std::max(worst, std::fabs(M[q] - pb.T[q]) / pb.scale[q]);
            if (worst < 0.2) break;
        }
    }
    for (int k = 0; k < NT; k++) s.tools[k] = t[k];
    for (int m = 0; m < NM; m++) s.M[m] = M[m];
    s.plan = base;
    applyTools(s.plan, t);
    finishSolution(s, pb);
    s.ok = true;
    return s;
}

// The fast path: from the last solution (its J, tools and measures), one step toward new inputs. No rendering.
// Range changes act through the base plan (cuts) at once; the measures' zero point is kept until the next solve.
// Tools the last solve didn't probe have no sensitivities yet: they are probed by the next slow solve.
inline Solution fastStep(const Solution& last, const Profile& p, const Inputs& in, int solo) {
    Solution s = last;
    if (!last.ok) return s;
    const Mix mix = mixOf(in, solo);
    const Plan base = basePlan(in, p, mix);
    // the zero point with this mix's colour, from the raw zero and the sensitivities
    double fl[NT];
    floorsOf(mix, p, fl);
    for (int m = 0; m < NM; m++) {
        s.Z[m] = last.Zraw[m];
        for (int k = 0; k < NT; k++) s.Z[m] += last.J[m][k] * fl[k];
    }
    Problem pb = makeProblem(in, p, mix, solo, s.Z, last.zok, last.units, last.inLoud, s.asked);
    for (int k = 0; k < NT; k++) {
        bool known = false;
        for (int m = 0; m < NM; m++) known = known || last.J[m][k] != 0.0;
        if (!known && last.tools[k] == 0.0) pb.on[k] = false;
    }
    double tn[NT];
    gnStep(pb, last.J, last.M, last.tools, 0.0, tn);
    // a second step from the predicted point (the trust region may have cut the first one short)
    double M[NM];
    for (int m = 0; m < NM; m++) {
        M[m] = last.M[m];
        for (int k = 0; k < NT; k++) M[m] += last.J[m][k] * (tn[k] - last.tools[k]);
    }
    double t2[NT];
    gnStep(pb, last.J, M, tn, 0.0, t2);
    for (int m = 0; m < NM; m++) {
        M[m] = last.M[m];
        for (int k = 0; k < NT; k++) M[m] += last.J[m][k] * (t2[k] - last.tools[k]);
    }
    for (int k = 0; k < NT; k++) s.tools[k] = t2[k];
    for (int m = 0; m < NM; m++) s.M[m] = M[m];
    s.plan = base;
    applyTools(s.plan, t2);
    finishSolution(s, pb);
    return s;
}

// ------------------------------------------------------------------------------------------------- moves
// What the device is doing, for the Moves list. The UI turns the kind into words; everything is numbers (no new
// symbols in Max per update).
enum MoveKind {
    MV_NONE, MV_LOWS_UP, MV_LOWS_DOWN, MV_WARM_UP, MV_WARM_DOWN, MV_BITE_UP, MV_BITE_DOWN, MV_AIR_UP, MV_AIR_DOWN,
    MV_TILT_UP, MV_TILT_DOWN, MV_SAT_EVEN, MV_SAT_ODD, MV_COMP, MV_EXP, MV_ATTACK_UP, MV_ATTACK_DOWN, MV_SUSTAIN_UP,
    MV_SUSTAIN_DOWN, MV_LOWCUT, MV_HIGHCUT, MV_WIDER, MV_NARROWER, MV_PAN, MV_MAKEUP, MV_STAGE_LEVEL
};
struct Move { int kind; double value; int q; double reached, asked; };

inline std::vector<Move> movesOf(const Solution& s) {
    std::vector<Move> mv;
    if (!s.ok) return mv;
    const Plan& pl = s.plan;
    auto add = [&](int kind, double value, int q) {
        Move m;
        m.kind = kind; m.value = value; m.q = q;
        m.reached = q >= 0 ? s.reached[q] : 0;
        m.asked = q >= 0 ? s.asked[q] : 0;
        mv.push_back(m);
    };
    const double* t = s.tools;
    if (std::fabs(t[T_WEIGHT]) >= 0.5) add(t[T_WEIGHT] > 0 ? MV_LOWS_UP : MV_LOWS_DOWN, t[T_WEIGHT], Q_WEIGHT);
    if (std::fabs(t[T_WARM]) >= 0.5) add(t[T_WARM] > 0 ? MV_WARM_UP : MV_WARM_DOWN, t[T_WARM], Q_WARM);
    if (std::fabs(t[T_BITE]) >= 0.5) add(t[T_BITE] > 0 ? MV_BITE_UP : MV_BITE_DOWN, t[T_BITE], Q_BITE);
    if (std::fabs(t[T_AIR]) >= 0.5) add(t[T_AIR] > 0 ? MV_AIR_UP : MV_AIR_DOWN, t[T_AIR], Q_AIR);
    if (std::fabs(t[T_TILT]) >= 0.5) add(t[T_TILT] > 0 ? MV_TILT_UP : MV_TILT_DOWN, t[T_TILT], Q_AIR);
    if (t[T_SAT] >= 0.05) add(pl[P_SAT_EVEN] >= 0.5 ? MV_SAT_EVEN : MV_SAT_ODD, t[T_SAT], Q_TEXTURE);
    if (t[T_COMP] >= 0.05) add(MV_COMP, pl[P_COMP_REL], Q_STEADY);
    if (t[T_EXP] >= 0.05) add(MV_EXP, t[T_EXP], Q_STEADY);
    if (std::fabs(t[T_TSA]) >= 0.05) add(t[T_TSA] > 0 ? MV_ATTACK_UP : MV_ATTACK_DOWN, t[T_TSA], Q_PUNCH);
    if (std::fabs(t[T_TSS]) >= 0.05) add(t[T_TSS] > 0 ? MV_SUSTAIN_UP : MV_SUSTAIN_DOWN, t[T_TSS], Q_TAIL);
    if (pl[P_LOWCUT_ON] > 0) add(MV_LOWCUT, pl[P_LOWCUT_HZ], -1);
    if (pl[P_HIGHCUT_ON] > 0) add(MV_HIGHCUT, pl[P_HIGHCUT_HZ], -1);
    if (pl[P_WIDTH] > 1.02) add(MV_WIDER, pl[P_WIDTH], -1);
    if (pl[P_WIDTH] < 0.98) add(MV_NARROWER, pl[P_WIDTH], -1);
    if (std::fabs(pl[P_PAN]) >= 0.02) add(MV_PAN, pl[P_PAN], -1);
    add(MV_MAKEUP, pl[P_MAKEUP_DB], -1);
    if (pl[P_STAGE_DB] <= -0.1) add(MV_STAGE_LEVEL, pl[P_STAGE_DB], -1);
    return mv;
}

}  // namespace lg
