// Lifegiver: the main thread. Takes the patch's messages (Live parameters, the view's clicks), runs Listen (takes →
// reading → solve) through the host's worker thread, hands plans to the audio thread, tells the view what happened
// (numbers only: no new Max symbols per update), and keeps the saved state.
//
// Flow:
//   Arm → the capture fills (audio thread) → a take (max 4; a 5th replaces the oldest) → analyse (worker) → the reading
//   → solve the active variation mix (worker) → the plan plays; then the five variations are solved one by one in the
//   background (their reach, and Solo).
//   A knob, the stage, the range, Fit, Clean or a variation strength moves → the fast path answers at once (one step
//   on the last solve's sensitivities) → the slow path re-solves 0.4 s after the last change (if the takes are here).
//
// Saved in the set (one hidden pattr, a list of numbers: state.h): the reading, the last solve (incl. its
// sensitivities), the playing plan and the inputs that made it. A reopened set plays that plan exactly; knob turns
// then use the saved sensitivities ("Re-listen to refine": the takes are not saved).
//
// Loading: init() → the host is asked for the saved state now, again at +300 ms and +800 ms. Nothing valid by +1.5 s =
// a new device, but its first save waits until +4.5 s, and a valid state that still arrives within 10 s (Live
// restoring late) replaces it.
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "brain.h"
#include "processor.h"
#include "state.h"
#include "version.h"

namespace lg {

constexpr int STATE_FORMAT = 2;
constexpr double LATE_RESTORE_MS = 10000;
constexpr double NEW_DEVICE_SAVE_MS = 3000;
constexpr double SOLVE_DEBOUNCE_MS = 400;
constexpr double SAVE_DEBOUNCE_MS = 1500;
constexpr int MAX_TAKES = 4;

// What the controller asks of its host (the Max wrapper, or the test rig).
struct Host {
    virtual ~Host() {}
    virtual void requestState() = 0;                          // ask the set's store (answer: message "statelist")
    virtual void storeState(const std::vector<double>&) = 0;  // put this list into the store
    virtual void log(const std::string&) {}                   // a line for the Max window
    virtual void ui(const std::string&, const std::vector<double>&) {}   // a message to the view (numbers only)
    virtual void param(const std::string&, double) {}         // set a Live parameter (the role buttons' presets)
    // run `work` on the worker thread, then `done` on the main thread (from a later tick)
    virtual void post(std::function<void()> work, std::function<void()> done) = 0;
};

// the Live parameters, by message name
struct ParamDef { const char* name; double lo, hi; };
static const ParamDef KNOB_PARAMS[NQ] = {{"weight", -10, 10}, {"warm", -10, 10}, {"bite", -10, 10}, {"air", -10, 10},
                                          {"texture", -10, 10}, {"punch", -10, 10}, {"tail", -10, 10}, {"steady", -10, 10}};

class Controller {
public:
    struct Atom { bool num = true; double d = 0; std::string s; };

    bool trace = false;   // load/save diagnostics in the Max window

    Controller(Processor* p, Host* h) : pr(p), host(h) {}
    ~Controller() { delete pending; }

    // The device finished loading (live.thisdevice).
    void init(double ms) {
        if (initAt >= 0) return;
        nowMs = ms;
        initAt = ms;
        asks = 1;
        host->requestState();
        sendUi();
    }

