// Test harness: the real engine (processor + controller) with a fake host that keeps the saved state, synthetic
// signals, and the measures the tests share. Time advances in blocks; the controller ticks every 15 ms.
#pragma once
// The allocation counter below replaces operator new/delete with malloc/free (a matched pair); GCC's
// -Wmismatched-new-delete can't see that and warns at every inlined container deallocation.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "../../source/engine/controller.h"

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

// ---- the fake host --------------------------------------------------------------------------------
// It plays [pattr state]: `stored` is what the set holds (Live restores it at `readyAt` ms; before that the pattr
// still holds its initial 0). requestState() answers like the patch: "statelist <the pattr's value>".
struct TestHost : Host {
    std::vector<Controller::Atom> stored = {Controller::Atom()};   // a new device's pattr: a single 0
    std::vector<Controller::Atom> beforeReady = {Controller::Atom()};
    double readyAt = 0, now = 0;
    int saves = 0, requests = 0;
    bool echo = true;                    // pattr passes a stored value straight back out (assume the worst)
    std::vector<std::string> logs;
    Controller* ctl = nullptr;
    void requestState() override {
        requests++;
        ctl->message("statelist", now >= readyAt ? stored : beforeReady);
    }
    void storeState(const std::vector<double>& v) override {
        saves++;
        stored.clear();
        for (double d : v) { Controller::Atom a; a.d = d; stored.push_back(a); }
        if (echo) ctl->message("statelist", stored);
    }
    void log(const std::string& t) override { logs.push_back(t); }
    bool logged(const std::string& part) const { for (auto& l : logs) if (l.find(part) != std::string::npos) return true; return false; }
    // what the set holds, decoded
    bool storedDoc(StateDoc& d) const {
        std::vector<double> v;
        for (auto& a : stored) { if (!a.num) return false; v.push_back(a.d); }
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
    void tick() { host.now = ms; if (ms - lastTick >= 15) { lastTick = ms; ctl->tick(ms); } }
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

}  // namespace test
