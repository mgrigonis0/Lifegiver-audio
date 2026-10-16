// Lifegiver: Listen's capture, on the audio thread (DESIGN §3).
//
// Arm → the capture starts at the first sound (or at once if audio is already running) and keeps ~4 s of sound:
// silence is skipped, but a 400 ms hold after anything audible keeps decays (Tail and Steady would read wrong
// without them). It stops by itself after 4 s of sound, 12 s of wall time, or a full buffer.
// The buffer is allocated once, when the object is made (main thread): the audio thread never allocates.
// States (atomic): IDLE → ARMED (main) → CAPTURING (audio) → DONE (audio) → IDLE (main, after copying the take).
#pragma once
#include <atomic>
#include <cmath>
#include <vector>

namespace lg {

class Capture {
public:
    enum State { IDLE, ARMED, CAPTURING, DONE };
    static constexpr long FRAMES = 1L << 19;   // 10.9 s at 48 kHz, 5.5 s at 96 kHz
    static constexpr double TARGET_SEC = 4.0, WALL_SEC = 12.0, HOLD_SEC = 0.4;
    static constexpr int CHUNK = 64;

    std::atomic<int> state{IDLE};
    std::atomic<double> keptSec{0.0};   // for the UI

    Capture() : l((size_t)FRAMES, 0.0f), r((size_t)FRAMES, 0.0f) {}

    // main thread
    bool arm() { int s = IDLE; return state.compare_exchange_strong(s, ARMED); }
    void cancel() {
        int s = ARMED;
        if (state.compare_exchange_strong(s, IDLE)) return;
        s = CAPTURING;
        state.compare_exchange_strong(s, IDLE);
    }
    long frames() const { return kept; }   // valid in DONE
    const float* left() const { return l.data(); }
    const float* right() const { return r.data(); }
    void release() { int s = DONE; state.compare_exchange_strong(s, IDLE); }

    // audio thread, every block
    void process(const double* L, const double* R, long n, double sr) {
        const int st = state.load(std::memory_order_acquire);
        if (st != ARMED && st != CAPTURING) return;
        if (st == ARMED) {
            // wait for the first sound (−50 dBFS)
            long first = -1;
            for (long i = 0; i < n; i++) if (std::fabs(L[i]) > 0.00316 || std::fabs(R[i]) > 0.00316) { first = i; break; }
            if (first < 0) return;
            int s = ARMED;
            if (!state.compare_exchange_strong(s, CAPTURING)) return;
            kept = 0; wall = 0; runPeak = 0; holdLeft = 0; keptSec.store(0.0);
            L += first; R += first; n -= first;
        }
        const long target = (long)(TARGET_SEC * sr), wallMax = (long)(WALL_SEC * sr), hold = (long)(HOLD_SEC * sr);
        for (long o = 0; o < n; o += CHUNK) {
            const long m = (n - o) < CHUNK ? (n - o) : CHUNK;
            double pk = 0;
            for (long i = o; i < o + m; i++) { pk = std::fmax(pk, std::fabs(L[i])); pk = std::fmax(pk, std::fabs(R[i])); }
            runPeak = std::fmax(runPeak, pk);
            const bool sound = pk > 1e-3 * 5.6 * runPeak && pk > 3.16e-4;   // −45 dB re the take's peak, −70 dBFS
            if (sound) holdLeft = hold;
            if (sound || holdLeft > 0) {
                for (long i = o; i < o + m && kept < FRAMES; i++) { l[(size_t)kept] = (float)L[i]; r[(size_t)kept] = (float)R[i]; kept++; }
            }
            holdLeft -= m;
            wall += m;
            if (kept >= target || kept >= FRAMES || wall >= wallMax) {
                keptSec.store(kept / sr);
                int s = CAPTURING;
                state.compare_exchange_strong(s, DONE);
                return;
            }
        }
        keptSec.store(kept / sr);
    }

private:
    std::vector<float> l, r;
    long kept = 0, wall = 0, holdLeft = 0;
    double runPeak = 0;
};

}  // namespace lg