    // A message from the patch: selector + arguments. A word where a number belongs counts as the default.
    void message(const std::string& m, const std::vector<Atom>& a) {
        auto n = [&](size_t i, double def) { return (i < a.size() && a[i].num) ? a[i].d : def; };
        for (int q = 0; q < NQ; q++) if (m == KNOB_PARAMS[q].name) { setInput(in.knob[q], clampd(n(0, 0), -10, 10)); return; }
        if (m == "amount") { amountPct = clampd(n(0, 100), 0, 100); pr->setAmount(amountPct / 100.0); return; }
        if (m == "pan") { setInput(in.pan, clampd(n(0, 0), -1, 1)); return; }
        if (m == "depth") { setInput(in.depth, clampd(n(0, 0.82), 0, 1)); return; }
        if (m == "width") { setInput(in.width, clampd(n(0, 0.5), 0, 1)); return; }
        if (m == "edge") { setInput(in.edge, clampd(n(0, 0), 0, 1)); return; }
        if (m == "lo") { setInput(in.lo, clampd(n(0, 20), 20, 20000)); return; }
        if (m == "hi") { setInput(in.hi, clampd(n(0, 20000), 20, 20000)); return; }
        if (m == "fit") { setInput(in.fit, clampd(n(0, 0.5), 0, 1)); return; }
        if (m == "clean") { setInput(in.clean, clampd(n(0, 40), 0, 100) / 100.0); return; }
        if (m == "role") { const int r = (int)clampd(std::round(n(0, 0)), 0, NROLES - 1); if (r != in.role) { in.role = r; inputsChanged(); } return; }
        if (m.size() == 4 && m.compare(0, 3, "var") == 0 && m[3] >= 'a' && m[3] <= 'e') { setInput(in.vars[m[3] - 'a'], clampd(n(0, 0), 0, 100) / 100.0); return; }
        if (m == "pickrole") { pickRole((int)clampd(std::round(n(0, 0)), 0, NROLES - 1)); return; }
        if (m == "solo") { const int s = (int)clampd(std::round(n(0, -1)), -1, NVAR - 1); solo = s == solo ? -1 : s; inputsChanged(); return; }
        if (m == "compare") { compareOn = n(0, 0) > 0.5; applyCompare(); sendUi(); return; }
        if (m == "arm") { arm(); return; }
        if (m == "removetake") { removeTake((int)n(0, -1)); return; }
        if (m == "statelist") { stateFrom(a); return; }
        if (m == "uiready") { sendUi(); return; }
        host->log("lifegiver: unknown message \"" + m + "\"");
    }

    // Called every ~15 ms on the main thread.
    void tick(double ms) {
        nowMs = ms;
        Report r;
        while (pr->reps.pop(r)) if (r.type == R_GARBAGE_PLAN) delete (Plan*)r.ptr;
        if (pending) sendPending();
        if (!restored && initAt >= 0) {
            if (asks == 1 && ms >= initAt + 300) { asks = 2; host->requestState(); }
            if (asks == 2 && ms >= initAt + 800) { asks = 3; host->requestState(); }
            if (ms >= initAt + 1500) noState();
        }
        // Listen
        const int cs = pr->capture.state.load();
        if (cs == Capture::DONE) takeDone();
        if (cs != lastCapState || (cs == Capture::CAPTURING && ms - lastProgressUi > 100)) {
            lastCapState = cs; lastProgressUi = ms;
            sendTakesUi();
        }
        if (solveAt >= 0 && ms >= solveAt && !busy) { solveAt = -1; startSolve(); }
        if (saveAt >= 0 && ms >= saveAt && restored) { saveAt = -1; save(); }
    }

    // ---- state -------------------------------------------------------------------------------------------------
    void stateFrom(const std::vector<Atom>& a) {
        std::vector<double> v;
        bool words = false;
        for (auto& x : a) { if (!x.num) { words = true; break; } v.push_back(x.d); }
        StateDoc d;
        const bool valid = !words && decodeState(v, d) && (int)d.get1("v", 0) >= 1 && d.get("id");
        if (valid && v == lastSaved) return;   // our own save coming back
        if (trace) host->log("lifegiver: [load] state answered at " + std::to_string((long)(nowMs - initAt)) + " ms" + (valid ? " (valid)" : ""));
        if (!valid) return;
        if (!restored) { readState(d); return; }
        // a valid, different state after we had decided "new device": Live restored late; the set's state wins
        if (byFallback && nowMs - initAt <= LATE_RESTORE_MS) {
            host->log("lifegiver: the set's state arrived late; using it instead of the new id");
            readState(d);
        }
    }

