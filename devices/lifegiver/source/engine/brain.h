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
    double room = 0.5;               // 17c: the room's size (the stage's back wall): 0 small … 0.5 as before … 1 large
    double lo = 20, hi = 20000;      // the range the sound may own
    double fit = 0.5;                // 0 = Identity … 1 = Fit
    double clean = 0.4;              // 0 … 1
    double vars[NVAR] = {0, 0, 0, 0, 0};       // character strengths A..E (0 = none)
    Inputs() { for (double& k : knob) k = 0; }
};

// a role's starting point on the stage (the button drops the shape here) — first draft, tuned by ear.
// The pan and the width are never part of it: where a sound sits left or right, and how wide it is, are the user's
// call (16c pan; 17c width: the user always wants a sound "as it is" unless they drag its handles).
struct RolePreset { double depth, edge, lo, hi; };
static const RolePreset ROLE_PRESETS[NROLES] = {
    {0.82, 0.0, 60, 20000},     // Lead: front
    {0.66, 0.15, 80, 20000},    // Hook
    {0.52, 0.0, 100, 16000},    // Support: just behind the lead, out of the lows
    {0.5, 0.1, 25, 20000},      // Rhythm
    {0.7, 0.1, 25, 16000},      // Bass: owns the lows
    {0.2, 0.0, 120, 16000},     // Bed: back, makes room below
    {0.15, 0.1, 150, 20000},    // FX: back, out of the lows
};
// The preset as the role button applies it to this sound: roles that keep their lows never cut into the sound's
// own lowest notes (a kick's fundamental is not "rumble").
inline RolePreset rolePresetFor(int role, const Profile& p) {
    RolePreset r = ROLE_PRESETS[(role >= 0 && role < NROLES) ? role : 0];
    const bool ownsLows = role == ROLE_LEAD || role == ROLE_HOOK || role == ROLE_RHYTHM || role == ROLE_BASS;
    if (ownsLows && p.valid) r.lo = std::max(20.0, std::min(r.lo, p.lowestHz * 0.8));
    return r;
}
// 17b: roles only place the sound (the preset above: distance, width, range); they no longer ask for presence, punch
// or weight by themselves (the user: taming through the reading, shaping only when asked). The stage's Edge handle is
// gone (it duplicated the Texture and Steady knobs); its Live parameter stays, unused, so old sets open.

// the stage's push per quality, in steps (shown as the arc on each knob): only the cues that come with distance
inline void stagePush(const Inputs& in, double* push) {
    for (int q = 0; q < NQ; q++) push[q] = 0;
    const double f = (clampd(in.depth, 0, 1) - 0.5) * 2;   // −1 back … +1 front
    // further back: less presence, air and attack, smaller level jumps; nearer: shorter tails (further back, the
    // ambience brings the tail). 16c also steadied the front (|f|): unintended, removed.
    push[Q_BITE] += 1.5 * f; push[Q_AIR] += 1.5 * f; push[Q_PUNCH] += 1.5 * f; push[Q_TAIL] -= 1.5 * std::max(f, 0.0);
    push[Q_STEADY] += std::max(-f, 0.0);
    for (int q = 0; q < NQ; q++) push[q] = clampd(push[q], -4, 4);
}
// Identity strength (the Live parameter "Identity Fit", fit 0 = full Identity): how much the qualities nobody asked
// to change are held where the sound has them
inline double identityStrength(const Inputs& in) { return 1 - clampd(in.fit, 0, 1); }
inline double stageLevelDb(const Inputs& in) { return -(1 - clampd(in.depth, 0, 1)) * 3.0; }
// the ambience: distance decides how much of the room is heard (none in the front half, up to 0.4 at the very back
// with the wall where it was before 17c); the room control (the stage's back wall, 17c) decides its size and scales
// how much of it comes through: small = 0.4 ×, as before = 1 ×, large = 1.6 × (the chain caps the mix at 0.5)
inline double ambMixFor(double depth, double room = 0.5) {
    return 0.4 * clampd((0.55 - depth) / 0.55, 0, 1) * (0.4 + 1.2 * clampd(room, 0, 1));
}
inline double roomSizeFor(double room) { return 0.4 + 0.8 * clampd(room, 0, 1); }

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
// 16c tools are appended (saved sensitivity tables of older sets read as a prefix)
enum Tool { T_WEIGHT, T_WARM, T_BITE, T_AIR, T_TILT, T_SAT, T_COMP, T_EXP, T_TSA, T_TSS,
            T_SAT2, T_DWARM, T_DBITE, T_SMOOTH, T_MBLO, T_MBMID, T_MBHI, NT };
