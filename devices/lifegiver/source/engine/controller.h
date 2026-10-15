// Lifegiver: the main thread. Takes the patch's messages, sets the audio thread's values, keeps the saved state.
// Phase 1 state: a format number, a device id made once per new device, and which engine build saved it, so a
// save → reopen test in Live shows the same id again ("lifegiver: state restored, id ...").
//
// Loading: init() → the host is asked for the saved state now, again at +300 ms and +800 ms (Live may restore the
// hidden parameter late); the first well-formed Lifegiver state wins. Nothing valid by +1.5 s = a new device.
// Nothing is saved before that is settled, so a slow load can never overwrite the set's state.
#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "processor.h"
#include "state.h"
#include "version.h"

namespace lg {

constexpr int STATE_FORMAT = 1;

// What the controller asks of its host (the Max wrapper, or the test rig).
struct Host {
    virtual ~Host() {}
    virtual void requestState() = 0;                          // ask for the set's saved list (answer: "statelist ...")
    virtual void storeState(const std::vector<double>&) = 0;  // put this list into the set
    virtual void log(const std::string&) {}                   // a line for the Max window
};

class Controller {
public:
    struct Atom { bool num = true; double d = 0; std::string s; };

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
        if (m == "statelist") { gotList(a); return; }
        if (m == "makeup") { pr->setMakeupDb(clampd(n(0, 0), -24, 24)); return; }   // the solver will set this; tests use it
        host->log("lifegiver: unknown message \"" + m + "\"");
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
    void readState(const StateDoc& d) {
        restored = true;
        deviceId = (uint32_t)d.get1("id", 0);
        savedBy = d.gets("saved_by");
        host->log("lifegiver: state restored, id " + std::to_string(deviceId) + " (saved by engine " +
                  (savedBy.empty() ? std::string("?") : savedBy) + ")");
        lastSaved = encodeState(d);
        if ((int)d.get1("v", 0) != STATE_FORMAT || savedBy != VERSION) dirty();   // re-save in this build's format
    }

    // Nothing valid came from the set: a new device.
    void noState() {
        restored = true;
        std::random_device rd;
        deviceId = 1000 + rd() % 9000;
        savedBy.clear();
        host->log("lifegiver: new device, id " + std::to_string(deviceId));
        dirty();
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
    double nowMs = 0, saveAt = -1, initAt = -1;
    int asks = 0;
    std::vector<double> lastSaved;

    static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
    void dirty(double delayMs = 500) { if (saveAt < 0) saveAt = nowMs + delayMs; }

    void gotList(const std::vector<Atom>& a) {
        if (restored) return;   // settled already (also: our own save echoing back from the patch)
        std::vector<double> v;
        v.reserve(a.size());
        for (auto& x : a) { if (!x.num) return; v.push_back(x.d); }   // a word: not ours (an empty or older value)
        StateDoc d;
        if (!decodeState(v, d) || (int)d.get1("v", 0) < 1 || !d.get("id")) return;
        readState(d);
    }

    void save() {
        StateDoc d;
        writeState(d);
        std::vector<double> v = encodeState(d);
        if (v == lastSaved) return;   // nothing changed: no new step in Live's undo history
        lastSaved = v;
        host->storeState(v);
    }
};

}  // namespace lg
