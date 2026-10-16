// Minimal WAV read/write for the offline tools (PCM 16/24/32-bit integer and 32-bit float; mono or stereo).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../source/engine/measure.h"

namespace wav {

inline bool read(const std::string& path, lg::Audio& out, std::string* err = nullptr) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { if (err) *err = "can't open " + path; return false; }
    std::vector<unsigned char> d;
    unsigned char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
    std::fclose(f);
    auto u16 = [&](size_t p) { return (uint32_t)d[p] | ((uint32_t)d[p + 1] << 8); };
    auto u32 = [&](size_t p) { return u16(p) | (u16(p + 2) << 16); };
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) { if (err) *err = "not a WAV"; return false; }
    int fmt = 0, ch = 0, bits = 0;
    uint32_t sr = 0;
    size_t dataAt = 0, dataLen = 0;
    for (size_t p = 12; p + 8 <= d.size();) {
        const uint32_t len = u32(p + 4);
        if (!std::memcmp(d.data() + p, "fmt ", 4)) {
            fmt = (int)u16(p + 8); ch = (int)u16(p + 10); sr = u32(p + 12); bits = (int)u16(p + 22);
            if (fmt == 0xFFFE && len >= 26) fmt = (int)u16(p + 32);   // WAVE_FORMAT_EXTENSIBLE: the sub-format
        } else if (!std::memcmp(d.data() + p, "data", 4)) {
            dataAt = p + 8; dataLen = std::min<size_t>(len, d.size() - dataAt);
        }
        p += 8 + len + (len & 1);
    }
    if (!dataAt || ch < 1 || !sr) { if (err) *err = "no audio data"; return false; }
    const int bps = bits / 8;
    const size_t frames = dataLen / (size_t)(bps * ch);
    out.sr = sr;
    out.l.resize(frames); out.r.resize(frames);
    for (size_t i = 0; i < frames; i++) {
        float v[2] = {0, 0};
        for (int c = 0; c < ch && c < 2; c++) {
            const unsigned char* s = d.data() + dataAt + (i * (size_t)ch + (size_t)c) * (size_t)bps;
            if (fmt == 3 && bits == 32) { float x; std::memcpy(&x, s, 4); v[c] = x; }
            else if (bits == 16) v[c] = (int16_t)(s[0] | (s[1] << 8)) / 32768.0f;
            else if (bits == 24) { int32_t x = (s[0] << 8) | (s[1] << 16) | (s[2] << 24); v[c] = (float)(x / 2147483648.0); }
            else if (bits == 32) { int32_t x; std::memcpy(&x, s, 4); v[c] = (float)(x / 2147483648.0); }
        }
        out.l[i] = v[0];
        out.r[i] = ch > 1 ? v[1] : v[0];
    }
    return true;
}

inline bool write(const std::string& path, const lg::Audio& a) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t frames = (uint32_t)a.size(), dataLen = frames * 8, sr = (uint32_t)a.sr;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataLen); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); w32(16); w16(3); w16(2); w32(sr); w32(sr * 8); w16(8); w16(32);
    std::fwrite("data", 1, 4, f); w32(dataLen);
    for (uint32_t i = 0; i < frames; i++) { std::fwrite(&a.l[i], 4, 1, f); std::fwrite(&a.r[i], 4, 1, f); }
    std::fclose(f);
    return true;
}

}  // namespace wav