constexpr int NT_16B = T_TSS + 1;
static const double TOOL_LO[NT] = {-12, -9, -10, -12, -6, 0, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0};
static const double TOOL_HI[NT] = {9, 6, 8, 9, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static const double TOOL_H[NT] = {3, 3, 3, 3, 2, 0.3, 0.4, 0.4, 0.4, 0.4, 0.3, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4};   // typical step (cost unit, probe size)

// The characters A..E, from subtle to strong (the user's "rising character"). Each one, at its strength (0..1):
// asks for its own sound (pushes, in steps, added to the stage's), prefers its own tools (costs, lower = preferred)
// and sets its style (compressor attack, release ms, max ratio, parallel mix; saturation evenness). All at 0 = no
// character: only what the stage and the knobs ask, by the cleanest tools. Strengths add (any blend).
static const char* const VAR_NAMES[NVAR] = {"Polish", "Warm", "Bold", "Vintage", "Driven"};
static const double VAR_PUSH[NVAR][NQ] = {
    // Weight Warm Bite Air Texture Punch Tail Steady
    {0, 0, 1, 2, 0, 0, 0, 0.5},           // A Polish: a little clearer and more open, gentle glue
    {0.5, 2.5, 0, -0.5, 0, 0, 0, 1},      // B Warm: body, glue
    {0, 0, 2.5, 0, 0, 3, -1, 0},          // C Bold: forward, sharper attacks
    {0, 2, -1, -3, 0, -1, 0, 1.5},        // D Vintage: rounded soft top, slow glue
    {1, 0, 2, 0, 0, 1.5, 0, 2.5},         // E Driven: dense, held level
};
// Each character's colour, always in its sound at its strength: the least amount of its signature tools
// (saturation drive, compressor amount, transient attack). The solver works around it, so the goal still holds.
static const double VAR_FLOOR[NVAR][NT] = {
    // Weight Warm Bite Air Tilt Sat  Comp  Exp TsA  TsS  Sat2  DWarm DBite Smooth MbLo MbMid MbHi
    {0, 0, 0, 0, 0, 0.0, 0.15, 0, 0.0, 0, 0.0, 0.0, 0.2, 0.0, 0.0, 0.0, 0.0},    // A Polish: gentle glue, harshness tamed
    {0, 0, 0, 0, 0, 0.3, 0.25, 0, 0.0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},    // B Warm: warm (even) harmonics, glue
    {0, 0, 0, 0, 0, 0.3, 0.0, 0, 0.3, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},     // C Bold: edgy (odd) harmonics, sharper attacks
    {0, 0, 0, 0, 0, 0.3, 0.35, 0, 0.0, 0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},   // D Vintage: harmonics before and after the glue
    {0, 0, 0, 0, 0, 0.5, 0.35, 0, 0.0, 0, 0.4, 0.0, 0.0, 0.0, 0.3, 0.3, 0.3},    // E Driven: dense, held in every band
};
static const double VAR_COST[NVAR][NT] = {
    // Weight Warm Bite  Air  Tilt  Sat  Comp  Exp  TsA  TsS  Sat2  DWarm DBite Smooth MbLo MbMid MbHi
    {1.0, 1.0, 1.0, 1.0, 40, 40, 1.0, 3.0, 2.0, 2.0, 40, 1.0, 0.8, 1.0, 1.5, 1.5, 1.5},    // A Polish: EQ and transparent dynamics, no colour
    {1.2, 1.2, 1.2, 1.2, 40, 0.5, 0.7, 3.0, 2.0, 1.2, 0.8, 2.0, 1.2, 1.5, 1.5, 1.5, 1.5},  // B Warm
    {1.2, 1.2, 0.8, 1.2, 40, 0.8, 2.0, 2.0, 0.5, 1.5, 0.8, 1.5, 3.0, 3.0, 2.0, 2.0, 2.0},  // C Bold
    {1.5, 1.5, 1.5, 2.5, 0.6, 0.6, 0.7, 3.0, 2.0, 1.2, 0.5, 1.5, 1.0, 1.0, 1.5, 1.5, 1.5}, // D Vintage: the top comes down by a tilt
    {1.2, 1.2, 1.2, 1.2, 40, 0.3, 0.5, 2.0, 1.0, 1.0, 0.4, 1.5, 1.5, 3.0, 0.6, 0.6, 0.6},  // E Driven
};
static const double VAR_STYLE[NVAR][5] = {
    {10, 150, 2.5, 1.0, 0.5},
    {25, 250, 2.5, 0.6, 0.85},
    {30, 120, 3.0, 1.0, 0.15},
    {40, 400, 2.5, 0.8, 0.6},
    {5, 100, 5.0, 1.0, 0.4},
};
// no character: the cleanest tools, a transparent compressor
static const double NEUTRAL_COST[NT] = {1.0, 1.0, 1.0, 1.0, 40, 40, 1.0, 2.0, 1.5, 1.5, 40, 2.0, 2.0, 1.5, 2.5, 2.5, 2.5};
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
    c[T_SAT2] *= 1 + 30 * (1 - clampd(p.harmonic, 0, 1));
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
    // 16c: the dynamic modules' places and levels (from the reading), Clean's resonance tamer, the ambience
    pl[P_DWARM_THR] = p.dynThr[0];
    pl[P_DBITE_THR] = p.dynThr[1];
    pl[P_MB_F1] = mbSplitLo(p.lowestHz);
    pl[P_MB_F2] = 2500;
    for (int b = 0; b < 3; b++) pl[P_MB_THR1 + b] = p.mbThr[b];
    pl[P_MB_PERIOD] = periodMsOf(p.f0min);
    bool anyRes = false;
    for (int k = 0; k < 4; k++) {
        const double f = p.resHz[k];
        const bool in_ = f > 20 && f > pl[P_LOWCUT_HZ] * 1.5 && f < pl[P_HIGHCUT_HZ] * 0.7;
        pl[P_RES_HZ1 + k] = in_ ? f : 0;
        anyRes = anyRes || in_;
    }
    pl[P_RES_DEPTH] = anyRes ? clampd(in.clean, 0, 1) : 0;
    // 17b: Clean tames what sticks out of the sound's own spectrum (the reading's excess), as far as Clean is turned.
    // It steps back for presence the user asks for (Bite up, from the knob or a character): taming never fights an ask.
    if (p.has17b && p.excHz > 20 && p.excDb >= EXC_MIN_DB) {
        double biteUp = std::max(0.0, in.knob[Q_BITE]);
        for (int v = 0; v < NVAR; v++) biteUp += m.w[v] * std::max(0.0, VAR_PUSH[v][Q_BITE]);
        const double keep = 1 - clampd(biteUp / 4, 0, 1);
        // 17c, firmer (17b was gentle in Live): at the usual level the whole excess is cut at full depth, as a
        // standing cut (P_EXC_FLOOR), a louder harsh hit gets more (the chain: EXC_SLOPE), and Clean reaches full depth
        // at 67 % (40 % → 0.6)
        pl[P_EXC_DEPTH] = clampd(clampd(in.clean, 0, 1) * EXC_DEPTH_PER_CLEAN, 0, 1) * keep;
        pl[P_EXC_HZ] = p.excHz;
        pl[P_EXC_Q] = p.excQ;
        pl[P_EXC_THR] = p.excThr - p.excDb / EXC_SLOPE;
        pl[P_EXC_FLOOR] = -clampd(p.excDb, 0, 12);
    }
    pl[P_AMB_MIX] = ambMixFor(in.depth, in.room);
    pl[P_AMB_SIZE] = roomSizeFor(in.room);
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
    pl[P_SAT2_DRIVE] = t[T_SAT2];
    pl[P_DWARM_AMT] = t[T_DWARM];
    pl[P_DBITE_AMT] = t[T_DBITE];
    pl[P_SMOOTH_AMT] = t[T_SMOOTH];
    pl[P_MB_LO] = t[T_MBLO];
    pl[P_MB_MID] = t[T_MBMID];
    pl[P_MB_HI] = t[T_MBHI];
}
inline void copyAverages(const Plan& from, Plan& to) {
    for (int i = 0; i < NAVG; i++) to[P_AVG_DWARM + i] = from[P_AVG_DWARM + i];
    to[P_AVG_EXC] = from[P_AVG_EXC];
}

