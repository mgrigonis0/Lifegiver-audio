# Lifegiver — Pre-build concept review

_2026-10-15. A critical read of DESIGN, BUILD_PLAN, RESEARCH, HANDOFF and CONVENTIONS against each other, the cited
sources and the wanderer-synth reference. Findings only; nothing here is decided until the user says so. Each item:
problem → why it matters → recommendation._

Severity: **C** = contradiction or gap that breaks the concept or the build if not settled now · **M** = major, settle
before the phase it touches · **R** = research correction · **A** = architecture/reference note · **U** = underused
· **S** = small.

## Verdict in one paragraph
The goal (an assistant that reads a sound and shapes it for its role, transparently) is sound and well supported by the
research. The control model and module list are good. Five logic gaps would bite during the build: the device's level
handling contradicts itself, the brain has no way to know it hit the target, saved sets would lose the reading, "order
as a choice" breaks plan-space blending, and the knobs' zero has two meanings. All five have a clean fix, and one of them
(closed-loop planning on the captured take) is also the biggest unused opportunity in the design.

---

## C — settle before building

**C1. Level: the docs disagree.** HANDOFF says "Distance = level + brightness cues" and RESEARCH says level is the main
distance cue, but DESIGN §5/§7 leave overall level out of "back" and §10/§11.13 loudness-match the output. Live's fader
also owns level. A loudness-matched output cancels the level cue; an unmatched one makes every A/B dishonest.
→ Split level into two parts. (1) *Make-up*: a static gain that removes the level change the processing causes by
accident, always on. (2) *Intended level*: only what the stage asks (back = quieter), small and shown in the plain-words
line ("−3 dB, further back"). Compare and Amount always audition loudness-matched. Consequence: with level partly
removed, distance must lean on brightness, softer attacks and a little room (see R2).

**C2. The brain can't know whether it hit the target.** §7 and §9 say plans are "re-solved so they still hit the
target", but nothing predicts what a plan does to *this* sound's qualities. Processing is nonlinear (saturation,
dynamics), so rules alone can't guarantee it.
→ **Closed-loop planning on the captured take.** The 4 s Listen take stays in memory (≈ 1.5 MB stereo at 48 kHz). The
worker thread renders the take through a candidate plan, measures the 8 qualities and the artifact measures, and
adjusts until the target is met (a few dozen fast offline renders). This one mechanism gives:
blends that really hit the target; a *static*, exact make-up gain (C1, M3); safety rules as *measurements* on this sound
(intermodulation and aliasing measured, not guessed from "dense/noisy"); and Moves that show real numbers (U3).

**C3. Reopening a set would lose or change the sound.** Phase 6 says "saved state in the set" but not what. If the
reading and plan aren't saved, a reload has no zero points; if the plan is re-derived and the brain changed in a new
build, old sets sound different. A 4 s take is too big for the pattr dict.
→ Save: the reading (profile + certainty), the resolved plan (module parameters), the user's Moves overrides, the
brain version, and a compact *sensitivity table* (how each quality moves per parameter, from the last solve). Reload
plays the saved plan exactly. After reload, knob changes use the sensitivity table; a full re-solve needs a Re-listen
(small hint: "Re-listen to refine"). Never re-listen automatically.

**C4. "Order as a choice" (§12.8) contradicts one-chain blending.** Blending A (cut before compressing) with B (colour
after) at 30 % has no answer, and switching order live clicks or needs two chains.
→ One fixed order. Where an alternative order matters, the module exists twice (pre and post slot) and the plan sets
amounts; amounts blend smoothly. Proposed: Clean (low cut, resonance) → EQ/tilt → dynamic EQ → pre-colour saturation
→ dynamics (comp, transient, multiband) → post-colour saturation/HF softening → width/pan → short ambience → make-up.

**C5. The knob zero has two meanings.** §4: "0 = as the sound is now". Same section: offsets "add on top of the
stage's target". Once the stage moves, a knob at 0 no longer means "as read".
→ Define **0 = what the stage asks**. The readout shows both: "reads: bright · stage: a bit brighter". Turning right =
more than the stage asks. With the stage at the role's neutral start, this equals "as read", so the idea survives.

## M — settle before the phase it touches

**M1. Real-time control path (Phase 5).** Dragging the stage or automating a knob must respond instantly, but a solve
takes time.
→ Two paths: *fast* = sensitivity table, instant, used while dragging and for automation; *slow* = closed-loop refine on
the worker, debounced, handed to the audio thread with smoothed parameters. Module on/off always via its dry/wet mix
(S-fade, ≥ 3 ms). Decide which controls are Live parameters (proposal: all knobs, stage x/y/width/edge/range, Identity ↔
Fit, Amount, blend; not the A–E buttons or Moves swaps).

