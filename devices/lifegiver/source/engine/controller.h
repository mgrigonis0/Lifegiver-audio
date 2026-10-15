// Lifegiver: the main thread. Takes the patch's messages, sends commands to the audio thread, keeps the saved state.
// Phase 1 state: a format number, a device id made once per new device, and which engine build saved it, so a
// save → reopen test in Live shows the same id again ("lifegiver: state restored, id ...").
#pragma once
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "processor.h"
#include "version.h"

namespace lg {

constexpr int STATE_FORMAT = 1;

// Flat saved state: numbers and text by key (stored by the host in a dict inside the Live set).
struct StateDoc {
    std::map<std::string, std::vector<double>> num;
    std::map<std::string, std::string> str;
    const std::vector<double>* get(const std::string& k) const { auto i = num.find(k); return i == num.end() ? nullptr : &i->second; }
    double get1(const std::string& k, double def) const { auto v = get(k); return (v && !v->empty()) ? (*v)[0] : def; }
    std::string gets(const std::string& k) const { auto i = str.find(k); return i == str.end() ? std::string() : i->second; }
};

// What the controller asks of its host (the Max wrapper, or the test rig).
struct Host {
    virtual ~Host() {}
    virtual void saveState() = 0;                  // the host calls writeState() and stores the result
    virtual void log(const std::string&) {}        // a line for the Max window
};

class Controller {
public:
    struct Atom { bool num = true; double d = 0; std::string s; };

    Controller(Processor* p, Host* h) : pr(p), host(h) {}

    // A message from the patch: selector + arguments. A word where a number belongs counts as the default.
    void message(const std::string& m, const std::vector<Atom>& a) {
        auto n = [&](size_t i, double def) { return (i < a.size() && a[i].num) ? a[i].d : def; };
        if (m == "amount") {
            amountPct = clampd(n(0, 100), 0, 100);
            pr->setAmount(amountPct / 100.0);
            return;   // a Live parameter: Live saves it itself, so no dirty()
        }
        if (m == "makeup") { pr->setMakeupDb(clampd(n(0, 0), -24, 24)); return; }   // the solver will set this; a message for tests
        host->log("lifegiver: unknown message \"" + m + "\"");
    }

    // Called every ~15 ms on the main thread.
    void tick(double ms) {
        nowMs = ms;
        Report r;
        while (pr->reps.pop(r)) {}
        if (saveAt >= 0 && ms >= saveAt && restored) { saveAt = -1; host->saveState(); }
    }

    // The set had saved state for this device.
    void readState(const StateDoc& d) {
        restored = true;
        const int fmt = (int)d.get1("v", 0);
        if (fmt < 1 || !d.get("id")) { noState(); return; }
        deviceId = (uint32_t)d.get1("id", 0);
        savedBy = d.gets("saved_by");
        host->log("lifegiver: state restored, id " + std::to_string(deviceId) + " (saved by engine " +
                  (savedBy.empty() ? std::string("?") : savedBy) + ")");
        if (fmt != STATE_FORMAT || savedBy != VERSION) dirty();   // re-save in this build's format
    }

    // Nothing came from the set: a new device.
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
    double nowMs = 0, saveAt = -1;

    static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
    void dirty(double delayMs = 500) { if (saveAt < 0) saveAt = nowMs + delayMs; }
};

}  // namespace lg