    void readState(const StateDoc& d) {
        restored = true;
        byFallback = false;
        saveAt = -1;
        deviceId = (uint32_t)d.get1("id", 0);
        savedBy = d.gets("saved_by");
        const int fmt = (int)d.get1("v", 0);
        if (fmt >= 2) readSound(d);
        host->log("lifegiver: state restored, id " + std::to_string(deviceId) + (prof.valid ? ", reading and plan kept" : "") +
                  " (saved by engine " + (savedBy.empty() ? std::string("?") : savedBy) + ")");
        lastSaved = encodeState(d);
        if (fmt != STATE_FORMAT || savedBy != VERSION) dirty();   // re-save in this build's format
        sendUi();
    }

    void noState() {
        restored = true;
        byFallback = true;
        std::random_device rd;
        deviceId = 1000 + rd() % 9000;
        savedBy.clear();
        host->log("lifegiver: new device, id " + std::to_string(deviceId));
        dirty(NEW_DEVICE_SAVE_MS);
    }

    void writeState(StateDoc& d) const {
        d.num.clear(); d.str.clear();
        d.num["v"] = {(double)STATE_FORMAT};
        d.num["id"] = {(double)deviceId};
        d.str["saved_by"] = VERSION;
        if (!prof.valid) return;
        // the reading
        std::vector<double> pv = {(double)prof.takes, prof.activeSec, (double)prof.onsets, prof.f0min, prof.f0max, prof.harmonic,
                                  prof.sustained, prof.corr, prof.sideDb, prof.loudness, prof.lowestHz, prof.peakRef, prof.levelHi, prof.levelLo};
        d.num["prof"] = pv;
        std::vector<double> rd;
        for (int q = 0; q < NQ; q++) { rd.push_back(prof.reading[q]); rd.push_back(prof.readValid[q] ? 1 : 0); rd.push_back(prof.certainty[q]); }
        d.num["read"] = rd;
        d.num["oct6"] = prof.oct6;
        // the last solve (the fast path's starting point)
        if (sol.ok) {
            std::vector<double> sv(sol.tools, sol.tools + NT);
            for (int m = 0; m < NM; m++) for (int k = 0; k < NT; k++) sv.push_back(sol.J[m][k]);
            for (int m = 0; m < NM; m++) { sv.push_back(sol.M[m]); sv.push_back(sol.Z[m]); }
            for (int q = 0; q < NQ; q++) { sv.push_back(sol.zok[q] ? 1 : 0); sv.push_back(sol.units.up[q]); sv.push_back(sol.units.dn[q]); sv.push_back(sol.units.ok[q] ? 1 : 0); }
            sv.push_back(sol.inLoud);
            d.num["sol"] = sv;
            d.num["solplan"] = std::vector<double>(sol.plan.v, sol.plan.v + NP);
        }
        // what plays, and the inputs that made it
        d.num["play"] = std::vector<double>(play.v, play.v + NP);
        d.num["playin"] = inputsVector(playInputs);
        d.num["playsolo"] = {(double)playSolo};
        d.num["playreach"] = {playReach};
        std::vector<double> vr(varReach, varReach + NVAR);
        d.num["varreach"] = vr;
    }

    uint32_t id() const { return deviceId; }
    bool isRestored() const { return restored; }
    const Profile& profile() const { return prof; }
    const Plan& playing() const { return play; }
    const Solution& lastSolve() const { return sol; }
    const Inputs& inputs() const { return in; }
    int takeCount() const { return (int)takes.size(); }
    bool isBusy() const { return busy; }
    double reachOf(int v) const { return varReach[v]; }

private:
    Processor* pr;
    Host* host;
    double amountPct = 100;
    Inputs in;
    int solo = -1;
    bool compareOn = false;
    uint32_t deviceId = 0;
    std::string savedBy;
    bool restored = false, byFallback = false;
    double nowMs = 0, saveAt = -1, initAt = -1, solveAt = -1, lastProgressUi = 0;
    int asks = 0, lastCapState = -1;
    std::vector<double> lastSaved;