**M2. Latency is fixed in Max for Live.** A device declares latency once ("Defined Latency" in the patcher, in samples);
it can't change while running. Any plan-dependent latency (oversampling on/off, lookahead) would misalign the track.
→ Target **zero latency**: SVF/IIR filters, Linkwitz-Riley crossovers, ADAA with a minimum-phase IIR 2× oversampler
(tiny, constant delay; dry path delayed to match), no lookahead. If anything needs latency later, it's constant and set
by `build_device.py`. Answers DESIGN §14's open latency question.

**M3. A true-peak limiter on a channel is the wrong tool.** Live mixes in floating point (no clipping between tracks);
−1 dBTP and −14 LUFS are delivery specs for the master. A per-track limiter colours exactly what Punch and Steady shape.
→ Drop the limiter. With static make-up (C1/C2) there are no surprise level jumps. At most an emergency soft clip far
above full scale, off in normal use.

**M4. Duplicate controls, against the design's own rule.** Edge (driven, grittier, held level) overlaps Texture + Steady;
the Range strip overlaps Weight/Air and Clean's low cut; stage pan overlaps Live's pan.
→ Keep the layers but write precedence: stage = target, knobs = offsets on it (C5), Range = hard limits nothing crosses,
Clean low cut = technical floor below Range. Pan earns its place only if it does more than Live's: *true stereo pan*
(moves a stereo image as a whole, coupled with width and mono lows). Live's default pan is a balance on stereo tracks,
but Live 11's split stereo mode already places each channel. User's call: keep stage pan for one-place control, or
drop it and let the mixer pan.

**M5. Role zones are ambiguous.** Dragging the shape into the Bed zone: does the role change?
→ Role only by button; zones are faint hints or removed.

**M6. Variations vs target.** A variation's character can oppose the target (Vintage = soft top, target = more Air).
Five may also collapse into near-copies on some sounds (a noisy texture loses B and D's saturation).
→ Rule: the target wins; the character is a tool preference. A variation that can't reach the target in character says
so ("reaches 70 %"). Measured near-duplicates are merged and shown as fewer options. User swaps/offs in Moves persist
across variation changes and are marked.

**M7. One solver instead of many special cases.** A–E, blending, Identity ↔ Fit, safety rules and per-sound limits are
described separately; built separately they'll conflict.
→ Write the brain as one cost to minimise: distance to target + artifact penalties (measured) + identity loss, within
per-sound limits. A variation = a set of tool weights; a blend = mixed weights, then solve; Identity ↔ Fit = the weight
on identity loss vs role pull. Same code path for everything; testable.

**M8. "Specificities" have no method.** Identity ↔ Fit and the resonance tamer's "character or flaw" decision rely on
them; RESEARCH names the concept but no way to detect it.
→ Practical proxy, to test on stems: peaks at a fixed frequency that stay across notes and levels = body/character
(keep); peaks that appear only on loud notes or certain pitches = flaw (tame); distinctive attack/decay shapes = character.
Low certainty → Identity acts gently. Mark as a research gap; tune by ear.

**M9. Undefined states.** Before the first Listen, and after the input changes (another part of the song, another patch).
→ Before Listen: clean pass-through, controls dimmed, "Listen first". Optional, user's call: a near-free drift light
(running level + brightness vs the take) that says "input changed since Listen". It doesn't adapt anything, so the "no
live tracking" decision stands.

**M10. Build order puts the riskiest part last.** The brain (Phase 5) comes after nine modules; if the core idea needs
changes, module work is wasted.
→ Insert a **vertical slice** after Phase 3: Weight/Warm/Bite/Air with EQ + tilt only, closed-loop solve, static
make-up, Moves text, one role, variation A only. Proves read → target → solve → hear end to end, then modules are added
to serve measured needs.

**M11. "Earns its place" has no yardstick.** Define before Phase 4, per module: target-hit error (in the quality's own
units), artifact budget (intermodulation, aliasing, clicks), CPU budget, and a blind A/B on the stems by the user.
Proposed CPU budget: ≤ 3 % of one core per instance on the user's Intel Mac at 48 kHz (it's a per-track device; ten
instances should be easy).

## R — research corrections

**R1. Punch bands overstated.** Fenton & Lee's model uses only 64 Hz–1 kHz because of a pivot near 1 kHz; the 2–8 kHz
bands trended *upward* and were left for further work. RESEARCH's "2–8 kHz transients read as presence or weight" isn't
in the paper. → Rewrite: "punch is modelled in 64 Hz–1 kHz; higher bands untested". Also: longer onsets matter more in
upper bands. Design §7's drum example stays valid.

