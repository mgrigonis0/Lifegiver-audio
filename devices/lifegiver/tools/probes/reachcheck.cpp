#include <chrono>
#include <cstdio>
#include <string>
#include "brain.h"
#include "capture.h"
#include "../wav.h"
using namespace lg;
static Audio listen(const Audio& src) {
    static Capture cap; cap.cancel(); cap.release(); cap.arm();
    std::vector<double> L(128), R(128);
    for (size_t i = 0; i < src.size() && cap.state.load() != Capture::DONE; i += 128) {
        const long n = (long)std::min<size_t>(128, src.size() - i);
        for (long k = 0; k < n; k++) { L[(size_t)k] = src.l[i + (size_t)k]; R[(size_t)k] = src.r[i + (size_t)k]; }
        cap.process(L.data(), R.data(), n, src.sr);
    }
    Audio t; t.sr = src.sr; const long n = cap.frames();
    t.l.assign(cap.left(), cap.left() + n); t.r.assign(cap.right(), cap.right() + n); cap.cancel(); cap.release(); return t;
}
int main(int argc, char** argv) {
    const char* roles[] = {"Rhythm", "Support", "Bed", "Lead", "Rhythm", "Lead", "Hook", "Lead", "Support", "Bed", "Rhythm"};
    const char* names[] = {"drumbass", "jazzmix", "strings", "voice", "snare", "sax", "clarinet", "trumpet2", "piano", "synthstrings", "hat_real"};
    double tq = 0, ts_ = 0, rsum[5] = {0}; int n = 0;
    for (int si = 0; si < 11; si++) {
        Audio src; std::string err;
        if (!wav::read(std::string(argv[1]) + "/" + names[si] + ".wav", src, &err)) continue;
        auto ts = makeTakeSet({listen(src)});
        Profile p = analyze(*ts);
        Inputs in; int role = ROLE_LEAD;
        for (int r = 0; r < NROLES; r++) if (std::string(roles[si]) == ROLE_NAMES[r]) role = r;
        in.role = role; const RolePreset rp = rolePresetFor(role, p);
        in.depth = rp.depth; in.width = rp.width; in.lo = rp.lo; in.hi = rp.hi;
        auto t0 = std::chrono::steady_clock::now();
        Solution q = solve(*ts, p, in, -1, nullptr, 8, false);
        auto t1 = std::chrono::steady_clock::now();
        Solution s = solve(*ts, p, in, -1);
        auto t2 = std::chrono::steady_clock::now();
        const double mq = std::chrono::duration<double, std::milli>(t1 - t0).count(), ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
        tq += mq; ts_ += ms; n++;
        std::printf("%-12s first plan %5.0f ms, solve %5.0f ms | A-E reach", names[si], mq, ms);
        for (int v = 0; v < NVAR; v++) { const Solution sv = solve(*ts, p, in, v, &s, 5); rsum[v] += sv.reach; std::printf(" %3.0f", sv.reach * 100); }
        std::printf("\n"); std::fflush(stdout);
        (void)q;
    }
    std::printf("mean       first plan %5.0f ms, solve %5.0f ms | A-E reach", tq / n, ts_ / n);
    for (int v = 0; v < NVAR; v++) std::printf(" %3.0f", rsum[v] / n * 100);
    std::printf("\n");
}