    std::vector<Audio> takes;
    std::shared_ptr<TakeSet> solveSet;
    Profile prof;
    Solution sol;                  // the last slow solve of the active mix
    Solution varSol[NVAR];         // each variation alone (background)
    double varReach[NVAR] = {-1, -1, -1, -1, -1};
    Solution shown;                // what the view shows (asked/reached/moves) for the playing plan
    Plan play;                     // the playing plan
    Inputs playInputs;
    int playSolo = -1;
    double playReach = -1;
    Plan* pending = nullptr;       // a plan waiting for the audio thread to take the previous one
    std::unique_ptr<Plan> restoredPlay;   // the plan a reopened set played, and the inputs that made it
    Inputs restoredInputs;
    int restoredSolo = -1;
    Solution restoredShown;
    int gen = 0;                   // bumps when the takes change: older worker results are dropped
    int solveGen = 0;
    bool busy = false;

    // ---- inputs ------------------------------------------------------------------------------------------------
    void setInput(double& field, double v) {
        if (field == v) return;
        field = v;
        inputsChanged();
    }
    static std::vector<double> inputsVector(const Inputs& i) {
        std::vector<double> v(i.knob, i.knob + NQ);
        const double rest[] = {(double)i.role, i.pan, i.depth, i.width, i.edge, i.lo, i.hi, i.fit, i.clean};
        v.insert(v.end(), rest, rest + 9);
        v.insert(v.end(), i.vars, i.vars + NVAR);
        return v;
    }
    static bool inputsFrom(const std::vector<double>* v, Inputs& i) {
        if (!v || v->size() != (size_t)(NQ + 9 + NVAR)) return false;
        for (int q = 0; q < NQ; q++) i.knob[q] = (*v)[(size_t)q];
        size_t p = NQ;
        i.role = (int)(*v)[p++]; i.pan = (*v)[p++]; i.depth = (*v)[p++]; i.width = (*v)[p++]; i.edge = (*v)[p++];
        i.lo = (*v)[p++]; i.hi = (*v)[p++]; i.fit = (*v)[p++]; i.clean = (*v)[p++];
        for (int k = 0; k < NVAR; k++) i.vars[k] = (*v)[p++];
        return true;
    }
    static bool sameInputs(const Inputs& a, const Inputs& b) { return inputsVector(a) == inputsVector(b); }

    void pickRole(int r) {
        const RolePreset p = rolePresetFor(r, prof);
        in.role = r;
        // the patch's parameters follow (and echo back here; the values are already set, so nothing happens twice)
        in.pan = p.pan; in.depth = p.depth; in.width = p.width; in.edge = p.edge; in.lo = p.lo; in.hi = p.hi;
        host->param("role", r);
        host->param("pan", p.pan); host->param("depth", p.depth); host->param("width", p.width);
        host->param("edge", p.edge); host->param("lo", p.lo); host->param("hi", p.hi);
        inputsChanged();
    }

    // the fast path, then the slow path a little later
    void inputsChanged() {
        applyCompare();
        if (!prof.valid) { sendUi(); return; }
        // back at exactly the inputs a reopened set was saved with: exactly the plan it played
        if (restoredPlay && sameInputs(in, restoredInputs) && solo == restoredSolo) {
            if (!(sameInputs(in, playInputs) && solo == playSolo)) { shown = restoredShown; setPlay(*restoredPlay, restoredShown.reach); }
            sendUi();
            return;
        }
        if (!(sameInputs(in, playInputs) && solo == playSolo)) {
            Solution s = (solo >= 0 && varSol[solo].ok) ? fastStep(varSol[solo], prof, in, solo) : fastStep(sol, prof, in, solo);
            if (s.ok) { shown = s; setPlay(s.plan, s.reach); }
            else { Plan b = basePlan(in, prof, mixOf(in, solo)); setPlay(b, -1); }
            if (solveSet) solveAt = nowMs + SOLVE_DEBOUNCE_MS;
        }
        sendUi();
    }

    void setPlay(const Plan& p, double reach) {
        play = p;
        playInputs = in;
        playSolo = solo;
        playReach = reach;
        sendPlanToAudio(play);
        dirty(SAVE_DEBOUNCE_MS);
    }

    void applyCompare() {
        const double stage = prof.valid ? stageLevelDb(in) : 0.0;
        pr->setCompare(compareOn, stage);
    }

