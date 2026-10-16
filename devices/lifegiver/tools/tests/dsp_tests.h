// Offline checks of the chain, the measures, Listen's capture, the reading and the brain (included by engine_test.cpp).
#pragma once

// a sine's level at f (Goertzel), dB re full scale
static double toneDb(const std::vector<double>& x, double f, double sr, size_t from = 0) {
    double s1 = 0, s2 = 0;
    const double c = 2 * std::cos(2 * M_PI * f / sr);
    size_t n = 0;
    for (size_t i = from; i < x.size(); i++, n++) { const double s0 = x[i] * (0.5 - 0.5 * std::cos(2 * M_PI * n / (double)(x.size() - from))) + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 10 * std::log10(p + 1e-30) - 20 * std::log10((double)n / 4.0);
}

static Stereo renderChain(const Plan& p, const Stereo& in, double sr, long block = 64) {
    Chain ch;
    ch.reset(p, sr);
    Stereo o = in;
    for (size_t d = 0; d < in.size(); d += (size_t)block) {
        const long n = (long)std::min<size_t>((size_t)block, in.size() - d);
        ch.process(o.l.data() + d, o.r.data() + d, o.l.data() + d, o.r.data() + d, n);
    }
    return o;
}

static void testChain() {
    std::printf("the chain\n");
    const double sr = 48000;
    Stereo in = noiseSig(0.5, sr);
    Stereo o = renderChain(Plan(), in, sr);
    bool exact = true;
    for (size_t i = 0; i < in.size(); i++) if (!bitEqual(in.l[i], o.l[i]) || !bitEqual(in.r[i], o.r[i])) exact = false;
    fails += expect(exact, "a neutral plan is exactly the input");

    // EQ: the filter's response is what it plays (bell +6 dB at 3 kHz, shelves)
    {
        Plan p;
        p[P_BITE_DB] = 6;
        Stereo s = sineSig(3000, 1.0, sr, 0.25);
        Stereo so = renderChain(p, s, sr);
        const double g = toneDb(so.l, 3000, sr, 4800) - toneDb(s.l, 3000, sr, 4800);
        Svf f; f.set(Svf::BELL, 3000, 0.9, 6, sr);
        std::printf("    bell +6 dB at 3 kHz: measured %+.2f dB, response %+.2f dB\n", g, linToDb(f.mag(3000, sr)));
        fails += expect(std::fabs(g - 6) < 0.1 && std::fabs(linToDb(f.mag(3000, sr)) - 6) < 0.05, "a +6 dB presence bell gives +6 dB at its centre");
        Svf ls; ls.set(Svf::LOWSHELF, 120, 0.7071, 4, sr);
        Svf hs; hs.set(Svf::HIGHSHELF, 10000, 0.7071, -5, sr);
        fails += expect(std::fabs(linToDb(ls.mag(30, sr)) - 4) < 0.2 && std::fabs(linToDb(ls.mag(3000, sr))) < 0.1 &&
                        std::fabs(linToDb(hs.mag(19000, sr)) + 5) < 0.4 && std::fabs(linToDb(hs.mag(500, sr))) < 0.1,
                        "shelves: their gain far inside, ~0 dB far outside");
    }
    // a plan jumping between neutral and every module working, on a low tone: S-shaped glides, no clicks
    {
        Stereo s;
        s.l.resize((size_t)(3 * sr)); s.r.resize(s.l.size());
        for (size_t i = 0; i < s.size(); i++) { s.l[i] = 0.3 * std::sin(2 * M_PI * 110 * i / sr) + 0.1 * std::sin(2 * M_PI * 165 * i / sr); s.r[i] = 0.9 * s.l[i]; }
        Chain ch;
        ch.reset(Plan(), sr);
        Stereo out = s;
        for (size_t d = 0; d < s.size(); d += 64) {
            if (d == (size_t)(0.8 * sr)) ch.setTarget(busyPlan());
            if (d == (size_t)(1.6 * sr)) ch.setTarget(Plan());
            if (d == (size_t)(1.62 * sr)) ch.setTarget(busyPlan());   // a new target in the middle of a glide
            const long n = (long)std::min<size_t>(64, s.size() - d);
            ch.process(out.l.data() + d, out.r.data() + d, out.l.data() + d, out.r.data() + d, n);
        }
        ClickStats a = findClicks(out.l), b = findClicks(out.r);
        std::printf("    plan jumps on a low tone: clicks %ld/%ld (worst %.2f)\n", a.count, b.count, std::max(a.worst, b.worst));
        fails += expect(a.count == 0 && b.count == 0, "plans change without clicks (also retargeted mid-glide)");
    }
    // silence after loud noise: no denormals, settles to exact zeros
    {
        Stereo s = noiseSig(1.0, sr);
        s.l.resize((size_t)(11 * sr), 0.0); s.r.resize(s.l.size(), 0.0);
        Stereo so = renderChain(busyPlan(), s, sr);
        long sub = 0;
        for (size_t i = 0; i < so.size(); i++) { if (std::fpclassify(so.l[i]) == FP_SUBNORMAL || std::fpclassify(so.r[i]) == FP_SUBNORMAL) sub++; }
        fails += expect(sub == 0, "10 s of silence after noise: no denormal numbers");
    }
    // stereo: width keeps a mono sound mono, the lows stay put; true pan moves the image
    {
        Stereo m = sineSig(1000, 0.5, sr, 0.3);
        m.r = m.l;
        Plan p; p[P_WIDTH] = 1.8;
        Stereo mo = renderChain(p, m, sr);
        double sd = 0;
        for (size_t i = 0; i < mo.size(); i++) sd = std::max(sd, std::fabs(mo.l[i] - mo.r[i]));
        fails += expect(sd < 1e-12, "widening a mono sound leaves it mono (no fake width)");
        Stereo lt; lt.l.assign((size_t)sr, 0.0); lt.r = lt.l;
        for (size_t i = 0; i < lt.size(); i++) lt.l[i] = 0.3 * std::sin(2 * M_PI * 2000 * i / sr) + 0.3 * std::sin(2 * M_PI * 40 * i / sr);
        Plan pr; pr[P_PAN] = 1;
        Stereo po = renderChain(pr, lt, sr);
        const double hiR = toneDb(po.r, 2000, sr, 9600), hiL = toneDb(po.l, 2000, sr, 9600);
        const double loR = toneDb(po.r, 40, sr, 9600), loL = toneDb(po.l, 40, sr, 9600);
        std::printf("    pan fully right, a left-only sound: 2 kHz L %.1f / R %.1f dB, 40 Hz L %.1f / R %.1f dB\n", hiL, hiR, loL, loR);
        fails += expect(hiR > hiL + 20 && loL > loR + 10, "true pan moves the left channel right; its lows stay where they were");
    }
    // saturation: even/odd character and aliasing kept low (ADAA)
    {
        Stereo s = sineSig(200, 1.0, sr, 0.25);
        s.r = s.l;
        Plan pe; pe[P_SAT_DRIVE] = 0.5; pe[P_SAT_EVEN] = 1; pe[P_SAT_REF] = 0.25;
        Plan po; po[P_SAT_DRIVE] = 0.5; po[P_SAT_EVEN] = 0; po[P_SAT_REF] = 0.25;
        Stereo e = renderChain(pe, s, sr), od = renderChain(po, s, sr);
        const double e2 = toneDb(e.l, 400, sr, 4800) - toneDb(e.l, 200, sr, 4800), e3 = toneDb(e.l, 600, sr, 4800) - toneDb(e.l, 200, sr, 4800);
        const double o2 = toneDb(od.l, 400, sr, 4800) - toneDb(od.l, 200, sr, 4800), o3 = toneDb(od.l, 600, sr, 4800) - toneDb(od.l, 200, sr, 4800);
        std::printf("    saturation 200 Hz: even H2 %.1f H3 %.1f dBc · odd H2 %.1f H3 %.1f dBc\n", e2, e3, o2, o3);
        fails += expect(e2 > o2 + 20 && o3 > -45, "even = 2nd harmonic, odd = 3rd harmonic (no 2nd)");
        // aliasing: a 7.5 kHz tone, hard drive; its 3rd harmonic (22.5 k) is in band, the 5th (37.5 k) folds to 10.5 k
        Stereo h = sineSig(7500, 1.0, sr, 0.25);
        h.r = h.l;
        Plan pa; pa[P_SAT_DRIVE] = 1; pa[P_SAT_EVEN] = 0; pa[P_SAT_REF] = 0.25;
        Stereo ha = renderChain(pa, h, sr);
        const double fund = toneDb(ha.l, 7500, sr, 4800), alias = toneDb(ha.l, 10500, sr, 4800);
        std::printf("    aliasing at full drive: the 5th harmonic folded to 10.5 kHz at %.1f dBc\n", alias - fund);
        fails += expect(alias - fund < -40, "aliasing stays under -40 dBc even at full drive");
    }
}

static void testMeasures() {
    std::printf("measures\n");
    {
        Audio a = sig::empty(5, 48000);
        for (size_t i = 0; i < a.size(); i++) { const float v = (float)(0.1 * std::sin(2 * kPi * 997 * i / 48000.)); a.l[i] = v; a.r[i] = v; }
        std::vector<double> b;
        loudnessBlocks(a, b);
        const double l = integratedLoudness(b);
        std::printf("    997 Hz at -20 dBFS, both channels: %.2f LUFS\n", l);
        fails += expect(std::fabs(l + 20) < 0.1, "BS.1770 loudness: a -20 dBFS stereo sine reads -20 LUFS");
    }
    {
        auto ts = makeTakeSet({sig::lead()});
        double prev = -1e9, d3 = 0, d6 = 0;
        bool up = true;
        for (double db : {0.0, 3.0, 6.0}) {
            Plan p; p[P_BITE_DB] = db;
            const Measured m = renderMeasure(*ts, p, 20, 20000);
            if (m.q[Q_BITE] <= prev) up = false;
            if (db == 0) prev = m.q[Q_BITE];
            if (db == 3) d3 = m.q[Q_BITE] - prev;
            if (db == 6) d6 = m.q[Q_BITE] - prev;
            if (db == 0) continue;
        }
        std::printf("    Bite after a presence lift of +3 / +6 dB: %+.2f / %+.2f dB\n", d3, d6);
        fails += expect(up && d3 > 1.5 && d6 > d3 + 1.5, "Bite grows with a presence lift, and keeps growing");
    }
    struct OnsetCase { const char* name; int lo, hi; };
    const OnsetCase oc[] = {{"lead", 7, 9}, {"drums", 7, 9}, {"bass", 7, 9}, {"pluck", 10, 12}, {"pad", 0, 3}, {"noise", 0, 1}};
    bool onsOk = true;
    std::printf("    onsets:");
    for (const OnsetCase& c : oc) {
        const Ref r = makeRef(sig::byName(c.name));
        std::printf(" %s %zu", c.name, r.onsets.size());
        if ((int)r.onsets.size() < c.lo || (int)r.onsets.size() > c.hi) onsOk = false;
    }
    std::printf("\n");
    fails += expect(onsOk, "note and hit starts are found (legato notes too), steady sounds have (almost) none");
    auto q = [](const char* n) { auto ts = makeTakeSet({sig::byName(n)}); MeasureAcc a; a.add(ts->takes[0], ts->refs[0]); return a.finish(20, 20000); };
    const Measured drums = q("drums"), lead = q("lead"), pluck = q("pluck"), pad = q("pad");
    std::printf("    punch drums %.1f lead %.1f · tail pluck %.1f lead %.1f · steady lead %.1f drums %.1f · pad corr %.2f\n",
                drums.q[Q_PUNCH], lead.q[Q_PUNCH], pluck.q[Q_TAIL], lead.q[Q_TAIL], lead.q[Q_STEADY], drums.q[Q_STEADY], pad.corr);
    fails += expect(drums.q[Q_PUNCH] > lead.q[Q_PUNCH] + 3, "drums read punchier than a synth lead");
    fails += expect(pluck.q[Q_TAIL] < lead.q[Q_TAIL] - 8, "plucks read shorter than sustained notes");
    fails += expect(lead.q[Q_STEADY] > drums.q[Q_STEADY] + 3, "a sustained lead reads steadier than drums");
    fails += expect(pad.corr < 0.5, "a detuned wide pad reads as wide (low L/R correlation)");
}

static void testCapture() {
    std::printf("Listen's capture\n");
    const double sr = 48000;
    // 0.5 s silence, 1 s tone, 2 s silence, 1 s tone, 1 s silence, 4 s tone
    Stereo s;
    auto add = [&](double sec, bool tone) {
        const size_t n = (size_t)(sec * sr), o = s.size();
        s.l.resize(o + n, 0.0); s.r.resize(o + n, 0.0);
        if (tone) for (size_t i = 0; i < n; i++) { s.l[o + i] = 0.3 * std::sin(2 * M_PI * 330 * i / sr) * std::exp(-(double)i / sr / 0.6); s.r[o + i] = s.l[o + i]; }
    };
    add(0.5, false); add(1, true); add(2, false); add(1, true); add(1, false); add(4, true);
    Capture cap;
    cap.arm();
    for (size_t d = 0; d < s.size() && cap.state.load() != Capture::DONE; d += 128)
        cap.process(s.l.data() + d, s.r.data() + d, (long)std::min<size_t>(128, s.size() - d), sr);
    const long n = cap.frames();
    long quiet = 0;
    for (long i = 0; i < n; i++) if (std::fabs(cap.left()[i]) < 1e-6) quiet++;
    std::printf("    kept %.2f s, of which %.2f s silent\n", n / sr, quiet / sr);
    fails += expect(cap.state.load() == Capture::DONE && std::fabs(n / sr - 4.0) < 0.05, "it stops by itself after 4 s of sound");
    fails += expect(quiet / sr < 1.0, "silences are skipped (only the short holds after sounds remain)");
    // decays are kept: a ringing tone decays below -45 dB re its peak... the hold keeps 0.4 s more
    Capture c2;
    c2.arm();
    Stereo z; z.l.assign((size_t)(13 * sr), 0.0); z.r = z.l;
    for (size_t i = 0; i < (size_t)(0.2 * sr); i++) { z.l[i] = 0.5 * std::sin(2 * M_PI * 440 * i / sr); z.r[i] = z.l[i]; }
    for (size_t d = 0; d < z.size() && c2.state.load() != Capture::DONE; d += 128) c2.process(z.l.data() + d, z.r.data() + d, 128, sr);
    fails += expect(c2.state.load() == Capture::DONE && c2.frames() < (long)(1.0 * sr), "a single short sound: it stops after 12 s of waiting, with just that sound");
    Capture c3;
    c3.arm();
    c3.cancel();
    fails += expect(c3.state.load() == Capture::IDLE, "Arm again cancels");
}

static void testReading() {
    std::printf("the reading\n");
    auto rd = [](const char* n) { auto ts = makeTakeSet({sig::byName(n)}); return analyze(*ts); };
    const Profile lead = rd("lead"), bass = rd("bass"), noise = rd("noise"), pluck = rd("pluck");
    std::printf("    lead %.0f–%.0f Hz harmonic %.2f · bass %.1f–%.1f Hz · noise harmonic %.2f · pluck sustained %.2f, lead %.2f\n",
                lead.f0min, lead.f0max, lead.harmonic, bass.f0min, bass.f0max, noise.harmonic, pluck.sustained, lead.sustained);
    fails += expect(std::fabs(lead.f0min - 220) < 4 && std::fabs(lead.f0max - 659) < 12, "pitch range of a lead (A3 … E5)");
    fails += expect(std::fabs(bass.f0min - 41.2) < 1.5 && std::fabs(bass.f0max - 61.7) < 2, "pitch range of a bass line (E1 … B1)");
    fails += expect(lead.harmonic > 0.9 && noise.harmonic < 0.2 && noise.f0min == 0, "pitched vs noisy");
    fails += expect(pluck.sustained < 0.6 && lead.sustained > 0.9, "plucked vs sustained");
    fails += expect(bass.lowestHz < 45 && lead.lowestHz > 150, "the lowest real content (Clean's floor sits below it)");
}

static void testBrain() {
    std::printf("the brain\n");
    auto ts = makeTakeSet({sig::lead()});
    const Profile p = analyze(*ts);
    Inputs in;
    in.depth = 0.5; in.width = 0.5; in.lo = 20; in.hi = 20000; in.clean = 0;
    in.knob[Q_BITE] = 5;
    const Solution s = solve(*ts, p, in, -1);
    std::printf("    lead, Bite +5: reached %+.1f of %+.1f, reach %.0f %%\n", s.reached[Q_BITE], s.asked[Q_BITE], s.reach * 100);
    fails += expect(s.ok && s.reach > 0.85 && std::fabs(s.reached[Q_BITE] - s.asked[Q_BITE]) < 1, "a knob turn is reached, measured on the take");
    {
        const Measured out = renderMeasure(*ts, s.plan, in.lo, in.hi);
        std::printf("    loudness in %.2f, out %.2f + make-up %+.2f\n", s.inLoud, out.loudness, s.plan[P_MAKEUP_DB]);
        fails += expect(std::fabs(out.loudness + s.plan[P_MAKEUP_DB] - s.inLoud) < 0.2, "the static make-up matches the loudness exactly");
    }
    const Solution s2 = solve(*ts, p, in, -1);
    bool same = true;
    for (int i = 0; i < NP; i++) if (s.plan[i] != s2.plan[i]) same = false;
    fails += expect(same, "solving is deterministic (same takes, same inputs → the same plan)");
    // the "Clean" variation never colours; noisy sounds don't get saturation
    Inputs tx = in;
    tx.knob[Q_BITE] = 0; tx.knob[Q_TEXTURE] = 6;
    const Solution sa = solve(*ts, p, tx, 0);
    fails += expect(sa.tools[T_SAT] == 0 && sa.reach < 0.2, "Clean (A) adds no harmonics, and says it can't reach more grain");
    const Solution sc = solve(*ts, p, tx, 2);
    fails += expect(sc.tools[T_SAT] > 0.05 && sc.reached[Q_TEXTURE] > 2, "Bold (C) adds harmonics for grain on a pitched sound");
    auto nts = makeTakeSet({sig::noise()});
    const Profile np = analyze(*nts);
    const Solution sn = solve(*nts, np, tx, 2);
    fails += expect(sn.tools[T_SAT] < 0.05, "no saturation on a noisy sound (it would make intermodulation)");
    // nothing there to lift: not adjustable
    Inputs w = in;
    w.knob[Q_BITE] = 0; w.knob[Q_WEIGHT] = 8;
    const Solution sw = solve(*ts, p, w, -1);
    fails += expect(!sw.units.ok[Q_WEIGHT] && sw.asked[Q_WEIGHT] == 0 && std::fabs(sw.plan[P_WEIGHT_DB]) < 0.01,
                    "a lead with no lows: Weight can't be lifted (nothing there), so nothing is boosted");
    // the fast path goes the right way at once
    Inputs fa = in;
    fa.knob[Q_AIR] = -6;
    const Solution f = fastStep(s, p, fa, -1);
    fails += expect(f.plan[P_AIR_DB] < s.plan[P_AIR_DB] - 1, "fast path: Air -6 lowers the top at once");
    // the role's starting place keeps a kick's fundamental
    auto dts = makeTakeSet({sig::drums()});
    const Profile dp = analyze(*dts);
    const RolePreset rp = rolePresetFor(ROLE_RHYTHM, dp);
    fails += expect(rp.lo <= dp.lowestHz * 0.8 + 1e-9, "Rhythm on drums: the range starts below the kick's lowest notes");
    // mono safety: an over-wide sound is narrowed by Clean
    Profile wide = p;
    wide.sideDb = 0;   // side as loud as mid
    Inputs cl = in;
    cl.clean = 1;
    const Plan bp = basePlan(cl, wide, mixOf(cl, -1));
    fails += expect(bp[P_WIDTH] < 0.8, "Clean narrows a sound that would lose a lot in mono");
    // stage: further back asks for less presence and air, and a little lower level
    Inputs back = in, front = in;
    back.depth = 0.05; front.depth = 0.95;
    double pb[NQ], pf[NQ];
    stagePush(back, pb);
    stagePush(front, pf);
    fails += expect(pb[Q_BITE] < pf[Q_BITE] - 2 && pb[Q_AIR] < pf[Q_AIR] - 2 && pb[Q_PUNCH] < pf[Q_PUNCH] - 2 &&
                    stageLevelDb(back) < -2.5 && stageLevelDb(back) >= -3 && stageLevelDb(front) > -0.2,
                    "the back of the stage: darker, softer attacks, up to -3 dB (the front: none of that)");
}
