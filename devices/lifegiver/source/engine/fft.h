// Lifegiver: a small radix-2 FFT for the offline measures (worker thread only; it allocates its tables once).
#pragma once
#include <cmath>
#include <complex>
#include <vector>

namespace lg {

class Fft {
public:
    explicit Fft(int n) : n_(n), tw_(n / 2), rev_(n), win_(n), buf_(n) {
        int bits = 0;
        while ((1 << bits) < n) bits++;
        for (int i = 0; i < n; i++) {
            int r = 0;
            for (int b = 0; b < bits; b++) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev_[i] = r;
        }
        for (int i = 0; i < n / 2; i++) tw_[i] = std::polar(1.0, -2 * 3.14159265358979323846 * i / n);
        // periodic Hann; power normalised so a full-scale sine's bin power sums to its mean power
        double ws = 0;
        for (int i = 0; i < n; i++) { win_[i] = 0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * i / n); ws += win_[i] * win_[i]; }
        norm_ = 2.0 / (ws * n);
    }
    int size() const { return n_; }

    // power spectrum of n real samples (Hann window), bins 0..n/2, scaled so that sum(bins) ≈ mean square of x
    void power(const double* x, std::vector<double>& out) {
        for (int i = 0; i < n_; i++) buf_[rev_[i]] = std::complex<double>(x[i] * win_[i], 0.0);
        transform();
        out.resize((size_t)n_ / 2 + 1);
        for (int i = 0; i <= n_ / 2; i++) out[(size_t)i] = std::norm(buf_[(size_t)i]) * norm_ * (i == 0 || i == n_ / 2 ? 0.5 : 1.0);
    }

private:
    int n_;
    std::vector<std::complex<double>> tw_;
    std::vector<int> rev_;
    std::vector<double> win_;
    std::vector<std::complex<double>> buf_;
    double norm_ = 1;

    void transform() {
        for (int len = 2; len <= n_; len <<= 1) {
            const int half = len / 2, step = n_ / len;
            for (int i = 0; i < n_; i += len)
                for (int j = 0; j < half; j++) {
                    const std::complex<double> t = buf_[(size_t)(i + j + half)] * tw_[(size_t)(j * step)];
                    buf_[(size_t)(i + j + half)] = buf_[(size_t)(i + j)] - t;
                    buf_[(size_t)(i + j)] += t;
                }
        }
    }
};

}  // namespace lg
