// Lifegiver: the lock-free hand-over between threads (same design as wanderer-synth's Ring).
#pragma once
#include <atomic>
#include <mutex>

namespace lg {

// Single-producer single-consumer ring. A producer that may run on two threads uses pushLocked().
// N slots hold N - 1 items. push() and pop() never allocate and never block.
template <typename T, int N>
struct Ring {
    static_assert(N >= 2, "a ring needs at least two slots");
    T buf[N];
    std::atomic<int> head{0}, tail{0};
    std::mutex pushLock;
    bool push(const T& x) {
        const int h = head.load(std::memory_order_relaxed), n = (h + 1) % N;
        if (n == tail.load(std::memory_order_acquire)) return false;   // full
        buf[h] = x;
        head.store(n, std::memory_order_release);
        return true;
    }
    bool pushLocked(const T& x) { std::lock_guard<std::mutex> g(pushLock); return push(x); }
    bool pop(T& x) {
        const int t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) return false;   // empty
        x = buf[t];
        tail.store((t + 1) % N, std::memory_order_release);
        return true;
    }
};

}  // namespace lg