**R2. Distance: level is the main cue only when it changes.** In [21] level varied with distance as in a real room.
Kolarik et al. found direct-to-reverberant ratio useful especially for far sources and when level isn't available. Since
Lifegiver mostly removes level (C1), "a little room" deserves more weight than RESEARCH implies. "Not *just* reverb"
still holds.

**R3. Loudness targets misapplied.** Spotify's −14 LUFS / −1 dBTP are delivery specs for the finished master, not
per-track goals (M3). Keep BS.1770 as the *measuring* method only.

**R4. Weak source on distortion.** [6] is an interview. → Replace with Geddes & Lee's AES papers on perception of
nonlinear distortion (same conclusions, citable).

**R5. Audio Commons models: use as starting points only.** They're absolute 0–100 regressions; an independent check
(DAGA 2025) covered only sharpness and roughness (r ≈ 0.67–0.73 against listeners). → Verify each relative measure on the
test stems before trusting it; expect warmth/depth to need our own definition.

Verified as stated: Sabin et al. ("warm" opposite across listeners and voice vs drums; "bright" consistent); ADAA +
oversampling; IMD on complex signals and multiband shaping; Giannoulis compressor design.

## A — architecture and the wanderer-synth reference

- **A1.** Wanderer's `perform64` resizes `thread_local` vectors on the audio thread (allocation, against CONVENTIONS).
  → Preallocate in `dsp64` from the max vector size when copying.
- **A2.** Wanderer is an instrument with one phase inlet; Lifegiver needs stereo audio in (`plugin~` → 2 signal inlets)
  and the dry path for Amount/Compare inside the external (same delay as the wet path).
- **A3.** Wanderer's handoff lists pattr dict save/restore and two-instance isolation as *not yet verified in Live*.
  Lifegiver depends on both (C3). → Phase 1 Live test adds: save the set, reopen, state intact.
- **A4.** Threads: three as designed (audio, main, worker), plus the solver also runs on the worker (C2), never on Max's
  main thread (it would freeze the UI). Results back by pointer through the report ring, freed on main (as Wanderer).
- **A5.** Take buffer: preallocate stereo, longest take at the highest sample rate (192 kHz × ~10 s wall time to allow
  silence skipping).

## U — underused

- **U1.** The take is used only for reading → use it for closed-loop planning (C2). Biggest single gain.
- **U2.** Certainty is only displayed → also scale each knob's range and the plan's strength by it.
- **U3.** Moves could show measured results ("Bite +2.0 of +2 asked", "level −0.4 dB matched").
- **U4.** Range could be a hard constraint of the solver rather than separate filters only.

## S — small

- **S1.** Silence skipping must keep decays (hold after onsets, relative threshold), or Tail and Steady read wrong.
- **S2.** Sparse material (a few hits in 4 s): certainty must count onsets/notes, not seconds.
- **S3.** Compressor timing from the *lowest* F0 seen, with margin: later notes may go lower than the take.
- **S4.** Amount 0 % must be bit-exact dry (with the same constant delay), tested.
- **S5.** The 169 px question stays open for mockup v2; the Moves console is the natural candidate for a pop-out.

## Proposed changes to the docs (after the user agrees, one at a time)

DESIGN: §4 (C5), §5/§10/§11.13 (C1, M3), §7/§9 (C2, M6, M7), §12.8 (C4), new "States and saving" (C3, M9), §14 (M2
answered). BUILD_PLAN: vertical slice after Phase 3 (M10), metrics in Phase 2 (M11), save/reopen in Phase 1 (A3).
RESEARCH: R1–R5.

## Sources checked in this review
- Max 8 docs, Live audio devices (Defined Latency): https://docs.cycling74.com/max8/vignettes/live_audiodevices
- Fenton & Lee, punch model (AES 139): https://eprints.hud.ac.uk/id/eprint/26288/9/AES%20139_Perceptual_Punch_Model_Fenton.pdf
- Auditory distance cues in rooms (Frontiers 2017): https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2017.00969/pdf
- Kolarik et al. 2011, level and DRR cues: https://ueaeprints.uea.ac.uk/id/eprint/81360
- Sabin, Rafii, Pardo 2011: https://users.cs.northwestern.edu/~zra446/doc/Sabin-Rafii-Pardo%20-%20Weighting-Function%96Based%20Rapid%20Mapping%20of%20Descriptors%20to%20Audio%20Processing%20Parameters%20-%20JAES%202011.pdf
- Czedik-Eysenberg & Reuter, DAGA 2025 (timbral model check): https://homepage.univie.ac.at/christoph.reuter/unterwegs/DAGA2025_Czedik-Eysenberg_Reuter_PAPER.pdf
- wanderer-synth `devices/wanderer/` at 1eb3c26 (wrapper, CI, handoff)
