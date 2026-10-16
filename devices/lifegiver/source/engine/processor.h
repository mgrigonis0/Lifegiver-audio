// Lifegiver: the audio thread. Runs Listen's capture on the input and the processing chain on the plan the main
// thread hands over (with Scale and the Average mode), then blends: Amount (0 % = the input, bit for bit) and Compare (the input at the plan's intended
// stage level, so before/after is loudness-matched).
//
// Thread rules (CONVENTIONS): process() never allocates, locks or frees.
//   - continuous values (Amount, Compare, its level): atomics, latest value wins, read once per block;
//   - a new plan: by pointer through `cmds`; the old one goes back through `reps` and is freed on the main thread.
// Values that arrive before audio starts (a set loading) apply at once; later changes glide.
#pragma once
#include <atomic>

#include "capture.h"
#include "chain.h"
#include "ring.h"
#include "smooth.h"

namespace lg {

enum CmdType { C_NONE, C_PLAN };
struct Cmd { int type = C_NONE; double a = 0; void* ptr = nullptr; };

enum RepType { R_NONE, R_GARBAGE_PLAN };
struct Report { int type = R_NONE; double a = 0; void* ptr = nullptr; };

static_assert(std::atomic<double>::is_always_lock_free, "the audio thread needs lock-free atomics");

constexpr double SMOOTH_TAU = 0.004;    // Amount: 4 ms per stage, S-shaped
constexpr double COMPARE_TAU = 0.006;   // Compare: a quick S-shaped crossfade
constexpr int SUB = 64;                 // internal block (stack buffers: no allocation)

class Processor {
public:
    Ring<Cmd, 64> cmds;         // main -> audio
    Ring<Report, 128> reps;     // audio -> main
    Capture capture;            // Listen

    Processor() { amount.reset(1.0); cmp.reset(0.0); cmpGain.reset(1.0); chain.reset(Plan(), sr); }
    ~Processor() { delete plan; }

    // Main thread → audio thread, any time, any rate.
    void setAmount(double a01) { amountTarget.store(a01 < 0 ? 0 : (a01 > 1 ? 1 : a01), std::memory_order_relaxed); }
    // Scale (0 … 4, 1 = the plan) and the Average mode: read by the chain once per block (it glides them itself)
    void setScale(double s) { scaleTarget.store(s < 0 ? 0 : (s > SCALE_MAX ? SCALE_MAX : s), std::memory_order_relaxed); }
    void setAverage(bool on) { averageTarget.store(on, std::memory_order_relaxed); }
    void setCompare(bool on, double levelDb) {
        compareTarget.store(on ? 1.0 : 0.0, std::memory_order_relaxed);
        compareLevel.store(dbToLin(levelDb), std::memory_order_relaxed);
    }
    // a new plan (allocated by the main thread; the processor owns it from here)
    bool sendPlan(Plan* p) {
        Cmd c;
        c.type = C_PLAN; c.ptr = p;
        return cmds.push(c);
    }

    // Called by the host before audio starts and whenever the sample rate changes (main thread, audio stopped).
    void prepare(double sampleRate) {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        running = false;   // audio (re)starts: the current values apply at once
        amount.setTime(SMOOTH_TAU, sr);
        cmp.setTime(COMPARE_TAU, sr);
        cmpGain.setTime(SMOOTH_TAU, sr);
        chain.setScale(scaleTarget.load()); chain.setAverage(averageTarget.load());
        chain.reset(plan ? *plan : Plan(), sr);
    }

    // inL/inR may be the same memory as outL/outR: every sample is read before it is written.
    void process(const double* inL, const double* inR, double* outL, double* outR, long n) {
        const bool first = !running;
        drainCommands(first);
        const double at = amountTarget.load(std::memory_order_relaxed);
        const double ct = compareTarget.load(std::memory_order_relaxed), cg = compareLevel.load(std::memory_order_relaxed);
        if (first) {
            amount.reset(at); cmp.reset(ct); cmpGain.reset(cg);
            running = true;
        }
        amount.set(at); cmp.set(ct); cmpGain.set(cg);
        chain.setScale(scaleTarget.load(std::memory_order_relaxed));
        chain.setAverage(averageTarget.load(std::memory_order_relaxed));
        capture.process(inL, inR, n, sr);
        double dl[SUB], dr[SUB], wl[SUB], wr[SUB];
        for (long o = 0; o < n; o += SUB) {
            const long m = (n - o) < SUB ? (n - o) : SUB;
            for (long i = 0; i < m; i++) { dl[i] = inL[o + i]; dr[i] = inR[o + i]; }
            chain.process(dl, dr, wl, wr, m);
            for (long i = 0; i < m; i++) {
                const double a = amount.next(), c = cmp.next(), g = cmpGain.next();
                double L, R;
                if (a <= 0.0) { L = dl[i]; R = dr[i]; }                // 0 %: exactly the input
                else if (a >= 1.0) { L = wl[i]; R = wr[i]; }
                else { L = dl[i] + a * (wl[i] - dl[i]); R = dr[i] + a * (wr[i] - dr[i]); }
                if (c > 0.0) {                                         // Compare: the input at the intended level
                    const double bl = dl[i] * g, br = dr[i] * g;
                    if (c >= 1.0) { L = bl; R = br; }
                    else { L += c * (bl - L); R += c * (br - R); }
                }
                outL[o + i] = L;
                outR[o + i] = R;
            }
        }
    }

    double sampleRate() const { return sr; }
    const Plan* currentPlan() const { return plan; }   // main thread may read only while audio is stopped (tests)

private:
    double sr = 44100.0;
    bool running = false;
    std::atomic<double> amountTarget{1.0}, compareTarget{0.0}, compareLevel{1.0}, scaleTarget{1.0};
    std::atomic<bool> averageTarget{false};
    Smoother amount, cmp, cmpGain;
    Chain chain;
    Plan* plan = nullptr;

    void drainCommands(bool first) {
        Cmd c;
        while (cmds.pop(c)) {
            if (c.type == C_PLAN && c.ptr) {
                Plan* p = (Plan*)c.ptr;
                if (first) { chain.setScale(scaleTarget.load()); chain.setAverage(averageTarget.load()); chain.reset(*p, sr); }
                else chain.setTarget(*p);
                if (plan) {
                    Report r;
                    r.type = R_GARBAGE_PLAN; r.ptr = plan;
                    if (!reps.push(r)) { /* ring full: keep it (freed with the processor) — never free here */ keep(plan); }
                }
                plan = p;
            }
        }
    }
    // a plan that couldn't be handed back (never in practice: the ring is far larger than the plan rate)
    Plan* kept[8] = {nullptr};
    void keep(Plan* p) { for (auto& k : kept) if (!k) { k = p; return; } }
public:
    void freeKept() { for (auto& k : kept) { delete k; k = nullptr; } }   // main thread, audio stopped
};

}  // namespace lg
