// Lifegiver: the audio thread. Phase 1: the processing chain is only the make-up gain stage (unity), so the device
// passes audio through. Amount blends dry and processed; at 0 % the output is the input, bit for bit.
//
// Thread rules (CONVENTIONS): process() never allocates, locks or frees. The engine works on the host's 64-bit
// buffers directly: no work buffers needed.
// Two ways in from the main thread:
//   - continuous values (Amount, make-up): atomics, latest value wins, read once per block. A queue could fill up
//     while audio is stopped and drop the newest value; an atomic never loses it.
//   - events (later: plans handed over by pointer): the `cmds` ring; anything to free comes back through `reps`.
#pragma once
#include <atomic>

#include "ring.h"
#include "smooth.h"

namespace lg {

enum CmdType { C_NONE };
struct Cmd { int type = C_NONE; double a = 0; void* ptr = nullptr; };

enum RepType { R_NONE };
struct Report { int type = R_NONE; double a = 0; void* ptr = nullptr; };

static_assert(std::atomic<double>::is_always_lock_free, "the audio thread needs lock-free atomics");

constexpr double SMOOTH_TAU = 0.004;   // 4 ms per stage: a step settles in ≈ 25 ms, S-shaped

class Processor {
public:
    Ring<Cmd, 256> cmds;        // main -> audio (events; unused in Phase 1)
    Ring<Report, 256> reps;     // audio -> main (unused in Phase 1)

    Processor() { amount.reset(1.0); makeup.reset(1.0); }

    // Main thread → audio thread, any time, any rate.
    void setAmount(double a01) { amountTarget.store(a01 < 0 ? 0 : (a01 > 1 ? 1 : a01), std::memory_order_relaxed); }
    void setMakeupDb(double db) { makeupTarget.store(dbToGain(db), std::memory_order_relaxed); }

    // Called by the host before audio starts and whenever the sample rate changes (main thread, audio stopped).
    void prepare(double sampleRate) {
        sr = sampleRate > 0 ? sampleRate : 44100.0;
        running = false;   // audio (re)starts: the current values apply at once
        amount.setTime(SMOOTH_TAU, sr);
        makeup.setTime(SMOOTH_TAU, sr);
    }

    // inL/inR may be the same memory as outL/outR: every sample is read before it is written.
    void process(const double* inL, const double* inR, double* outL, double* outR, long n) {
        drainCommands();
        const double at = amountTarget.load(std::memory_order_relaxed), mt = makeupTarget.load(std::memory_order_relaxed);
        if (!running) {
            // values that arrived before audio started (Live restoring the set) apply at once: no glide on load
            amount.reset(at); makeup.reset(mt);
            running = true;
        }
        amount.set(at);
        makeup.set(mt);
        for (long i = 0; i < n; i++) {
            const double dl = inL[i], dr = inR[i];
            const double a = amount.next();
            const double g = makeup.next();
            if (a <= 0.0) { outL[i] = dl; outR[i] = dr; continue; }   // 0 %: exactly the input
            const double wl = dl * g, wr = dr * g;                      // the chain (Phase 1: make-up only)
            if (a >= 1.0) { outL[i] = wl; outR[i] = wr; continue; }
            outL[i] = dl + a * (wl - dl);
            outR[i] = dr + a * (wr - dr);
        }
    }

    double sampleRate() const { return sr; }

private:
    double sr = 44100.0;
    bool running = false;      // false until the first block: the first values jump, later ones glide
    std::atomic<double> amountTarget{1.0}, makeupTarget{1.0};
    Smoother amount, makeup;

    void drainCommands() {
        Cmd c;
        while (cmds.pop(c)) {}
    }
};

}  // namespace lg
