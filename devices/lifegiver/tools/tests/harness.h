// Test harness: the real engine (processor + controller) with a fake host that plays the patch: the hidden [pattr]
// store, the worker thread (jobs run at the next tick, or held back on request), the view (keeps the last message
// of each kind) and Live parameters set by the engine. Time advances in blocks; the controller ticks every 15 ms.
#pragma once
// The allocation counter below replaces operator new/delete with malloc/free (a matched pair); GCC's
// -Wmismatched-new-delete can't see that and warns at every inlined container deallocation.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "../../source/engine/controller.h"
#include "signals.h"

// ---- allocation counter: any allocation while `g_watchAlloc` is on is counted (the audio thread must never allocate)
inline std::atomic<bool> g_watchAlloc{false};
inline std::atomic<long> g_allocs{0};
void* operator new(std::size_t n) {
    if (g_watchAlloc.load(std::memory_order_relaxed)) g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace test {
using namespace lg;

struct Stereo { std::vector<double> l, r; size_t size() const { return l.size(); } };

inline uint32_t lcg(uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
inline double noise(uint32_t& s) { return (lcg(s) >> 8) / double(1 << 24) * 2 - 1; }

// full-scale-ish stereo noise with a few exact values that break naive arithmetic (0, -0, tiny, near 1)
inline Stereo noiseSig(double sec, double sr, uint32_t seed = 11) {
    Stereo s; const size_t n = (size_t)(sec * sr);
    s.l.resize(n); s.r.resize(n);
    for (size_t i = 0; i < n; i++) { s.l[i] = 0.9 * noise(seed); s.r[i] = 0.9 * noise(seed); }
    if (n > 8) { s.l[1] = 0.0; s.l[2] = -0.0; s.l[3] = 1e-300; s.r[4] = 0.999999999; s.r[5] = -1.0; }
    return s;
}

inline Stereo sineSig(double f, double sec, double sr, double amp = 0.5) {
    Stereo s; const size_t n = (size_t)(sec * sr);
    s.l.resize(n); s.r.resize(n);
    for (size_t i = 0; i < n; i++) { s.l[i] = amp * std::sin(2 * M_PI * f * i / sr); s.r[i] = amp * std::sin(2 * M_PI * f * 1.5 * i / sr); }
    return s;
}

inline Stereo fromAudio(const Audio& a) {
    Stereo s;
    s.l.assign(a.l.begin(), a.l.end());
    s.r.assign(a.r.begin(), a.r.end());
    return s;
}

// ---- the fake host --------------------------------------------------------------------------------
struct TestHost : Host {
    // the hidden [pattr state]: a new device's pattr holds a single 0 (it never had a value stored)
    std::vector<Controller::Atom> pattr = {Controller::Atom()};
    bool pattrWorks = true;
    double now = 0;
    int saves = 0, requests = 0;
    bool echo = true;                    // pattr passes a stored value straight back out (assume the worst)
    bool holdJobs = false;               // keep worker jobs waiting (tests of results arriving late)
    std::vector<std::string> logs;
    std::map<std::string, std::vector<double>> uiLast;
    std::map<std::string, int> uiCount;
    std::map<std::string, double> params;
    std::deque<std::pair<std::function<void()>, std::function<void()>>> jobs;
    Controller* ctl = nullptr;
    std::vector<uint32_t> savedIds;

    void requestState() override { requests++; ctl->message("statelist", pattr); }
    void storeState(const std::vector<double>& v) override {
        saves++;
        StateDoc d;
        if (decodeState(v, d)) savedIds.push_back((uint32_t)d.get1("id", 0));
        if (pattrWorks) {
            pattr.clear();
            for (double x : v) { Controller::Atom a; a.d = x; pattr.push_back(a); }
            if (echo) ctl->message("statelist", pattr);
        }
    }
    void log(const std::string& t) override { logs.push_back(t); }
    void ui(const std::string& sel, const std::vector<double>& v) override { uiLast[sel] = v; uiCount[sel]++; }
    void param(const std::string& name, double v) override { params[name] = v; }
    void post(std::function<void()> work, std::function<void()> done) override { jobs.emplace_back(std::move(work), std::move(done)); }
    void runJobs() {
        while (!holdJobs && !jobs.empty()) {
            auto j = std::move(jobs.front());
            jobs.pop_front();
            j.first();
            j.second();
        }
    }
    bool logged(const std::string& part) const { for (auto& l : logs) if (l.find(part) != std::string::npos) return true; return false; }
    static bool doc(const std::vector<Controller::Atom>& held, StateDoc& d) {
        std::vector<double> v;
        for (auto& a : held) { if (!a.num) return false; v.push_back(a.d); }
        return decodeState(v, d);
    }
};

struct Rig {
    std::unique_ptr<Processor> pr{new Processor()};
    TestHost host;
    std::unique_ptr<Controller> ctl;
    double sr;
    double ms = 0, lastTick = -1e9;
    long sample = 0;

    explicit Rig(double sampleRate = 48000) : sr(sampleRate) {
        pr->prepare(sr);
        ctl.reset(new Controller(pr.get(), &host));
        host.ctl = ctl.get();
    }
    void msg(const std::string& sel, std::vector<double> nums = {}) {
        std::vector<Controller::Atom> a;
        for (double d : nums) { Controller::Atom x; x.d = d; a.push_back(x); }
        ctl->message(sel, a);
    }
    void tick() { host.now = ms; if (ms - lastTick >= 15) { lastTick = ms; host.runJobs(); ctl->tick(ms); } }
    void init() { host.now = ms; ctl->init(ms); }
    // process `in` from `from` for `n` samples in blocks of `block`, appending to `out`
    void run(const Stereo& in, size_t from, size_t n, Stereo& out, long block = 64) {
        const size_t o = out.size();
        out.l.resize(o + n); out.r.resize(o + n);
        for (size_t done = 0; done < n; done += (size_t)block) {
            const long b = (long)std::min<size_t>((size_t)block, n - done);
            pr->process(in.l.data() + from + done, in.r.data() + from + done, out.l.data() + o + done, out.r.data() + o + done, b);
            sample += b;
            ms = sample / sr * 1000;
            tick();
        }
    }
    // silence for `sec` (lets timers run)
    void idle(double sec) {
        Stereo z; z.l.assign((size_t)(sec * sr), 0.0); z.r = z.l;
        Stereo o;
        run(z, 0, z.size(), o);
    }
    // play a sound with Arm pressed (Listen), then let the worker finish
    void listen(const Audio& a) {
        msg("arm");
        Stereo s = fromAudio(a), o;
        run(s, 0, s.size(), o);
        idle(0.5);
    }
};

// ---- measures --------------------------------------------------------------------------------------
// clicks: a jump in the second difference far above what the signal's own content allows (wanderer-synth's measure)
struct ClickStats { long count = 0; double worst = 0; long worstAt = -1; };
inline ClickStats findClicks(const std::vector<double>& x, double ratio = 0.02, double absFloor = 2e-3) {
    ClickStats cs;
    const size_t n = x.size();
    std::vector<double> ahead(n, 0.0);
    double pa = 0;
    for (size_t i = n; i-- > 0;) { pa = std::max(pa * 0.9995, std::fabs(x[i])); ahead[i] = pa; }
    double pk = 0;
    for (size_t i = 2; i < n; i++) {
        pk = std::max(pk * 0.9995, std::fabs(x[i]));
        const double d2 = std::fabs(x[i] - 2 * x[i - 1] + x[i - 2]);
        const double lim = std::max(absFloor, ratio * std::max(std::max(pk, ahead[i]), 0.05));
        if (d2 > lim) { cs.count++; if (d2 / lim > cs.worst) { cs.worst = d2 / lim; cs.worstAt = (long)i; } }
    }
    return cs;
}

inline bool bitEqual(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }

inline int expect(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    return ok ? 0 : 1;
}

// a plan with every module working (for safety, click and CPU tests)
inline Plan busyPlan() {
    Plan p;
    p[P_LOWCUT_ON] = 1; p[P_LOWCUT_HZ] = 80;
    p[P_WEIGHT_DB] = 3; p[P_WARM_DB] = -2; p[P_BITE_DB] = 4; p[P_AIR_DB] = 3; p[P_TILT_DB] = -1.5;
    p[P_SAT_DRIVE] = 0.4; p[P_SAT_REF] = 0.3;
    p[P_COMP_AMT] = 0.7; p[P_COMP_THR] = -20; p[P_EXP_AMT] = 0.3; p[P_EXP_THR] = -45;
    p[P_TS_ATTACK] = 0.5; p[P_TS_SUSTAIN] = -0.4;
    p[P_HIGHCUT_ON] = 1; p[P_HIGHCUT_HZ] = 12000;
    p[P_WIDTH] = 1.4; p[P_PAN] = 0.3;
    p[P_MAKEUP_DB] = 2; p[P_STAGE_DB] = -1.5;
    // 16c: every dynamic module, the second saturation and the ambience
    p[P_SAT2_DRIVE] = 0.3;
    p[P_DWARM_AMT] = 0.5; p[P_DBITE_AMT] = 0.5; p[P_DWARM_THR] = -8; p[P_DBITE_THR] = -16;
    p[P_SMOOTH_AMT] = 0.5;
    p[P_MB_LO] = 0.5; p[P_MB_MID] = 0.5; p[P_MB_HI] = 0.5; p[P_MB_THR1] = -24; p[P_MB_THR2] = -28; p[P_MB_THR3] = -32;
    p[P_RES_DEPTH] = 0.7; p[P_RES_HZ1] = 700; p[P_RES_HZ2] = 1500; p[P_RES_HZ3] = 3100; p[P_RES_HZ4] = 6200;
    p[P_AMB_MIX] = 0.2; p[P_AMB_SIZE] = 0.9;
    p[P_AVG_DWARM] = -2; p[P_AVG_DBITE] = -3; p[P_AVG_SMOOTH] = -1; p[P_AVG_MB1] = -3; p[P_AVG_MB2] = -3; p[P_AVG_MB3] = -3;
    p[P_AVG_RES1] = -4; p[P_AVG_RES2] = -4; p[P_AVG_RES3] = -4; p[P_AVG_RES4] = -4;
    return p;
}

}  // namespace test