    // at most one plan in flight: a newer one waits (replacing an older waiting one) until the audio thread took it
    void sendPlanToAudio(const Plan& p) {
        delete pending;
        pending = new Plan(p);
        sendPending();
    }
    void sendPending() {
        if (!pending) return;
        if (pr->cmds.push(Cmd{C_PLAN, 0, pending})) pending = nullptr;
    }

    // ---- Listen ------------------------------------------------------------------------------------------------
    void arm() {
        const int cs = pr->capture.state.load();
        if (cs == Capture::ARMED || cs == Capture::CAPTURING) { pr->capture.cancel(); sendTakesUi(); return; }   // Arm again = stop
        pr->capture.arm();
        sendTakesUi();
    }

    void takeDone() {
        const long n = pr->capture.frames();
        Audio t;
        t.sr = pr->sampleRate();
        t.l.assign(pr->capture.left(), pr->capture.left() + n);
        t.r.assign(pr->capture.right(), pr->capture.right() + n);
        pr->capture.release();
        if (n < (long)(0.3 * t.sr)) { host->log("lifegiver: that take was too short (under 0.3 s of sound)"); sendTakesUi(); return; }
        if ((int)takes.size() >= MAX_TAKES) takes.erase(takes.begin());   // a 5th replaces the oldest
        takes.push_back(std::move(t));
        restoredPlay.reset();
        startAnalysis();
    }

    void removeTake(int i) {
        if (i < 0 || i >= (int)takes.size()) return;
        takes.erase(takes.begin() + i);
        if (takes.empty()) {   // back to "Listen first": clean pass-through
            gen++;
            solveSet.reset();
            prof = Profile();
            sol = Solution(); shown = Solution();
            for (int v = 0; v < NVAR; v++) { varSol[v] = Solution(); varReach[v] = -1; }
            Plan neutral;
            setPlay(neutral, -1);
            sendUi();
            return;
        }
        startAnalysis();
    }

    void startAnalysis() {
        const int g = ++gen;
        busy = true;
        auto tk = std::make_shared<std::vector<Audio>>(takes);
        auto outP = std::make_shared<Profile>();
        auto outS = std::make_shared<std::shared_ptr<TakeSet>>();
        host->post([tk, outP, outS]() {
            auto ts = makeTakeSet(*tk);
            *outP = analyze(*ts);
            *outS = solveSetOf(*tk);
        }, [this, g, outP, outS]() {
            if (g != gen) return;
            prof = *outP;
            solveSet = *outS;
            sol = Solution();
            for (int v = 0; v < NVAR; v++) { varSol[v] = Solution(); varReach[v] = -1; }
            busy = false;
            startSolve();
        });
        sendUi();
    }

    void startSolve() {
        if (!solveSet || !prof.valid) return;
        busy = true;
        const int g = gen, sg = ++solveGen;
        auto ss = solveSet;
        const Profile p = prof;
        const Inputs inp = in;
        auto warm = std::make_shared<Solution>(sol);
        auto out = std::make_shared<Solution>();
        host->post([ss, p, inp, warm, out]() {
            *out = solve(*ss, p, inp, -1, warm->ok ? warm.get() : nullptr);
        }, [this, g, sg, out, inp]() {
            if (g != gen || sg != solveGen) return;
            busy = false;
            sol = *out;
            restoredPlay.reset();
            if (sol.ok) {
                // the inputs may have moved while it solved: step from the new solution to where they are now
                const bool same = sameInputs(inp, in) && solo < 0;
                Solution s = same ? sol : ((solo >= 0 && varSol[solo].ok) ? fastStep(varSol[solo], prof, in, solo) : fastStep(sol, prof, in, solo));
                shown = s;
                setPlay(s.plan, s.reach);
            }
            sendUi();
            startVariations(0);
        });
        sendUi();
    }

