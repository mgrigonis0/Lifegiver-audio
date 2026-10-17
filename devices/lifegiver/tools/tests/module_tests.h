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
static Stereo renderScaled(const Plan& p, const Stereo& in, double sr, double scale, bool average = false, bool live = false) {
    Chain ch;
    ch.setLiveMatch(live);   // the device's live level match (on the audio thread)
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
    // ambience: further back = a short room. On a pluck the tail measure grows; on drums the room is too short to move
    // it (a hit's decay is measured within its first tens of ms), so there: the gaps between the hits fill a little.
    {
        bool longer = true;
        // how loud the room is at the very back, against the dry sound (BS.1770): present, but well under the sound
        // (mixes with reverb at −9 LU or more were all judged too wet; RESEARCH)
        for (const char* nm : {"drums", "pluck"}) {
            const Audio a = sig::byName(nm, 3);
            Plan p;
            p[P_AMB_MIX] = ambMixFor(0.0); p[P_AMB_SIZE] = roomSizeFor(0.5);
            Stereo o = renderChain(p, fromAudio(a), a.sr);
            Audio room = a;
            for (size_t i = 0; i < a.size(); i++) { room.l[i] = (float)(o.l[i] - a.l[i]); room.r[i] = (float)(o.r[i] - a.r[i]); }
            std::vector<double> b0, b1;
            loudnessBlocks(a, b0); loudnessBlocks(room, b1);
            const double lu = integratedLoudness(b1) - integratedLoudness(b0);
            std::printf("    the room at the very back (mix %.2f), %s: %+.1f LU under the dry sound\n", p[P_AMB_MIX], nm, lu);
            longer = longer && lu > -24 && lu < -10;
        }
        for (const char* nm : {"pluck"}) {
            const Audio a = sig::byName(nm, 3);
            const Ref ref = makeRef(a);
            Plan p;
            p[P_AMB_MIX] = ambMixFor(0.1); p[P_AMB_SIZE] = roomSizeFor(0.5);
            Stereo o = renderChain(p, fromAudio(a), a.sr);
            MeasureAcc m0, m1;
            m0.add(a, ref);
            m1.add(toAudio(o, a.sr), ref);
            const double t0 = m0.finish(20, 20000).q[Q_TAIL], t1 = m1.finish(20, 20000).q[Q_TAIL];
            std::printf("    ambience at the back (mix %.2f), %s: Tail %.1f → %.1f dB\n", p[P_AMB_MIX], nm, t0, t1);
            longer = longer && t1 > t0 + 0.7;
        }
        fails += expect(longer && ambMixFor(0.8) == 0.0, "the ambience adds a short room at the back (-24 … -10 LU, a tail grows); none in the front half");
        // 17c: the room control (the stage's back wall). Small = drier, large = more and longer; even a large room at
        // the very back stays under −9 LU (wetter mixes were all judged too wet: RESEARCH, De Man 2017), and the
        // front half stays dry whatever the room
        {
            bool order = true, dry = true;
            for (const char* nm : {"drums", "pluck", "pad"}) {
                const Audio a = sig::byName(nm, 3);
                double lu[3];
                const double rooms[3] = {0.0, 0.5, 1.0};
                for (int k = 0; k < 3; k++) {
                    Plan p;
                    p[P_AMB_MIX] = ambMixFor(0.0, rooms[k]); p[P_AMB_SIZE] = roomSizeFor(rooms[k]);
                    Stereo o = renderChain(p, fromAudio(a), a.sr);
                    Audio room = a;
                    for (size_t i = 0; i < a.size(); i++) { room.l[i] = (float)(o.l[i] - a.l[i]); room.r[i] = (float)(o.r[i] - a.r[i]); }
                    std::vector<double> b0, b1;
                    loudnessBlocks(a, b0); loudnessBlocks(room, b1);
                    lu[k] = integratedLoudness(b1) - integratedLoudness(b0);
                }
                std::printf("    the room at the very back, %s: small %+.1f, as before %+.1f, large %+.1f LU under the dry sound\n", nm, lu[0], lu[1], lu[2]);
                order = order && lu[0] < lu[1] - 3 && lu[2] > lu[1] + 1;
                dry = dry && lu[2] <= -9;
            }
            fails += expect(order && dry && ambMixFor(0.6, 1.0) == 0.0,
                            "the room control: small drier, large wetter (never above -9 LU); the front half stays dry");
        }
        // dragged back and forth on the stage (the room wakes, fades, sleeps, wakes again): no clicks
        {
            Chain ch;
            Plan front, back;
            back[P_AMB_MIX] = ambMixFor(0.0); back[P_AMB_SIZE] = roomSizeFor(0.5);
            ch.reset(front, sr);
            std::vector<double> L((size_t)(8 * sr)), R(L.size());
            for (size_t i = 0; i < L.size(); i++) L[i] = R[i] = 0.4 * std::sin(2 * kPi * 70 * i / sr);
            const double at[] = {0.5, 1.0, 3.5, 3.6, 3.7, 5.0, 7.5};   // includes waking after a full sleep (> 1.5 s)
            size_t next = 0;
            bool toBack = true;
            for (size_t d = 0; d < L.size(); d += 64) {
                if (next < 7 && d >= (size_t)(at[next] * sr)) { ch.setTarget(toBack ? back : front); toBack = !toBack; next++; }
                const long n = (long)std::min<size_t>(64, L.size() - d);
                ch.process(L.data() + d, R.data() + d, L.data() + d, R.data() + d, n);
            }
            const ClickStats cs = findClicks(L);
            std::printf("    the stage dragged front <-> back 7 times (the room wakes and sleeps), low tone: clicks %ld\n", cs.count);
            fails += expect(cs.count == 0, "the room wakes and sleeps without clicks");
        }
        // the room's tail is colourless: no ringing pitch (16c's comb rang 20-40 dB over its neighbours)
        {
            Ambience amb; amb.setup(sr); amb.reset(); amb.setSize(1.1, 7000 - 3000 * 1.1);
            const int n = (int)(1.2 * sr);
            std::vector<double> x((size_t)n);
            for (int i = 0; i < n; i++) { amb.write(i == 0 ? 1.0 : 0.0); double l, r; amb.read(l, r); amb.advance(); x[(size_t)i] = l; }
            // spectrum of the tail (60 ms .. 560 ms), 4 Hz steps 200 Hz .. 4 kHz; peaks over the median of their 1/3 octave
            const int s0 = (int)(0.06 * sr), s1 = (int)(0.56 * sr);
            std::vector<double> db;
            for (double f = 200; f <= 4000; f += 4) {
                double re = 0, im = 0;
                for (int i = s0; i < s1; i++) {
                    const double wnd = 0.5 - 0.5 * std::cos(2 * kPi * (i - s0) / (s1 - s0)), ph = 2 * kPi * f * i / sr;
                    re += wnd * x[(size_t)i] * std::cos(ph); im += wnd * x[(size_t)i] * std::sin(ph);
                }
                db.push_back(10 * std::log10(re * re + im * im + 1e-30));
            }
            int over = 0;
            double worst = -99;
            for (size_t k = 0; k < db.size(); k++) {
                const double f = 200 + 4.0 * k;
                std::vector<double> nb;
                for (size_t j = 0; j < db.size(); j++) { const double g = 200 + 4.0 * j; if (g >= f / 1.12 && g <= f * 1.12) nb.push_back(db[j]); }
                std::nth_element(nb.begin(), nb.begin() + (long)(nb.size() / 2), nb.end());
                const double ex = db[k] - nb[nb.size() / 2];
                worst = std::max(worst, ex);
                if (ex > 12) over++;
            }
            const double share = 100.0 * over / (double)db.size();
            std::printf("    the room's tail: %.2f %% of the spectrum rings > 12 dB over its neighbours, worst %+.1f dB (decaying noise: ~0 %%, ~+11 dB)\n", share, worst);
            fails += expect(share < 0.5 && worst < 16, "the room's tail doesn't ring (no pitch builds up)");
        }
    }
    // 17b: Clean's excess tamer. A piercing bright noise (a +9 dB bump at 4.3 kHz on high-passed noise, hat-like hits)
    // is read as sticking out and tamed; an evenly bright one and a harmonic lead are left alone (no sameness)
    {
        auto hits = [&](bool bump, uint32_t seed) {
            Stereo n = noiseSig(6.0, sr, seed);
            Svf h1[2], h2[2], pk[2];
            for (int c = 0; c < 2; c++) { h1[c].set(Svf::HP, 3000, 0.7071, 0, sr); h2[c].set(Svf::HP, 3000, 0.7071, 0, sr); pk[c].set(Svf::BELL, 4300, 1.4, 9, sr); }
            const size_t step = (size_t)(0.125 * sr);
            for (size_t i = 0; i < n.size(); i++) {
                const double t = (double)(i % step) / sr, env = 0.5 * std::exp(-t / 0.012);
                double l = h2[0].tick(h1[0].tick(n.l[i] * env)), r = h2[1].tick(h1[1].tick(n.r[i] * env));
                if (bump) { l = pk[0].tick(l); r = pk[1].tick(r); }
                n.l[i] = l; n.r[i] = r;
            }
            return n;
        };
        const Stereo pierce = hits(true, 31), even = hits(false, 31);
        const Profile pp = analyze(*makeTakeSet({toAudio(pierce, sr)}));
        const Profile pe = analyze(*makeTakeSet({toAudio(even, sr)}));
        const Profile pl = analyze(*makeTakeSet({sig::lead(4)}));
        std::printf("    sticks out: piercing noise %+.1f dB at %.0f Hz · even noise %s · a lead %s\n", pp.excDb, pp.excHz,
                    pe.excHz > 0 ? "yes" : "nothing", pl.excHz > 0 ? "yes" : "nothing");
        fails += expect(pp.excHz > 0 && std::fabs(std::log2(pp.excHz / 4300)) < 0.34 && pp.excDb >= 3,
                        "the reading finds what sticks out of a piercing sound (within 1/3 octave)");
        fails += expect(pe.excHz == 0 && pl.excHz == 0, "an evenly bright sound and a harmonic lead: nothing sticks out (left alone)");
        // Clean tames it: the bump's band comes down; Clean 0 leaves it exactly
        Inputs in;
        in.clean = 1;
        const Mix mx = mixOf(in, -1);
        Plan on = basePlan(in, pp, mx);
        on[P_LOWCUT_ON] = 0; on[P_RES_DEPTH] = 0; on[P_AMB_MIX] = 0; on[P_WIDTH] = 1; on[P_STAGE_DB] = 0;
        Plan off = on; off[P_EXC_DEPTH] = 0;
        const Stereo a1 = renderChain(on, pierce, sr), a0 = renderChain(off, pierce, sr);
        const double cut = toneDb(a1.l, 4300, sr, 9600) - toneDb(a0.l, 4300, sr, 9600);
        const double side = toneDb(a1.l, 12000, sr, 9600) - toneDb(a0.l, 12000, sr, 9600);
        bool exact = true;
        for (size_t i = 0; i < pierce.size(); i++) exact = exact && bitEqual(a0.l[i], pierce.l[i]);
        std::printf("    Clean 100 %%: the bump %+.1f dB, 12 kHz %+.1f dB\n", cut, side);
        fails += expect(cut < -3 && std::fabs(side) < 0.5 && exact, "Clean tames the excess (bump down, the rest untouched; depth 0 = exactly the input)");
        // an ask for presence wins: Bite up (knob or a character) takes the taming back
        in.knob[Q_BITE] = 4;
        fails += expect(basePlan(in, pp, mx)[P_EXC_DEPTH] == 0.0, "asking for Bite (+4) switches the taming off: it never fights an ask");
        in.knob[Q_BITE] = 0;
        in.clean = 0.4;
        fails += expect(std::fabs(basePlan(in, pp, mx)[P_EXC_DEPTH] - 0.6) < 1e-9, "the taming follows Clean (17c: 40 % = 0.6 depth, full from 67 %)");
    }
    // 17b: Identity strength holds what nobody asked to change; roles only place the sound
    {
        auto ts = makeTakeSet({sig::lead(3)});
        const Profile pr = analyze(*ts);
        auto sideEffect = [&](double fit) {
            Inputs in;
            in.fit = fit;
            in.knob[Q_AIR] = 8;
            const Solution s = solve(*ts, pr, in, -1);
            double d = 0;
            for (int q = 0; q < NQ; q++) if (q != Q_AIR && s.units.ok[q]) d += std::fabs(s.reached[q]);
            return d;
        };
        const double strong = sideEffect(0.0), weak = sideEffect(1.0);
        std::printf("    Air +8 on a lead: the other qualities moved %.2f steps in all at Identity 100 %%, %.2f at 0 %%\n", strong, weak);
        fails += expect(strong <= weak + 1e-6, "Identity strength: more of it, less change to what wasn't asked");
        Inputs a, b;
        a.role = ROLE_RHYTHM; b.role = ROLE_LEAD;
        a.depth = b.depth = 0.6;
        double pa[NQ], pb[NQ];
        stagePush(a, pa); stagePush(b, pb);
        bool same = true;
        for (int q = 0; q < NQ; q++) same = same && pa[q] == pb[q];
        fails += expect(same && std::fabs(pa[Q_PUNCH] - 0.3) < 1e-9, "roles only place: at the same distance every role asks the same (distance cues only)");
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
        Stereo o = renderScaled(hard, l, sr, 4.0, false, true);
        double pk = 0;
        for (size_t i = 0; i < o.size(); i++) pk = std::max(pk, std::max(std::fabs(o.l[i]), std::fabs(o.r[i])));
        // matched by loudness (BS.1770, as the device measures it), over the last 2 s: at +32 dB of Bite the spectrum
        // tilts so far that plain RMS and loudness disagree by several dB
        auto lastLufs = [&](const Stereo& x) {
            Audio t; t.sr = sr;
            for (size_t i = (size_t)(4 * sr); i < x.size(); i++) { t.l.push_back((float)x.l[i]); t.r.push_back((float)x.r[i]); }
            std::vector<double> bl; loudnessBlocks(t, bl); return integratedLoudness(bl);
        };
        // at a normal level the loudness is matched; on loud noise the safety ceiling has the last word (peaks under
        // 0 dBFS beat the match: the match is measured before the ceiling, so it can't chase it)
        Stereo q = noiseSig(6.0, sr, 5);
        for (size_t i = 0; i < q.size(); i++) { q.l[i] *= 0.05; q.r[i] *= 0.05; }
        const Stereo oq = renderScaled(hard, q, sr, 4.0, false, true);
        const double lv = lastLufs(oq) - lastLufs(q);
        std::printf("    Scale 400 %%: on noise at %.1f LUFS the output is %+.1f LU vs the input; on loud noise the peak is %.3f\n", lastLufs(q), lv, pk);
        fails += expect(pk <= 1.0 && std::fabs(lv) < 1, "Scale 400 %: loudness matched to the input, peaks under 0 dBFS");
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
        // a 16c/17a set (reading without the 17b excess, plans without the tamer's values)
        StateDoc d2;
        TestHost::doc(a.host.pattr, d2);
        d2.num["prof"].resize(23);
        d2.num["play"].resize(NP_16C);
        d2.num["solplan"].resize(NP_16C);
        d2.str["saved_by"] = "2026-10-17a";
        Rig c;
        c.host.pattr = atomsOf(encodeState(d2));
        c.init();
        c.idle(1.0);
        bool same2 = c.ctl->profile().valid && !c.ctl->profile().has17b;
        for (int i = 0; i < NP; i++) same2 = same2 && bitEqual(c.ctl->playing()[i], i < NP_16C ? a.ctl->playing()[i] : neutral[i]);
        fails += expect(same2, "a 17a set opens: its reading back, exactly its plan plays (the excess tamer off)");
        c.msg("air", {-8});
        c.idle(0.1);
        fails += expect(c.ctl->playing()[P_AIR_DB] < air - 0.5 && c.ctl->playing()[P_EXC_DEPTH] == 0.0,
                        "and its knobs still act; Clean's taming waits for a new Listen (no excess known)");
        // a 17b set (plans without the standing cut, inputs without the room)
        StateDoc d3;
        TestHost::doc(a.host.pattr, d3);
        d3.num["play"].resize(NP_17B);
        d3.num["solplan"].resize(NP_17B);
        d3.num["playin"].resize(d3.num["playin"].size() - 1);
        d3.str["saved_by"] = "2026-10-17b";
        Rig e;
        e.host.pattr = atomsOf(encodeState(d3));
        e.init();
        e.idle(1.0);
        bool same3 = e.ctl->profile().valid && e.ctl->profile().has17b && e.ctl->inputs().room == 0.5;
        for (int i = 0; i < NP; i++) same3 = same3 && bitEqual(e.ctl->playing()[i], i < NP_17B ? a.ctl->playing()[i] : neutral[i]);
        fails += expect(same3, "a 17b set opens: exactly its plan plays (no standing cut until it re-plans), the room as before");
    }
}
