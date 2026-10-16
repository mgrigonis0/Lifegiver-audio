// Offline checks of the 16c modules: dynamic EQ, resonance tamer, high smoothing, multiband, second saturation,
// ambience, Scale (with its level match and ceiling), the Average mode, roles that don't pan, and reading a 16b set
// (included by engine_test.cpp).
#pragma once

static Audio toAudio(const Stereo& s, double sr) {
    Audio a;
    a.sr = sr;
    a.l.assign(s.l.begin(), s.l.end());
    a.r.assign(s.r.begin(), s.r.end());
    return a;
}
// power in a narrow band around f (dB), skipping the first `from` samples
static double bandDb(const std::vector<double>& x, double f, double sr, size_t from) {
    Svf b; b.set(Svf::BPN, f, 12, 0, sr);
    double s = 0; size_t n = 0;
    for (size_t i = 0; i < x.size(); i++) { const double y = b.tick(x[i]); if (i >= from) { s += y * y; n++; } }
    return powToDb(s / std::max<size_t>(n, 1) + 1e-30);
}
static double rmsDb(const std::vector<double>& x, size_t from, size_t to) {
    double s = 0;
    for (size_t i = from; i < to; i++) s += x[i] * x[i];
    return powToDb(s / std::max<size_t>(to - from, 1) + 1e-30);
}
static Stereo renderScaled(const Plan& p, const Stereo& in, double sr, double scale, bool average = false) {
    Chain ch;
    ch.setScale(scale);
    ch.setAverage(average);
    ch.reset(p, sr);
    Stereo o = in;
    for (size_t d = 0; d < in.size(); d += 64) {
        const long n = (long)std::min<size_t>(64, in.size() - d);
        ch.process(o.l.data() + d, o.r.data() + d, o.l.data() + d, o.r.data() + d, n);
    }
    return o;
}

