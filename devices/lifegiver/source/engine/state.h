// Lifegiver: the saved state and how it is stored in the Live set.
//
// Live stores it in a hidden Live parameter ([pattr state], type blob) as a plain list of numbers. Why numbers:
// - the data lives in the device's own parameter, so it follows the device and needs no shared name (a dict's name
//   changes on reload and can point at another device's dict: build 15a lost its state exactly that way);
// - no symbols are made per save (Max never frees symbols).
// Layout: MAGIC, CODEC, then the number entries (count; per entry: key, value count, values), then the text entries
// (count; per entry: key, text). Every key and text is (length, character codes 0..127).
#pragma once
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace lg {

// Flat saved state: numbers and text by key.
struct StateDoc {
    std::map<std::string, std::vector<double>> num;
    std::map<std::string, std::string> str;
    const std::vector<double>* get(const std::string& k) const { auto i = num.find(k); return i == num.end() ? nullptr : &i->second; }
    double get1(const std::string& k, double def) const { auto v = get(k); return (v && !v->empty()) ? (*v)[0] : def; }
    std::string gets(const std::string& k) const { auto i = str.find(k); return i == str.end() ? std::string() : i->second; }
    bool empty() const { return num.empty() && str.empty(); }
};

constexpr double STATE_MAGIC = 4931;   // "this list is Lifegiver state"
constexpr double STATE_CODEC = 1;
constexpr size_t STATE_MAX_NUMBERS = 30000;   // stays under Max's list limits

inline void encodeText(const std::string& s, std::vector<double>& out) {
    out.push_back((double)s.size());
    for (unsigned char c : s) out.push_back((double)(c < 128 ? c : '?'));
}

inline std::vector<double> encodeState(const StateDoc& d) {
    std::vector<double> o;
    o.push_back(STATE_MAGIC);
    o.push_back(STATE_CODEC);
    o.push_back((double)d.num.size());
    for (auto& kv : d.num) {
        encodeText(kv.first, o);
        o.push_back((double)kv.second.size());
        for (double v : kv.second) o.push_back(std::isfinite(v) ? v : 0.0);
    }
    o.push_back((double)d.str.size());
    for (auto& kv : d.str) { encodeText(kv.first, o); encodeText(kv.second, o); }
    return o;
}

// false (and `d` left empty) for anything that is not a complete, well-formed Lifegiver state
inline bool decodeState(const std::vector<double>& in, StateDoc& d) {
    d = StateDoc();
    size_t p = 0;
    auto count = [&](double maxv, long& out) {
        if (p >= in.size()) return false;
        const double v = in[p++];
        if (!(v >= 0 && v <= maxv) || v != std::floor(v)) return false;
        out = (long)v;
        return true;
    };
    auto text = [&](std::string& s) {
        long n;
        if (!count(10000, n) || p + (size_t)n > in.size()) return false;
        s.clear();
        for (long i = 0; i < n; i++) {
            const double c = in[p++];
            if (!(c >= 0 && c < 128) || c != std::floor(c)) return false;
            s.push_back((char)c);
        }
        return true;
    };
    if (in.size() < 4 || in.size() > STATE_MAX_NUMBERS || in[0] != STATE_MAGIC || in[1] != STATE_CODEC) return false;
    p = 2;
    long nn, ns;
    StateDoc t;
    if (!count(100000, nn)) return false;
    for (long e = 0; e < nn; e++) {
        std::string k;
        long nv;
        if (!text(k) || !count(100000, nv) || p + (size_t)nv > in.size()) return false;
        std::vector<double> v(in.begin() + (long)p, in.begin() + (long)p + nv);
        p += (size_t)nv;
        t.num[k] = v;
    }
    if (!count(100000, ns)) return false;
    for (long e = 0; e < ns; e++) {
        std::string k, v;
        if (!text(k) || !text(v)) return false;
        t.str[k] = v;
    }
    if (p != in.size()) return false;   // trailing junk: not ours
    d = t;
    return true;
}

}  // namespace lg