    // the five variations alone, one after the other, in the background (each starts from the mix's sensitivities)
    void startVariations(int v) {
        if (v >= NVAR || !solveSet || !sol.ok) return;
        const int g = gen, sg = solveGen;
        auto ss = solveSet;
        const Profile p = prof;
        const Inputs inp = in;
        auto warm = std::make_shared<Solution>(sol);
        auto out = std::make_shared<Solution>();
        host->post([ss, p, inp, warm, out, v]() {
            *out = solve(*ss, p, inp, v, warm.get(), 5);
        }, [this, g, sg, out, v]() {
            if (g != gen || sg != solveGen) return;
            varSol[v] = *out;
            varReach[v] = out->ok ? out->reach : -1;
            if (solo == v) { playInputs = Inputs(); inputsChanged(); }   // soloing this one: use its own solve now
            sendVarsUi();
            startVariations(v + 1);
        });
    }

    // ---- saving ------------------------------------------------------------------------------------------------
    void readSound(const StateDoc& d) {
        const std::vector<double>* pv = d.get("prof");
        const std::vector<double>* rd = d.get("read");
        if (!pv || pv->size() != 14 || !rd || rd->size() != (size_t)(3 * NQ)) return;
        Profile p;
        p.takes = (int)(*pv)[0]; p.activeSec = (*pv)[1]; p.onsets = (int)(*pv)[2]; p.f0min = (*pv)[3]; p.f0max = (*pv)[4];
        p.harmonic = (*pv)[5]; p.sustained = (*pv)[6]; p.corr = (*pv)[7]; p.sideDb = (*pv)[8]; p.loudness = (*pv)[9];
        p.lowestHz = (*pv)[10]; p.peakRef = (*pv)[11]; p.levelHi = (*pv)[12]; p.levelLo = (*pv)[13];
        for (int q = 0; q < NQ; q++) { p.reading[q] = (*rd)[(size_t)(3 * q)]; p.readValid[q] = (*rd)[(size_t)(3 * q + 1)] > 0.5; p.certainty[q] = (*rd)[(size_t)(3 * q + 2)]; }
        if (const std::vector<double>* o = d.get("oct6")) p.oct6 = *o;
        p.valid = true;
        prof = p;
        takes.clear();
        solveSet.reset();
        sol = Solution();
        const std::vector<double>* sv = d.get("sol");
        const std::vector<double>* sp = d.get("solplan");
        const size_t solN = NT + NM * NT + 2 * NM + 4 * NQ + 1;
        if (sv && sv->size() == solN && sp && sp->size() == (size_t)NP) {
            size_t k = 0;
            for (int t = 0; t < NT; t++) sol.tools[t] = (*sv)[k++];
            for (int m = 0; m < NM; m++) for (int t = 0; t < NT; t++) sol.J[m][t] = (*sv)[k++];
            for (int m = 0; m < NM; m++) { sol.M[m] = (*sv)[k++]; sol.Z[m] = (*sv)[k++]; }
            for (int q = 0; q < NQ; q++) { sol.zok[q] = (*sv)[k++] > 0.5; sol.units.up[q] = (*sv)[k++]; sol.units.dn[q] = (*sv)[k++]; sol.units.ok[q] = (*sv)[k++] > 0.5; }
            sol.inLoud = (*sv)[k++];
            for (int i = 0; i < NP; i++) sol.plan[i] = (*sp)[(size_t)i];
            sol.ok = true;
        }
        const std::vector<double>* pl = d.get("play");
        Inputs pin;
        if (pl && pl->size() == (size_t)NP && inputsFrom(d.get("playin"), pin)) {
            for (int i = 0; i < NP; i++) play[i] = (*pl)[(size_t)i];
            playInputs = pin;
            playSolo = (int)d.get1("playsolo", -1);
            playReach = d.get1("playreach", -1);
            if (const std::vector<double>* vr = d.get("varreach")) for (int v = 0; v < NVAR && v < (int)vr->size(); v++) varReach[v] = (*vr)[(size_t)v];
            // what showed with it (asked/reached/moves): one step from the solve with the inputs that made it
            if (sol.ok) shown = fastStep(sol, prof, playInputs, playSolo);
            shown.plan = play;
            shown.reach = playReach;
            sendPlanToAudio(play);   // exactly what was playing
            restoredPlay.reset(new Plan(play));
            restoredInputs = playInputs;
            restoredSolo = playSolo;
            restoredShown = shown;
        }
        // the parameters may already hold other values (or will arrive): the fast path takes it from here
        inputsChanged();
    }