// The Average mode's values: every dynamic module at amount 1 on the takes, its unit gain averaged over the sound.
inline void measureAverages(const TakeSet& ts, Plan& pl) {
    Plan u = pl;
    u[P_MAKEUP_DB] = 0; u[P_STAGE_DB] = 0;
    u[P_DWARM_AMT] = 1; u[P_DBITE_AMT] = 1; u[P_SMOOTH_AMT] = 1; u[P_MB_LO] = 1; u[P_MB_MID] = 1; u[P_MB_HI] = 1;
    u[P_RES_DEPTH] = 1;
    u[P_EXC_DEPTH] = pl[P_EXC_HZ] > 20 && pl[P_EXC_DEPTH] > 0 ? 1 : 0;
    double sum[NAVG] = {0}, excSum = 0;
    int n = 0;
    for (size_t k = 0; k < ts.takes.size(); k++) {
        const Audio& a = ts.takes[k];
        Chain ch;
        ch.reset(u, a.sr);
        std::vector<double> L(a.size()), R(a.size());
        for (size_t i = 0; i < a.size(); i++) { L[i] = a.l[i]; R[i] = a.r[i]; }
        ch.process(L.data(), R.data(), L.data(), R.data(), (long)a.size());
        const ChainStats st = ch.stats();
        for (int i = 0; i < NAVG; i++) sum[i] += st.avg[i];
        excSum += st.exc;
        n++;
    }
    for (int i = 0; i < NAVG; i++) pl[P_AVG_DWARM + i] = n ? sum[i] / n : 0;
    pl[P_AVG_EXC] = n ? excSum / n : 0;
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

// Which tools may serve which quality, and in which direction (the solver only moves tools that serve something
// asked; the variation mix then prefers some of them). Keeps each move musical: no "more bite" by cutting everything
// else, no expander to change a tone colour, no "more grain" from the smoothing, no "more warmth" from a dynamic cut.
// 0 = no, 1 = both ways, 2 = only to raise it, 3 = only to lower it.
static const int TOOL_FOR[NQ][NT] = {
    // Weight Warm Bite Air Tilt Sat Comp Exp TsA TsS Sat2 DWarm DBite Smooth MbLo MbMid MbHi
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0},   // Weight
    {0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0},   // Warm
    {0, 0, 1, 0, 0, 2, 0, 0, 0, 0, 2, 0, 3, 0, 0, 0, 0},   // Bite
    {0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   // Air
    {0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 3, 0, 0, 0},   // Texture
    {0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},   // Punch
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0},   // Tail
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 2, 2, 2},   // Steady
};
inline bool toolServes(int q, int k, double steps) {
    const int f = TOOL_FOR[q][k];
    return f == 1 || (f == 2 && steps > 0) || (f == 3 && steps < 0);
}