static void testModules() {
    std::printf("the 16c modules\n");
    const double sr = 48000;
    // off = exactly the input, even with their places set
    {
        Plan p;
        p[P_RES_HZ1] = 1500; p[P_RES_HZ2] = 3000; p[P_MB_F1] = 180; p[P_DWARM_THR] = -3; p[P_AMB_SIZE] = 1.1;
        Stereo in = noiseSig(0.5, sr), o = renderChain(p, in, sr);
        bool exact = true;
        for (size_t i = 0; i < in.size(); i++) if (!bitEqual(in.l[i], o.l[i]) || !bitEqual(in.r[i], o.r[i])) exact = false;
        fails += expect(exact, "every new module at 0 is exactly the input");
    }
    // multiband: the three bands sum to an all-pass (no colour when nothing is compressed)
    {
        Plan p;
        p[P_MB_LO] = 1; p[P_MB_MID] = 1; p[P_MB_HI] = 1; p[P_MB_THR1] = 40; p[P_MB_THR2] = 40; p[P_MB_THR3] = 40;
        double worst = 0;
        for (double f : {60.0, 200.0, 900.0, 2500.0, 7000.0, 15000.0}) {
            Stereo s = sineSig(f, 0.5, sr, 0.3);
            Stereo o = renderChain(p, s, sr);
            worst = std::max(worst, std::fabs(toneDb(o.l, f, sr, 9600) - toneDb(s.l, f, sr, 9600)));
        }
        std::printf("    multiband, nothing compressed: largest level change %.3f dB (60 Hz … 15 kHz)\n", worst);
        fails += expect(worst < 1e-6, "the multiband's bands sum to exactly the input");
        Plan c = p;
        c[P_MB_THR1] = -30; c[P_MB_THR2] = -30; c[P_MB_THR3] = -30;
        Stereo n = noiseSig(1.0, sr);
        Stereo o = renderChain(c, n, sr);
        const double d = rmsDb(o.l, 9600, o.size()) - rmsDb(n.l, 9600, n.size());
        std::printf("    multiband at full amount on loud noise: %.1f dB\n", d);
        fails += expect(d < -6, "the multiband compresses a loud sound");
    }
    // dynamic EQ: Bite cuts the 3 kHz bursts, leaves the steady 500 Hz tone alone
    {
        Stereo s;
        s.l.resize((size_t)(4 * sr)); s.r.resize(s.l.size());
        uint32_t seed = 5;
        for (size_t i = 0; i < s.size(); i++) {
            const double t = i / sr, burst = std::fmod(t, 0.4) < 0.1 ? 0.3 : 0.0;
            s.l[i] = s.r[i] = 0.2 * std::sin(2 * M_PI * 500 * t) + burst * std::sin(2 * M_PI * 3000 * t) + 0.01 * noise(seed);
        }
        auto ts = makeTakeSet({toAudio(s, sr)});
        const Profile pr = analyze(*ts);
        Plan p;
        p[P_DBITE_AMT] = 1; p[P_DBITE_THR] = pr.dynThr[1];
        Stereo o = renderChain(p, s, sr);
        const double cut = bandDb(o.l, 3000, sr, 9600) - bandDb(s.l, 3000, sr, 9600);
        const double keep = bandDb(o.l, 500, sr, 9600) - bandDb(s.l, 500, sr, 9600);
        std::printf("    dynamic Bite (its usual share %.1f dB): the 3 kHz bursts %+.1f dB, the 500 Hz tone %+.2f dB\n", pr.dynThr[1], cut, keep);
        fails += expect(cut < -3 && std::fabs(keep) < 0.5, "dynamic EQ: cuts the band when it builds up, not the rest");
    }
    // resonance tamer: a ringing peak in noise is found and tamed; the rest stays
    {
        Stereo s = noiseSig(4.0, sr);
        Svf r[2];
        for (auto& f : r) f.set(Svf::BELL, 1500, 12, 15, sr);
        for (size_t i = 0; i < s.size(); i++) { s.l[i] = 0.3 * r[0].tick(s.l[i]); s.r[i] = 0.3 * r[1].tick(s.r[i]); }
        auto ts = makeTakeSet({toAudio(s, sr)});
        const Profile pr = analyze(*ts);
        std::printf("    resonances found: %.0f %.0f %.0f %.0f Hz\n", pr.resHz[0], pr.resHz[1], pr.resHz[2], pr.resHz[3]);
        bool found = false;
        for (double f : pr.resHz) found = found || std::fabs(f / 1500 - 1) < 0.05;
        fails += expect(found, "the tamer finds a ringing peak (1.5 kHz)");
        Plan p;
        p[P_RES_DEPTH] = 1;
        for (int k = 0; k < 4; k++) p[P_RES_HZ1 + k] = pr.resHz[k];
        Stereo o = renderChain(p, s, sr);
        const double at = bandDb(o.l, 1500, sr, 9600) - bandDb(s.l, 1500, sr, 9600);
        const double off = bandDb(o.l, 600, sr, 9600) - bandDb(s.l, 600, sr, 9600);
        std::printf("    tamer at full depth: the peak %+.1f dB, 600 Hz %+.2f dB\n", at, off);
        fails += expect(at < -3 && std::fabs(off) < 0.5, "the tamer dips the resonance, not the rest");
        const Profile pl = analyze(*makeTakeSet({sig::lead()}));
        std::printf("    the synthetic lead (partials only): resonances %.0f %.0f %.0f %.0f Hz\n", pl.resHz[0], pl.resHz[1], pl.resHz[2], pl.resHz[3]);
        fails += expect(pl.resHz[0] == 0, "a sound's own partials are not taken for resonances");
    }
    // high smoothing: crackle over a bright tone → less grain (Texture down); a clean bright tone barely changes
    {
        Stereo s, c;
        s.l.resize((size_t)(4 * sr)); s.r.resize(s.l.size()); c = s;
        uint32_t seed = 7;
        Svf hp; hp.set(Svf::HP, 1000, 0.7071, 0, sr);
        double ph = 0;
        for (size_t i = 0; i < s.size(); i++) {
            ph += 2 * M_PI * 220 / sr;
            const double tone = sig::harm(ph, 220, sr, 1.0) * 0.1;
            const double crack = (lcg(seed) % 300 == 0) ? noise(seed) * 3 : 0;
            c.l[i] = c.r[i] = tone;
            s.l[i] = s.r[i] = tone + 0.4 * hp.tick(crack);
        }
        const Audio a = toAudio(s, sr);
        const Ref ref = makeRef(a);
        Plan p, p2;
        p[P_SMOOTH_AMT] = 1; p2[P_SMOOTH_AMT] = 2;
        Stereo o = renderChain(p, s, sr), o2 = renderChain(p2, s, sr), oc = renderChain(p, c, sr);
        MeasureAcc m0, m1, m2;
        m0.add(a, ref);
        m1.add(toAudio(o, sr), ref);
        m2.add(toAudio(o2, sr), ref);
        const double t0 = m0.finish(20, 20000).q[Q_TEXTURE], t1 = m1.finish(20, 20000).q[Q_TEXTURE], t2 = m2.finish(20, 20000).q[Q_TEXTURE];
        double de = 0, ce = 0;
        for (size_t i = 9600; i < c.size(); i++) { de += (oc.l[i] - c.l[i]) * (oc.l[i] - c.l[i]); ce += c.l[i] * c.l[i]; }
        std::printf("    smoothing: crackly tone Texture %.1f → %.1f (amount 1) → %.1f dB (amount 2); a clean bright tone changes by %.1f dB\n", t0, t1, t2, powToDb(de / ce));
        fails += expect(t1 < t0 - 0.3 && t2 < t0 - 1 && powToDb(de / ce) < -40, "smoothing lowers the grain of a crackly sound (modestly), leaves a clean tone alone");
    }
    // second saturation: harmonics after the dynamics
    {
        Stereo s = sineSig(200, 1.0, sr, 0.25);
        s.r = s.l;
        Plan p; p[P_SAT2_DRIVE] = 0.6; p[P_SAT_EVEN] = 0; p[P_SAT_REF] = 0.25;
        Stereo o = renderChain(p, s, sr);
        const double h3 = toneDb(o.l, 600, sr, 4800) - toneDb(o.l, 200, sr, 4800);
        std::printf("    second saturation: H3 %.1f dBc\n", h3);
        fails += expect(h3 > -45, "the post saturation adds harmonics");
    }
    // ambience: further back = more tail (on short hits and on a pluck)
    {
        bool longer = true;
        for (const char* nm : {"drums", "pluck"}) {
            const Audio a = sig::byName(nm, 3);
            const Ref ref = makeRef(a);
            Plan p;
            p[P_AMB_MIX] = ambMixFor(0.1); p[P_AMB_SIZE] = ambSizeFor(0.1);
            Stereo o = renderChain(p, fromAudio(a), a.sr);
            MeasureAcc m0, m1;
            m0.add(a, ref);
            m1.add(toAudio(o, a.sr), ref);
            const double t0 = m0.finish(20, 20000).q[Q_TAIL], t1 = m1.finish(20, 20000).q[Q_TAIL];
            std::printf("    ambience at the back (mix %.2f), %s: Tail %.1f → %.1f dB\n", p[P_AMB_MIX], nm, t0, t1);
            longer = longer && t1 > t0 + 0.7;
        }
        fails += expect(longer && ambMixFor(0.8) == 0.0, "the ambience lengthens the tail at the back; none in the front half");
    }
    // Scale
    {
        Plan busy = busyPlan();
        Plan bare = busy;
        for (const ScaleRange& r : SCALED) bare[r.idx] = 0;
        bare[P_MAKEUP_DB] = 0;
        Stereo n = noiseSig(1.0, sr, 21);
        for (size_t i = 0; i < n.size(); i++) { n.l[i] *= 0.3; n.r[i] *= 0.3; }
        Stereo a = renderScaled(busy, n, sr, 0.0), b = renderScaled(bare, n, sr, 1.0);
        double diff = 0;
        for (size_t i = 0; i < n.size(); i++) diff = std::max(diff, std::max(std::fabs(a.l[i] - b.l[i]), std::fabs(a.r[i] - b.r[i])));
        fails += expect(diff < 1e-9, "Scale 0 %: no shaping (the placement stays: cuts, Clean, stage)");
        // the shape doubles (the level match takes the overall level back to the input's)
        Plan p; p[P_BITE_DB] = 6;
        Stereo s;
        s.l.resize((size_t)sr); s.r.resize(s.l.size());
        for (size_t i = 0; i < s.size(); i++) s.l[i] = s.r[i] = 0.1 * std::sin(2 * M_PI * 3000 * i / sr) + 0.1 * std::sin(2 * M_PI * 300 * i / sr);
        const Stereo o2 = renderScaled(p, s, sr, 2.0);
        const double g2 = (toneDb(o2.l, 3000, sr, 4800) - toneDb(s.l, 3000, sr, 4800)) - (toneDb(o2.l, 300, sr, 4800) - toneDb(s.l, 300, sr, 4800));
        Svf bell; bell.set(Svf::BELL, 3000, 0.9, 12, sr);
        const double want = linToDb(bell.mag(3000, sr)) - linToDb(bell.mag(300, sr));
        std::printf("    Bite +6 dB at Scale 200 %%: 3 kHz over 300 Hz %+.2f dB (a +12 dB bell: %+.2f)\n", g2, want);
        fails += expect(std::fabs(g2 - want) < 0.3, "Scale 200 % doubles the shaping");
        // 400 %: collapse, but the level is matched to the input and the peaks stay under 0 dBFS
        Plan hard; hard[P_SAT_DRIVE] = 1; hard[P_SAT_REF] = 0.3; hard[P_BITE_DB] = 8; hard[P_WEIGHT_DB] = 6; hard[P_COMP_AMT] = 1; hard[P_COMP_THR] = -20;
        Stereo l = noiseSig(6.0, sr, 4);
        for (size_t i = 0; i < l.size(); i++) { l.l[i] *= 0.5; l.r[i] *= 0.5; }
        Stereo o = renderScaled(hard, l, sr, 4.0);
        double pk = 0;
        for (size_t i = 0; i < o.size(); i++) pk = std::max(pk, std::max(std::fabs(o.l[i]), std::fabs(o.r[i])));
        const double lv = rmsDb(o.l, (size_t)(4 * sr), o.size()) - rmsDb(l.l, (size_t)(4 * sr), l.size());
        std::printf("    Scale 400 %% on loud noise: peak %.3f, level %+.1f dB vs the input\n", pk, lv);
        fails += expect(pk <= 1.0 && std::fabs(lv) < 2, "Scale 400 %: level matched to the input, peaks under 0 dBFS");
        // automation of Scale on a low tone: no clicks
        Stereo t;
        t.l.resize((size_t)(4 * sr)); t.r.resize(t.l.size());
        for (size_t i = 0; i < t.size(); i++) { t.l[i] = 0.3 * std::sin(2 * M_PI * 110 * i / sr) + 0.1 * std::sin(2 * M_PI * 165 * i / sr); t.r[i] = 0.9 * t.l[i]; }
        Chain ch;
        ch.reset(Plan(), sr);
        Stereo out = t;
        for (size_t d = 0; d < t.size(); d += 64) {
            if (d == (size_t)(0.2 * sr)) ch.setTarget(busyPlan());
            if (d == (size_t)(0.5 * sr)) ch.setScale(4);
            if (d == (size_t)(1.5 * sr)) ch.setScale(0);
            if (d == (size_t)(2.5 * sr)) ch.setScale(1.3);
            if (d == (size_t)(3.0 * sr)) ch.setAverage(true);
            if (d == (size_t)(3.5 * sr)) ch.setAverage(false);
            const long n2 = (long)std::min<size_t>(64, t.size() - d);
            ch.process(out.l.data() + d, out.r.data() + d, out.l.data() + d, out.r.data() + d, n2);
        }
        ClickStats ca = findClicks(out.l), cb = findClicks(out.r);
        std::printf("    Scale 100→400→0→130 %%, Dynamic→Average→Dynamic on a low tone: clicks %ld/%ld (worst %.2f)\n", ca.count, cb.count, std::max(ca.worst, cb.worst));
        fails += expect(ca.count == 0 && cb.count == 0, "Scale and the Average switch change without clicks");
    }
    // the Average mode: what the solve measured is held (a steady signal: the same as dynamic, within a dB)
    {
        auto ts = makeTakeSet({sig::lead(3)});
        const Profile pr = analyze(*ts);
        Inputs in;
        in.vars[4] = 1;   // Driven: multiband in every band
        in.knob[Q_BITE] = -6;
        const Solution s = solve(*ts, pr, in, -1);
        std::printf("    Driven, Bite -6: dynamic Bite %.2f, multiband %.2f %.2f %.2f; held averages: dBite %.1f, mb %.1f %.1f %.1f dB\n",
                    s.tools[T_DBITE], s.tools[T_MBLO], s.tools[T_MBMID], s.tools[T_MBHI], s.plan[P_AVG_DBITE], s.plan[P_AVG_MB1], s.plan[P_AVG_MB2], s.plan[P_AVG_MB3]);
        const Stereo x = fromAudio(sig::lead(3));
        Stereo dynO = renderScaled(s.plan, x, 48000, 1.0, false), avgO = renderScaled(s.plan, x, 48000, 1.0, true);
        const double d = rmsDb(avgO.l, 4800, avgO.size()) - rmsDb(dynO.l, 4800, dynO.size());
        std::printf("    Average vs Dynamic, overall level: %+.2f dB\n", d);
        fails += expect(s.plan[P_AVG_MB1] < 0 && std::fabs(d) < 1.5, "Average holds the measured average (about the same level, no movement)");
    }
    // roles don't pan
    {
        Rig r;
        r.init();
        r.idle(2.0);
        r.msg("pan", {0.4});
        r.host.params.clear();
        r.msg("pickrole", {ROLE_FX});
        fails += expect(!r.host.params.count("pan") && std::fabs(r.ctl->inputs().pan - 0.4) < 1e-12 && r.host.params.count("depth"),
                        "a role button leaves the pan where the user put it");
    }
    // a set saved by 16b (10 tools, 31 plan values, a 14-value reading) opens: exactly its plan plays, knobs act
    {
        Rig a;
        a.init();
        a.idle(2.0);
        a.listen(sig::lead(4));
        a.idle(4.0);
        bool known = a.ctl->lastSolve().ok;
        for (int k = 0; k < NT; k++) {
            bool col = false;
            for (int m = 0; m < NM; m++) col = col || a.ctl->lastSolve().J[m][k] != 0.0;
            known = known && col;
        }
        fails += expect(known, "after Listen every tool's effect is measured (the asked ones first, the rest right after)");
        StateDoc d;
        TestHost::doc(a.host.pattr, d);
        std::vector<double>& pv = d.num["prof"];
        pv.resize(14);
        const std::vector<double> sv = d.num["sol"];
        std::vector<double> old;
        size_t k = 0;
        for (int t = 0; t < NT; t++, k++) if (t < NT_16B) old.push_back(sv[k]);
        for (int m = 0; m < NM; m++) for (int t = 0; t < NT; t++, k++) if (t < NT_16B) old.push_back(sv[k]);
        for (; k < sv.size(); k++) old.push_back(sv[k]);
        d.num["sol"] = old;
        d.num["play"].resize(NP_16B);
        d.num["solplan"].resize(NP_16B);
        d.str["saved_by"] = "2026-10-16b";
        Rig b;
        b.host.pattr = atomsOf(encodeState(d));
        b.init();
        b.idle(1.0);
        bool same = b.ctl->profile().valid;
        const Plan neutral;
        for (int i = 0; i < NP; i++) same = same && bitEqual(b.ctl->playing()[i], i < NP_16B ? a.ctl->playing()[i] : neutral[i]);
        fails += expect(same, "a 16b set opens: its reading back, exactly its plan plays (new modules off)");
        const double air = b.ctl->playing()[P_AIR_DB];
        b.msg("air", {-8});
        b.idle(0.1);
        fails += expect(b.ctl->playing()[P_AIR_DB] < air - 0.5, "and its knobs still act (saved sensitivities)");
    }
}