    void dirty(double delayMs = 500) { if (saveAt < 0 || saveAt > nowMs + delayMs) saveAt = nowMs + delayMs; }

    void save() {
        StateDoc d;
        writeState(d);
        std::vector<double> v = encodeState(d);
        if (v == lastSaved) return;   // nothing changed: no new step in Live's undo history
        lastSaved = v;
        if (trace) host->log("lifegiver: [save] id " + std::to_string(deviceId) + ", " + std::to_string(v.size()) + " numbers");
        host->storeState(v);
    }

    // ---- the view ----------------------------------------------------------------------------------------------
    void sendTakesUi() {
        std::vector<double> v = {(double)takes.size()};
        for (int i = 0; i < MAX_TAKES; i++) v.push_back(i < (int)takes.size() ? takes[(size_t)i].size() / takes[(size_t)i].sr : 0);
        v.push_back((double)pr->capture.state.load());
        v.push_back(pr->capture.keptSec.load());
        v.push_back(busy ? 1 : 0);
        host->ui("takes", v);
    }

    void sendVarsUi() {
        std::vector<double> v(varReach, varReach + NVAR);
        v.push_back((double)solo);
        host->ui("vars", v);
    }

    void sendUi() {
        sendTakesUi();
        // facts: valid, takes heard, seconds, lowest/highest note (MIDI, 0 = none), harmonic 0..2, sustained 0..2,
        // stereo correlation, certainty dots 1..3, reach of the playing plan (−1 = none), stage level dB, compare
        double cert = 0;
        for (int q = 0; q < NQ; q++) cert += prof.certainty[q];
        cert /= NQ;
        auto midi = [](double f) { return f > 0 ? 69 + 12 * std::log2(f / 440.0) : 0.0; };
        host->ui("facts", {prof.valid ? 1.0 : 0.0, (double)prof.takes, prof.activeSec, midi(prof.f0min), midi(prof.f0max),
                           prof.harmonic > 0.6 ? 2.0 : (prof.harmonic > 0.3 ? 1.0 : 0.0),
                           prof.sustained > 0.66 ? 2.0 : (prof.sustained > 0.33 ? 1.0 : 0.0), prof.corr,
                           cert >= 0.8 ? 3.0 : (cert >= 0.5 ? 2.0 : 1.0), playReach, prof.valid ? stageLevelDb(in) : 0.0,
                           compareOn ? 1.0 : 0.0, (double)takes.size()});
        // knobs: per quality: word (0 = —, 1..4), dots 1..3, stage push (steps), adjustable, reached, asked
        double push[NQ];
        stagePush(in, push);
        std::vector<double> kv;
        for (int q = 0; q < NQ; q++) {
            const bool adj = prof.valid && (sol.ok ? sol.units.ok[q] : prof.readValid[q]);
            kv.push_back(prof.valid ? (double)readingWord(q, prof.reading[q], prof.readValid[q]) : 0.0);
            const double c = prof.certainty[q];
            kv.push_back(c >= 0.8 ? 3 : (c >= 0.5 ? 2 : 1));
            kv.push_back(push[q]);
            kv.push_back(adj ? 1 : 0);
            kv.push_back(shown.ok ? shown.reached[q] : 0);
            kv.push_back(shown.ok ? shown.asked[q] : 0);
        }
        host->ui("knobs", kv);
        // moves: kind, value, quality (−1), reached, asked — at most 12
        std::vector<double> mv;
        if (prof.valid && shown.ok) {
            Solution s = shown;
            s.plan = play;
            const std::vector<Move> ms = movesOf(s);
            for (size_t i = 0; i < ms.size() && i < 12; i++) {
                mv.push_back(ms[i].kind); mv.push_back(ms[i].value); mv.push_back(ms[i].q);
                mv.push_back(ms[i].reached); mv.push_back(ms[i].asked);
            }
        }
        host->ui("moves", mv);
        sendVarsUi();
    }
};

}  // namespace lg