// the characters' colour: the least of each tool, at their strengths
inline void floorsOf(const Mix& m, const Profile& p, double* floor) {
    for (int k = 0; k < NT; k++) {
        double f = 0;
        for (int v = 0; v < NVAR; v++) f += m.w[v] * VAR_FLOOR[v][k];
        if (k == T_SAT || k == T_SAT2) f *= clampd(p.harmonic, 0, 1);   // safety: little or no saturation on noisy, dense sound
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
    double lambda = 0.1;
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
    const double keepW = 0.1 + 0.5 * identityStrength(in);   // Identity: protect what isn't asked
    for (int q = 0; q < NQ; q++) {
        const double steps = clampd(push[q] + in.knob[q], -10, 10);
        const bool can = u.ok[q] && (steps >= 0 ? u.up[q] : u.dn[q]) > 0.05;
        askedOut[q] = zok[q] && u.ok[q] ? steps : 0;
        pb.T[q] = Z[q] + (zok[q] ? stepsToDb(u, q, steps) : 0);
        pb.scale[q] = std::max(0.5, std::max(u.up[q], u.dn[q]) / 10);   // never finer than the measures' noise
        bool asked = std::fabs(steps) >= 0.25 && can;
        bool tool = false;
        for (int k = 0; k < NT; k++) if (toolServes(q, k, steps) && pb.cost[k] < 20) { tool = true; if (asked) pb.on[k] = true; }
        // asked, but this variation mix has no tool for it (e.g. grain in "Clean"): it is not reached, and other
        // tools must not be bent to fake it
        if (asked && !tool) asked = false, pb.unreachable[q] = true;
        pb.w[q] = !zok[q] || !u.ok[q] || pb.unreachable[q] ? 0 : (asked ? 1.0 : keepW);
    }
    // Identity (17b): with some strength, the qualities nobody asked to change get their own two-way tools (the EQ bands)
    // enabled as holders, so a move's side effect on them can be held back where the sound has them (an Air lift that
    // also lifts Bite gets a small Bite counter-move). Off at 0 %: then side effects just show.
    if (identityStrength(in) >= 0.25) {
        bool anyAsked = false;
        for (int k = 0; k < NT; k++) anyAsked = anyAsked || pb.on[k];
        for (int q = 0; anyAsked && q < NQ; q++) {
            if (pb.w[q] <= 0 || pb.w[q] >= 1.0) continue;
            for (int k = 0; k < NT; k++) if (TOOL_FOR[q][k] == 1 && pb.cost[k] < 20) pb.on[k] = true;
        }
    }
    // a quality nobody asked for, with none of its tools in play: its change is a side effect to show, not a cost
    for (int q = 0; q < NQ; q++) {
        if (pb.w[q] <= 0 || pb.w[q] >= 1.0) continue;
        bool served = false;
        for (int k = 0; k < NT; k++) served = served || (pb.on[k] && TOOL_FOR[q][k] != 0);
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
constexpr double JNOISE = 0.75;   // dB per typical step
inline double toolLo(const Problem& pb, int k) { return pb.floor[k] > 0 ? std::max(TOOL_LO[k], pb.floor[k]) : TOOL_LO[k]; }
inline void gnStep(const Problem& pb, const double J[NM][NT], const double* M, const double* t, double mu, double* tNew) {
    int idx[NT], n = 0;
    for (int k = 0; k < NT; k++) { tNew[k] = pb.on[k] ? clampd(t[k], toolLo(pb, k), TOOL_HI[k]) : pb.floor[k]; if (pb.on[k]) idx[n++] = k; }
    if (!n) return;
    double A[NT][NT], g[NT], d[NT];
    for (int i = 0; i < n; i++) { g[i] = 0; for (int j = 0; j < n; j++) A[i][j] = 0; }
    // a tool's small effect on a quality it doesn't serve is mostly the measures' noise: the step ignores it (a big
    // side effect still counts), or the solver would chase noise with tools that can't really do it
    auto row = [&](const double* Jr, double r, double s2, int q) {
        double jh[NT];
        for (int i = 0; i < n; i++) {
            const int k = idx[i];
            jh[i] = Jr[k] * TOOL_H[k];   // in tool steps (better conditioned)
            if (q >= 0 && TOOL_FOR[q][k] == 0 && std::fabs(jh[i]) < JNOISE) jh[i] = 0;
        }
        for (int i = 0; i < n; i++) {
            g[i] += s2 * jh[i] * r;
            for (int j = 0; j < n; j++) A[i][j] += s2 * jh[i] * jh[j];
        }
    };
    for (int q = 0; q < NQ; q++) {
        if (pb.w[q] <= 0) continue;
        row(J[q], M[q] - pb.T[q], pb.w[q] / (pb.scale[q] * pb.scale[q]), q);
    }
    row(J[NQ], M[NQ] - pb.inLoud, LOUD_W / (LOUD_SCALE * LOUD_SCALE), -1);
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

// a sensitivity, kept physical: one typical step of a tool moves a measure by at most JMAX dB
constexpr double JMAX = 15.0;
inline double clampJ(double j, int k) { return clampd(j, -JMAX / TOOL_H[k], JMAX / TOOL_H[k]); }

// The slow path. `warm` (optional): a previous solution on the same takes to start from (its J and tools).
// `probeAll` false: only the tools this ask enables are probed now (the plan plays sooner); the device then fills in
// the rest with completeSensitivities() in the background.
inline Solution solve(const TakeSet& ts, const Profile& p, const Inputs& in, int solo, const Solution* warm = nullptr,
                      int maxIter = 8, bool probeAll = true) {
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
    // Sensitivities, probed one tool at a time from the current point. Every tool's column is filled (here, or right
    // after by completeSensitivities), so the saved table answers any later knob or character on the fast path, also
    // after a reopen without the takes.
    auto probeTool = [&](int k) {
        double tt[NT], out[NM];
        for (int i = 0; i < NT; i++) tt[i] = t[i];
        double h = TOOL_H[k];
        if (tt[k] + h > TOOL_HI[k]) h = -h;
        tt[k] += h;
        eval(tt, out);
        for (int m = 0; m < NM; m++) s.J[m][k] = clampJ((out[m] - M[m]) / h, k);
    };
    auto probe = [&]() { for (int k = 0; k < NT; k++) if (pb.on[k]) probeTool(k); };
    bool any = false;
    for (int k = 0; k < NT; k++) any = any || pb.on[k];
    if (warm && warm->ok) {
        for (int m = 0; m < NM; m++) for (int k = 0; k < NT; k++) s.J[m][k] = warm->J[m][k];
        for (int k = 0; k < NT; k++) {
            bool known = false;
            for (int m = 0; m < NM; m++) known = known || s.J[m][k] != 0.0;
            if (!known && (probeAll || pb.on[k])) probeTool(k);
        }
    } else {
        for (int k = 0; k < NT; k++) if (probeAll || pb.on[k]) probeTool(k);
    }
    // An ask the enabled tools can't get near (their sensitivities, two typical steps each, add up to under half of
    // it) is reported unreached instead of chased: chasing it bends everything else for nothing.
    Problem pbF = pb;
    for (int q = 0; q < NQ; q++) {
        if (pbF.w[q] < 1.0) continue;
        const double want = std::fabs(pbF.T[q] - M[q]);
        double can = 0;
        for (int k = 0; k < NT; k++) if (pbF.on[k] && TOOL_FOR[q][k] != 0) can += 2 * std::fabs(s.J[q][k]) * TOOL_H[k];
        if (want > 0.3 && can < 0.5 * want) { pbF.w[q] = 0; pbF.unreachable[q] = true; }
    }
    const Problem& pbS = pbF;
    if (any) {
        double cur = costOf(pbS, M, t);
        double mu = 0.01;
        bool fresh = !(warm && warm->ok);   // J was just probed here
        bool probedHere = fresh;            // J has been measured at a point of this solve (not only warm)
        for (int it = 0; it < maxIter; it++) {
            double tn[NT], Mn[NM];
            gnStep(pbS, s.J, M, t, mu, tn);
            double step = 0;
            for (int k = 0; k < NT; k++) step += std::fabs(tn[k] - t[k]) / TOOL_H[k];
            if (step < 1e-3) {
                if (fresh) break;
                probe(); fresh = true; probedHere = true; continue;
            }
            eval(tn, Mn);
            const double c = costOf(pbS, Mn, tn);
            // Broyden update of J with what this step actually did, in tool steps (so a dB of EQ and a tenth of
            // compression count alike); a tiny step says more about the measures' noise than about the tools, so it
            // teaches nothing, and no tool's typical step may move a measure by more than JMAX dB
            double dt[NT], ds[NT], dd = 0;
            for (int k = 0; k < NT; k++) { dt[k] = tn[k] - t[k]; ds[k] = dt[k] / TOOL_H[k]; dd += ds[k] * ds[k]; }
            if (dd > 0.0025) {
                for (int m = 0; m < NM; m++) {
                    double pred = 0;
                    for (int k = 0; k < NT; k++) pred += s.J[m][k] * dt[k];
                    const double err = (Mn[m] - M[m]) - pred;
                    for (int k = 0; k < NT; k++) s.J[m][k] = clampJ(s.J[m][k] + err * ds[k] / dd / TOOL_H[k], k);
                }
            }
            // the measures aren't linear in the tools: a step that overshoots is tried shorter (half, quarter)
            // along the same direction before the model is blamed
            double cBest = c;
            for (double f : {0.5, 0.25}) {
                if (cBest < cur) break;
                double th[NT], Mh[NM];
                for (int k = 0; k < NT; k++) th[k] = t[k] + f * (tn[k] - t[k]);
                eval(th, Mh);
                const double ch = costOf(pbS, Mh, th);
                if (ch < cBest) { cBest = ch; for (int k = 0; k < NT; k++) tn[k] = th[k]; for (int m = 0; m < NM; m++) Mn[m] = Mh[m]; }
            }
            if (cBest < cur) {
                const double c2 = cBest;
                for (int k = 0; k < NT; k++) t[k] = tn[k];
                for (int m = 0; m < NM; m++) M[m] = Mn[m];
                const bool big = cur - c2 > 0.002 * (cur + 1e-9);
                cur = c2;
                mu = std::max(mu / 3, 1e-4);
                // a small gain is a reason to stop, unless the model came from elsewhere and was never measured here:
                // a warm start from another character mix (17c) can stall far from the goal, so it is measured once
                if (!big && probedHere) break;
                fresh = false;
                if (!big) { probe(); fresh = true; probedHere = true; }
            } else if (!fresh) {
                probe(); fresh = true; probedHere = true;   // the model was off: measure it again here
            } else {
                mu *= 4;
            }
            double worst = 0;
            for (int q = 0; q < NQ; q++) if (pbS.w[q] >= 1.0) worst = std::max(worst, std::fabs(M[q] - pbS.T[q]) / pbS.scale[q]);
            if (worst < 0.2) break;
        }
    }
    for (int k = 0; k < NT; k++) s.tools[k] = t[k];
    for (int m = 0; m < NM; m++) s.M[m] = M[m];
    s.plan = base;
    applyTools(s.plan, t);
    measureAverages(ts, s.plan);
    finishSolution(s, pbS);
    s.ok = true;
    return s;
}

// The tools a solve didn't probe (nothing asked of them yet), probed at its solution: the fast path then knows every
// tool. Worker thread.
inline void completeSensitivities(const TakeSet& ts, const Profile& p, const Inputs& in, int solo, Solution& s) {
    if (!s.ok || ts.takes.empty()) return;
    const Plan base = basePlan(in, p, mixOf(in, solo));
    for (int k = 0; k < NT; k++) {
        bool known = false;
        for (int m = 0; m < NM; m++) known = known || s.J[m][k] != 0.0;
        if (known) continue;
        double tt[NT], out[NM];
        for (int i = 0; i < NT; i++) tt[i] = s.tools[i];
        double h = TOOL_H[k];
        if (tt[k] + h > TOOL_HI[k]) h = -h;
        tt[k] += h;
        Plan pl = base;
        applyTools(pl, tt);
        toMeasures(renderMeasure(ts, pl, in.lo, in.hi), out);
        for (int m = 0; m < NM; m++) s.J[m][k] = clampJ((out[m] - s.M[m]) / h, k);
    }
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
    copyAverages(last.plan, s.plan);
    finishSolution(s, pb);
    return s;
}

// ------------------------------------------------------------------------------------------------- moves
// What the device is doing, for the Moves list. The UI turns the kind into words; everything is numbers (no new
// symbols in Max per update).
enum MoveKind {
    MV_NONE, MV_LOWS_UP, MV_LOWS_DOWN, MV_WARM_UP, MV_WARM_DOWN, MV_BITE_UP, MV_BITE_DOWN, MV_AIR_UP, MV_AIR_DOWN,
    MV_TILT_UP, MV_TILT_DOWN, MV_SAT_EVEN, MV_SAT_ODD, MV_COMP, MV_EXP, MV_ATTACK_UP, MV_ATTACK_DOWN, MV_SUSTAIN_UP,
    MV_SUSTAIN_DOWN, MV_LOWCUT, MV_HIGHCUT, MV_WIDER, MV_NARROWER, MV_PAN, MV_MAKEUP, MV_STAGE_LEVEL,
    // 16c
    MV_SAT2_EVEN, MV_SAT2_ODD, MV_DWARM, MV_DBITE, MV_TAMER, MV_SMOOTH, MV_MB_LO, MV_MB_MID, MV_MB_HI, MV_AMBIENCE,
    // 17b
    MV_EXCESS,
    // 17c: Identity holding a quality nobody asked to change (its own EQ band moved against a side effect); the
    // move's quality says which ("Holding Bite")
    MV_HOLD
};
struct Move { int kind; double value; int q; double reached, asked; };

// `scale`: Scale's multiplier on the shaping (the list shows what plays)
inline std::vector<Move> movesOf(const Solution& s, double scale = 1.0) {
    std::vector<Move> mv;
    if (!s.ok) return mv;
    const Plan& pl = s.plan;
    double sc[NT];
    for (int k = 0; k < NT; k++) sc[k] = s.tools[k] * scale;
    auto add = [&](int kind, double value, int q) {
        Move m;
        m.kind = kind; m.value = value; m.q = q;
        m.reached = q >= 0 ? s.reached[q] : 0;
        m.asked = q >= 0 ? s.asked[q] : 0;
        mv.push_back(m);
    };
    const double* t = sc;
    // an EQ band serves only its own quality (TOOL_FOR): moved while nothing asked for that quality, it is Identity
    // holding it against another move's side effect (17c: shown as such, so Identity's work is visible)
    auto eq = [&](int k, int q, int up, int down) {
        if (std::fabs(t[k]) < 0.5) return;
        if (std::fabs(s.asked[q]) < 0.25) add(MV_HOLD, t[k], q);
        else add(t[k] > 0 ? up : down, t[k], q);
    };
    eq(T_WEIGHT, Q_WEIGHT, MV_LOWS_UP, MV_LOWS_DOWN);
    eq(T_WARM, Q_WARM, MV_WARM_UP, MV_WARM_DOWN);
    eq(T_BITE, Q_BITE, MV_BITE_UP, MV_BITE_DOWN);
    eq(T_AIR, Q_AIR, MV_AIR_UP, MV_AIR_DOWN);
    if (std::fabs(t[T_TILT]) >= 0.5) add(t[T_TILT] > 0 ? MV_TILT_UP : MV_TILT_DOWN, t[T_TILT], Q_AIR);
    if (t[T_SAT] >= 0.05) add(pl[P_SAT_EVEN] >= 0.5 ? MV_SAT_EVEN : MV_SAT_ODD, t[T_SAT], Q_TEXTURE);
    if (t[T_COMP] >= 0.05) add(MV_COMP, pl[P_COMP_REL], Q_STEADY);
    if (t[T_EXP] >= 0.05) add(MV_EXP, t[T_EXP], Q_STEADY);
    if (std::fabs(t[T_TSA]) >= 0.05) add(t[T_TSA] > 0 ? MV_ATTACK_UP : MV_ATTACK_DOWN, t[T_TSA], Q_PUNCH);
    if (std::fabs(t[T_TSS]) >= 0.05) add(t[T_TSS] > 0 ? MV_SUSTAIN_UP : MV_SUSTAIN_DOWN, t[T_TSS], Q_TAIL);
    if (t[T_SAT2] >= 0.05) add(pl[P_SAT_EVEN] >= 0.5 ? MV_SAT2_EVEN : MV_SAT2_ODD, t[T_SAT2], Q_TEXTURE);
    if (t[T_DWARM] >= 0.05) add(MV_DWARM, t[T_DWARM], Q_WARM);
    if (t[T_DBITE] >= 0.05) add(MV_DBITE, t[T_DBITE], Q_BITE);
    if (t[T_SMOOTH] >= 0.05) add(MV_SMOOTH, t[T_SMOOTH], Q_TEXTURE);
    if (t[T_MBLO] >= 0.05) add(MV_MB_LO, t[T_MBLO], Q_STEADY);
    if (t[T_MBMID] >= 0.05) add(MV_MB_MID, t[T_MBMID], Q_STEADY);
    if (t[T_MBHI] >= 0.05) add(MV_MB_HI, t[T_MBHI], Q_STEADY);
    if (pl[P_RES_DEPTH] > 0.02) {
        int n = 0;
        for (int k = 0; k < 4; k++) if (pl[P_RES_HZ1 + k] > 20) n++;
        if (n) add(MV_TAMER, n, -1);
    }
    if (pl[P_EXC_DEPTH] > 0.02 && pl[P_EXC_HZ] > 20) add(MV_EXCESS, pl[P_EXC_HZ], -1);
    if (pl[P_AMB_MIX] > 0.005) add(MV_AMBIENCE, pl[P_AMB_MIX], -1);
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
