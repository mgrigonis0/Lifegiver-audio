// Lifegiver: the main thread. Takes the patch's messages, sets the audio thread's values, keeps the saved state.
// Phase 1 state: a format number, a device id made once per new device, and which engine build saved it, so a
// save → reopen test in Live shows the same id again ("lifegiver: state restored, id ...").
//
// Loading: init() → the host is asked for the saved state now, again at +300 ms and +800 ms. Answers can come from
// two stores (the hidden pattr and the hidden dict, see build_device.py); the first well-formed Lifegiver state wins.
// Nothing valid by +1.5 s = a new device, but its first save waits until +4.5 s, and a valid state that still arrives
// within 10 s (Live restoring late) replaces it. Until build 15c is confirmed in Live, every answer and every save
// in the first 10 s is written to the Max window (`trace`).
#pragma once
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "processor.h"
#include "state.h"
#include "version.h"

namespace lg {

constexpr int STATE_FORMAT = 1;
constexpr double LATE_RESTORE_MS = 10000;   // a valid state arriving this long after init still wins
constexpr double NEW_DEVICE_SAVE_MS = 3000; // a new device saves this long after deciding it is new

// What the controller asks of its host (the Max wrapper, or the test rig).
struct Host {
    virtual ~Host() {}
    virtual void requestState() = 0;                          // ask the set's stores (answers: stateFrom())
    virtual void storeState(const std::vector<double>&) = 0;  // put this list into every store
    virtual void log(const std::string&) {}                   // a line for the Max window
};

class Controller {
public:
    struct Atom { bool num = true; double d = 0; std::string s; };

    bool trace = true;   // load/save diagnostics in the Max window (first 10 s after init, and every save)

    Controller(Processor* p, Host* h) : pr(p), host(h) {}

    // The device finished loading (live.thisdevice).
    void init(double ms) {
        if (initAt >= 0) return;
        nowMs = ms;
        initAt = ms;
        asks = 1;
        host->requestState();
    }

    // A message from the patch: selector + arguments. A word where a number belongs counts as the default.
    void message(const std::string& m, const std::vector<Atom>& a) {
        auto n = [&](size_t i, double def) { return (i < a.size() && a[i].num) ? a[i].d : def; };
        if (m == "amount") {
            amountPct = clampd(n(0, 100), 0, 100);
            pr->setAmount(amountPct / 100.0);
            return;   // a Live parameter: Live saves it itself
        }
        if (m == "statelist") { stateFrom("pattr", a); return; }
        if (m == "makeup") { pr->setMakeupDb(clampd(n(0, 0), -24, 24)); return; }   // the solver will set this; tests use it
        host->log("lifegiver: unknown message \"" + m + "\"");
    }

    // An answer from one of the set's stores ("pattr" or "dict").
    void stateFrom(const std::string& src, const std::vector<Atom>& a) {
        std::vector<double> v;
        bool words = false;
        for (auto& x : a) { if (!x.num) { words = true; break; } v.push_back(x.d); }
        StateDoc d;
        const bool valid = !words && decodeState(v, d) && (int)d.get1("v", 0) >= 1 && d.get("id");
        const bool own = valid && v == lastSaved;   // our own save coming back
        if (tracing()) host->log("lifegiver: [load] " + src + " answered at " + std::to_string((long)(nowMs - initAt)) +
                                 " ms: " + describe(a, words, valid, d) + (own ? " (our own save)" : ""));
        latest[src] = words ? std::vector<double>{NAN} : v;   // each store's latest answer (NaN marks words)
        if (own) return;
        if (!restored) {
            if (!valid) return;
            readState(d, src);
            // a store that answered differently before (empty, stale, broken): save again so all hold the same
            for (auto& kv : latest) if (kv.second != v) { resync(); break; }
            return;
        }
        if (!byFallback && nowMs - initAt <= LATE_RESTORE_MS) {
            // restored, and another store disagrees (empty, stale, broken): save again so both hold the same
            resync();
            return;
        }
        // a valid, different state after we had decided "new device": Live restored late; the set's state wins,
        // and is written back so the other store stops holding the new id
        if (valid && byFallback && nowMs - initAt <= LATE_RESTORE_MS) {
            host->log("lifegiver: the set's state arrived late; using it instead of the new id");
            readState(d, src);
            resync();
        }
    }

    // Called every ~15 ms on the main thread.
    void tick(double ms) {
        nowMs = ms;
        Report r;
        while (pr->reps.pop(r)) {}
        if (!restored && initAt >= 0) {
            if (asks == 1 && ms >= initAt + 300) { asks = 2; host->requestState(); }
            if (asks == 2 && ms >= initAt + 800) { asks = 3; host->requestState(); }
            if (ms >= initAt + 1500) noState();
        }
        if (saveAt >= 0 && ms >= saveAt && restored) { saveAt = -1; save(); }
    }

    // A well-formed state from the set.
    void readState(const StateDoc& d, const std::string& src = "set") {
        restored = true;
        byFallback = false;
        saveAt = -1;
        deviceId = (uint32_t)d.get1("id", 0);
        savedBy = d.gets("saved_by");
        host->log("lifegiver: state restored from the " + src + ", id " + std::to_string(deviceId) + " (saved by engine " +
                  (savedBy.empty() ? std::string("?") : savedBy) + ")");
        lastSaved = encodeState(d);
        if ((int)d.get1("v", 0) != STATE_FORMAT || savedBy != VERSION) dirty();   // re-save in this build's format
    }

    // Nothing valid came from the set: a new device.
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
    }

    uint32_t id() const { return deviceId; }
    bool isRestored() const { return restored; }

private:
    Processor* pr;
    Host* host;
    double amountPct = 100;
    uint32_t deviceId = 0;
    std::string savedBy;
    bool restored = false;     // nothing is saved before the set's own state had its chance to load
    bool byFallback = false;   // "new device" was decided by the timeout (a late real state may still replace it)
    double nowMs = 0, saveAt = -1, initAt = -1;
    int asks = 0;
    std::vector<double> lastSaved;
    std::map<std::string, std::vector<double>> latest;   // each store's latest answer during this load

    static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
    void dirty(double delayMs = 500) { if (saveAt < 0) saveAt = nowMs + delayMs; }
    void resync() { lastSaved.clear(); dirty(); }   // the next save happens even though nothing changed
    bool tracing() const { return trace && initAt >= 0 && nowMs - initAt <= LATE_RESTORE_MS; }

    static std::string describe(const std::vector<Atom>& a, bool words, bool valid, const StateDoc& d) {
        if (a.empty()) return "nothing";
        if (words) return std::to_string(a.size()) + " item(s), first word \"" + (a[0].num ? std::string("<number>") : a[0].s) + "\"";
        if (valid) return "a Lifegiver state, id " + std::to_string((long)d.get1("id", 0)) + ", " + std::to_string(a.size()) + " numbers";
        std::string s = std::to_string(a.size()) + " number(s), not a Lifegiver state (first:";
        for (size_t i = 0; i < a.size() && i < 4; i++) s += " " + std::to_string(a[i].d);
        return s + ")";
    }

    void save() {
        StateDoc d;
        writeState(d);
        std::vector<double> v = encodeState(d);
        if (v == lastSaved) return;   // nothing changed: no new step in Live's undo history
        lastSaved = v;
        if (trace) host->log("lifegiver: [save] id " + std::to_string(deviceId) + ", " + std::to_string(v.size()) + " numbers");
        host->storeState(v);
    }
};

}  // namespace lg
