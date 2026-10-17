# Lifegiver — Design

_2026-10-16 (revised after the pre-build review, `REVIEW.md`; §17 holds what build 16a actually does). The full design as decided. `HANDOFF.md` holds the
current state; this file holds the what and the why. Theory and sources: `RESEARCH.md`. Build order: `BUILD_PLAN.md`._

## 1. What the device is

A Max for Live audio effect that listens to a sound, reads what it is, and helps the user shape it for the job it has in
the mix. An **assistant, not an autopilot**: it gives an educated starting point and a set of strategies; the user decides
by ear. It must **work**: results over flashiness.

What it is **not** (keep it this way): not a modulation/motion device, not a reverb/delay effect, not a mix analyser,
not a limiter. It hears only its own track; the user hears the mix and steers.

## 2. The three layers

| Layer | What it is | Who drives it |
|---|---|---|
| **Clean** | Technical fixes: resonances, harsh high notes, rumble/DC | Automatic; one amount control |
| **The sound, as read** (8 knobs) | What the sound *is*; the user's own extra on top | Listening sets each knob's zero; the user turns |
| **Stage** | What the sound should *become*: role, place, size, range, intensity | The user; this is the main way to steer |

Plus: **Moves** (what the device is doing about it), **Variations A–E** (five strategies, mixed at any strengths),
**Identity ↔ Fit**, **Amount**, **Compare**.

## 3. Listen

- Listen = **Arm**: press Arm, then play (or it starts at once on running audio). Captures ~4 s of non-silent audio,
  skips silence, stops itself.
- **Up to 4 takes** (e.g. verse, chorus, low notes, high notes), each removable (✕). Re-listen replaces or adds a take.
  Why: more takes give the full note range (the lowest note sets safe dynamics timing) and a plan that works across the
  song; beyond 4 the reading barely changes while UI and background work grow (to verify on the stems).
- The reading merges all takes; certainty grows with the number of notes/onsets heard, not with seconds.
- Silence skipping keeps decays (hold after each onset, threshold relative to the take), or Tail and Steady read wrong.
- Analysis runs only on takes; no live tracking and no "input changed" light (the user re-listens when the part changes).
- It reads the **input**, never the output: it must not chase its own processing.
- Takes stay in memory: the brain tests its plans on them (§7).
- High notes: educated guess from the harmonic make-up; level-triggered taming catches what the guess misses.

## 4. The knobs: relative, zero = "as the sound was read"

- 8 knobs: **Weight · Warm · Bite · Air** (tone) · **Texture** · **Punch · Tail · Steady** (dynamics).
- Listening does **not** move the knobs. It sets each knob's **zero point** to the reading. Every knob sits in the
  middle at 0. Turning is **the user's own extra, on top of whatever the stage asks**: right = more of that quality,
  left = less, relative to *this* sound. The stage never turns the knobs, so there is always full travel both ways.
- Each knob shows a thin **stage arc** on its ring: from 0, left or right, as far as the stage is pushing that quality.
  The pointer is the user's turn. A small readout says what was heard and what the stage adds ("reads: soft · stage +1"),
  plus certainty dots.
- Result per quality = reading + stage + knob.
- Why relative: the same descriptor means different processing for different sounds and people (Sabin et al.: one
  listener's "warm" for a voice was nearly opposite their "warm" for drums). A fixed knob-to-EQ mapping can't work;
  a step relative to the measured sound can.
- **Range adapts to the sound**: a full turn is scaled to a safe maximum for this sound (an already bright sound has
  less "+Air" room before harshness), and gentler when certainty is low. No dead zones, no wall of distortion. Cuts
  have more safe room than boosts.
- Each knob is defined as a **measured quantity relative to the sound** (e.g. Warm ≈ low-mid energy relative to the
  sound's own fundamental and spectrum), not as a fixed EQ shape. Audio Commons models are a starting point only; each
  measure is verified on the test stems (RESEARCH §Measuring).
- Width and depth are not knobs (they live on the stage).

## 5. Stage: the main way to steer

Seen **from above**: back wall at the top, the listener at the bottom. The sound is a shape on it.

- **Drag the shape**: left/right = **pan**; forward/back = **distance**.
  - **Pan** is a true stereo pan: the whole stereo image moves, coupled with width; the lows stay centred.
  - **Distance**: front = clear, present, tighter level jumps, drier. Back = darker top, softer attack, smaller level
    jumps, a little more short room, and **up to about −3 dB** at the very back (a feel, not a mix move: the fader does
    the rest). Because level is kept small, brightness, attack and a little room carry most of the distance (RESEARCH
    §Distance).
- **Stretch it sideways** = **width** (mid/side; lows stay mono; only as wide as the sound holds up in mono).
- **Range it owns**: a side strip with low and high handles (frequency, log scale) = which part of the spectrum the
  sound may claim. It is a **hard limit**: neither the stage nor the knobs push energy outside it. Clean's low cut is
  the technical floor below it.
- **Edge smooth ↔ jagged** (handle above the shape) = **intensity**: calm/smooth ↔ driven (denser, grittier, held level).
  It is a stage target like the rest: it moves the Texture and Steady arcs; the knobs add on top.
- **Roles**: **Lead · Hook · Support · Rhythm · Bass · Bed · FX**, as buttons. A role drops the shape into a sensible
  start **and sets the device's priorities**:
  - Lead: protects clarity and presence. Hook: forward and distinct, short. Support (backing vocals, doubles,
    counter-melodies): just behind the lead, softer presence, gives way. Rhythm: protects attacks. Bass: owns the lows,
    mono, steady, harmonics so it is heard on small speakers. Bed: makes room, steadies, carves mids. FX: allows width
    and colour.
  - The role is the user's choice, never guessed, and **only a button changes it**: dragging a Lead to the back keeps it
    a Lead (its starting point and priorities stay). Faint floor zones only hint where each role usually sits.
- A plain-words line under the stage says what is being asked ("up front: present, clear, tighter level, drier · narrow ·
  owns 110 Hz – 9 k · a little drive · −0 dB").
- UI style: like PondSynth and Wanderer (`design/ui-reference/`): dark panels, coloured section titles with an underline
  bar, Live-style dials with the value and a muted sub-label below, pill buttons. PondSynth shows a draggable shape
  fits the 169 px strip.

## 6. Identity ↔ Fit

One control. **Fit** = sit nicely in the role (move toward what the role needs). **Identity** = exaggerate what makes this
sound *this* sound. This is what separates the device from an auto-corrector, which pushes everything to an average.
Research: a sound's identity lives in its **specificities** (hollow, metallic, a pinched end), not in shared dimensions.

Detecting specificities (no published method; our proxy, tested on the stems and tuned by ear):
- Peaks at a fixed frequency that stay across notes, takes and levels = body/character (keep or exaggerate).
- Peaks that appear only on loud notes or certain pitches = flaw (tame).
- A distinctive attack or decay shape = character.
- Low certainty → Identity acts gently.

Identity ↔ Fit also decides whether a spectral peak is character to keep or a flaw to tame (resonance tamer), and
applies to all variations.

## 7. How the device decides (where the smartness lives)

1. **What it is** (profile from the takes): the 8 qualities + facts: F0 / pitch range, harmonic ↔ inharmonic/noisy,
   plucked ↔ sustained, sparse ↔ dense, stereo correlation, resonances, specificities, certainty of each.
2. **What it needs** (target): role + stage position/size/range/edge → target per quality, plus the user's knob turns,
   pulled toward Fit or Identity.
3. **How to close the gap**: **one solver**. It looks for module settings that minimise: distance to the target
   + artifact penalties (measured) + identity loss, within per-sound limits and the Range. A variation is a set of tool
   preferences; a mix of variations is mixed preferences, then solved; Identity ↔ Fit is the weight on identity loss vs
   role pull. One code path for everything, testable.
4. **Closed loop on the takes**: the worker renders the takes through a candidate plan, measures the qualities and the
   artifacts, and adjusts until the target is met. So the device *knows* it hit the target, the make-up gain is exact
   and static, and safety rules are measurements on this sound (e.g. intermodulation actually measured before choosing
   saturation), not guesses.
   Examples of the same request reaching different tools by sound:
   - More **bite**: clean harmonic synth → add upper harmonics (they fuse). Dense/noisy → dynamic EQ presence (saturation
     would make intermodulation: muddy, metallic). Already harsh → lower what masks the bite instead of boosting.
   - More **weight**: real low fundamental → gentle low lift. No fundamental → low-band harmonics the ear hears as bass
     (an EQ boost would only raise noise).
   - More **punch**: drums → transient shaping in 64 Hz–1 kHz (where punch is modelled). Pad → slower compression timed
     from F0 (fast time constants distort low notes).
   - **Further back**: darker top, softer attack, smaller level jumps, a little room, up to −3 dB.
5. **Two speeds**: a *fast path* (a sensitivity table from the last solve: how each quality moves per setting) answers
   drags and automation instantly; the *slow path* (closed-loop refine) runs on the worker, debounced, and hands its plan
   over with smoothed parameters.
6. **Safety rules** (from `RESEARCH.md`): saturation only soft, low-order, antialiased, and only where the measured
   intermodulation stays small; compressor time constants longer than the period of the lowest note heard (with
   margin); never erase specificities (unless the user goes toward Fit); each change has a limit per sound.

## 8. Moves console

Its own small console. Lists the actions the device chose, in plain words ("Adding upper harmonics for bite",
"Steadying level, slow", "Clearing lows below 110 Hz", "Softening attack"), with the **measured result** ("Bite +2.0 of
+2 asked", "level matched, −0.4 dB"). Per move: **amount**, **on/off**, and a **method swap** where more than one method
exists ("bite via EQ instead"). User swaps and offs persist when the variation mix changes, and are marked.
Why: the strategy is visible and changeable; the main complaint in a user study of an assisted compressor was opacity.

## 9. Variations A–E, mixed at any strengths

_16b: A–E became characters of rising strength (Polish, Warm, Bold, Vintage, Driven), all 0 = none; see §17. The
mixing rules below still hold._

For one goal the device plans **five strategies** that reach the same target with different tool families, so each
sounds a bit different:

| | Name | Character | Main tools |
|---|---|---|---|
| A | Clean | clearer, no colour | EQ, dynamic EQ, transparent dynamics |
| B | Rich | fuller, warmer | harmonics (even), glue compression, parallel |
| C | Bold | edge, attack, forward | transient shaper, presence dynamic EQ, odd harmonics, attack/body split |
| D | Vintage | rounded, soft top, slow | soft saturation, HF softening, slow compression, low-band harmonics |
| E | Tight | controlled, focused | multiband dynamics, resonance control, soft clip, narrower |

- Calculated **per sound, every time**, not presets. A method that would damage this sound is swapped for one that works
  (e.g. no overtones on a noisy texture). Each variation is **labelled by what it actually does** ("clearer, no colour").
- **The target wins; character is a tool preference.** A variation that can't reach the target in character says so
  ("reaches 70 %"). Variations that measure nearly the same on this sound are merged and shown as fewer options.
- **Mixing: any number at any strengths** (one, a pair, or all five, e.g. A 50 % · C 30 % · E 20 %). Mixing happens
  **in the plan, not the audio**: all variations use the same modules in the same order, so a mix is one new plan,
  solved so it still hits the target. One chain → no extra CPU, no phase/comb problems.
- UI: buttons A–E in the Moves console, each with its own strength slider; click to audition one alone (instant,
  level-matched). The Moves list shows the active (mixed) plan.
- All five are planned in the background; only the active plan runs audio.

## 10. Overall controls and level

- **Amount**: strength and A/B in one: 0 % = the original, **bit-exact** (with the same constant delay), 100 % = the full
  plan.
- **Compare**: before/after, always loudness-matched.
- **Clean**: amount of automatic cleanup.
- Level, in three parts: **make-up** removes any level change the processing causes by accident (the solve's make-up
  acts at once; since 17a the device also measures its own output against its input as it plays and glides a small
  correction on top, so it stays matched at any setting); **intended level** is only the stage's distance (0 to about
  −3 dB), shown in the plain-words line; **Gain** (±12 dB, centre 0) is the user's own trim after everything. No
  output limiter: Live mixes in floating point, and a limiter would colour what Punch and Steady shape.

## 11. Under the hood: tools (modules)

Modular: any module can be added or removed later without redesign. Each must **earn its place by a measured
improvement** (BUILD_PLAN Phase 2 metrics) in offline tests on real audio.

1. **Resonance tamer**: narrow, signal-reactive cuts on spiky peaks (ERB-spaced detection). Identity ↔ Fit decides
   character vs flaw.
2. **Low cut / DC**: below the sound's real lows.
3. **EQ**: shelves and bells (SVF, minimum phase, instant response) for Weight, Warm, Bite, Air.
4. **Dynamic EQ**: bands that act only when needed (harsh loud notes, boomy low notes; de-essing is a case of this).
5. **Tilt**: one gentle slope over the spectrum (natural bright/dark; a distance cue).
6. **Saturation**: soft, low-order, antialiased (ADAA + 2× minimum-phase IIR oversampling). Even = warm, odd = edgy.
   Two slots: before and after dynamics.
7. **Band saturation** (2–3 bands, phase-coherent Linkwitz-Riley crossovers): lows → weight without boom; highs → exciter.
8. **Transient shaper**: attack (Punch) and sustain (Tail), level-independent (fast vs slow envelope difference),
   focused on low-mids.
9. **Compressor**: feedforward, dB-domain smoothing, smooth decoupled peak detector, soft knee, RMS option; time constants
   from the lowest F0.
10. **Multiband dynamics** (2–3 bands, same crossovers).
11. **Pan and width**: true stereo pan, mid/side width, lows mono; mono sources widened by decorrelation (not Haas).
12. **Short ambience**: early reflections + small darkened room, for distance, not as an effect.
13. **Output**: make-up (solved, then corrected live) + stage level, then the user's Gain; K-weighted loudness
    (BS.1770) is the measuring method.

**Fixed order** (every plan, every variation): Clean (low cut, resonance) → EQ/tilt → dynamic EQ → saturation (pre)
→ dynamics (compressor, transient shaper, multiband) → saturation (post) / HF softening → pan/width → short ambience
→ output. Where order matters, a module has two slots and the plan sets amounts; amounts mix smoothly.

## 12. Under the hood: methods (how tools are used)

1. **Parallel processing**: every move has a dry/wet mix (New York compression, parallel saturation); the mix is also
   how a move fades on and off without clicks.
2. **Attack/body split**: process the start of notes and their sustained part separately.
3. **Upward compression and downward expansion**: lift quiet detail (gentler Steady); lower what's between notes.
4. **Soft clipping**: shave the top of peaks for density/control (drums, plucks).
5. **Mid/side shaping**: EQ and saturation differ for centre and sides.
6. **Gain staging into colour**: level into saturation is measured and set, so "a little drive" is the same on any input.
7. **Filtered compressor detection**: the detector ignores deep lows (no pumping).
8. **HF softening** (tape-style): gentle level-dependent taming of the top.
9. **Mono safety check**: width only as far as the sound holds together in mono.

Consider later (cost: latency): **tonal/noisy split** (harmonic-percussive separation) to process tone and noise
separately (good for Identity and Texture).

Left out on purpose: long reverb, delay, chorus/modulation (effects, other devices); pitch/formant shift and octave-down
synthesis (change what the sound is); gates/noise reduction (repair, not shaping); linear-phase EQ (pre-ringing,
latency, CPU); more than 3 bands (artifacts for little gain); output limiter (§10).

## 13. Technology

- **C++ engine**, same setup as `wanderer-synth` (see `CONVENTIONS.md`): header-only C++17 with no Max deps, tested
  offline on Linux; thin Max wrapper `lifegiver~` (stereo in via `plugin~`, the dry path inside the external);
  GitHub Actions builds a universal `.mxo` and publishes the release zip.
- Threads: audio thread = the processing chain and take capture (never allocates/locks); main thread = control, state,
  Moves, fast path; worker thread = analysis and the closed-loop solver (never on Max's main thread: it would freeze the
  UI). Command/report rings between them; plans handed over by pointer and freed on the main thread.
- **Zero latency**: Max for Live latency is a fixed patcher setting ("Defined Latency") that can't change while running,
  so every plan must have the same latency. All filters are minimum-phase/IIR, no lookahead; the oversampler's tiny
  constant delay is matched on the dry path. If anything ever needs latency, it is constant and set by
  `build_device.py`.
- Take memory preallocated: 4 takes, stereo, sized for the highest sample rate.
- CPU budget: ≤ 3 % of one core per instance on the user's Intel Mac at 48 kHz.
- The brain lives in C++ (testable), not in ES5 JS. JS is for views only.

## 14. States and saving

- **Before the first Listen**: clean pass-through; knobs and stage dimmed with "Listen first".
- **Saved in the set** (pattr dict, as Wanderer): the reading (profile + certainty), the resolved plan (module
  settings), the stage, knobs, role, variation strengths, the user's Moves overrides, the brain version and the
  sensitivity table. Not the take audio (too big for the set).
- **Reopening** plays the saved plan exactly, even if a newer build's brain would plan differently. Knob and stage
  changes then use the saved sensitivity table; a full refine needs a Re-listen (hint: "Re-listen to refine"). The
  device never re-listens on its own.

## 15. Open questions

- Exact knob definitions (what each measures, its unit and safe range): decided per knob in the build, with tests.
- Role target tables (what each role/position asks of each quality): first draft in the build, tuned by ear.
- Specificity detection (§6): proxy to test on the stems.
- Whether the full UI fits 169 px or the Moves console needs a pop-out: decide on mockup v2.

## 17. As built (16a, revised in 16b and 16c): the concrete definitions

First drafts, measured on the synthetic signals and the user's 10 stems (`tools/render bench`); tuned by ear next.

**Measures** (`source/engine/measure.h`), each in dB, relative to the sound; frames, onsets and level windows are
chosen once on the input take and reused for every processed render (a change is the processing, never the selection):
- Weight / Warm / Bite / Air = power in [range lo–150 Hz] / [150–600 Hz] / [1.8–5 kHz] / [8 kHz–range hi] vs the rest
  of the range. A band (almost) empty (< −40 dB) can't be shaped: its knob shows "nothing to shape".
- Texture = spectral flatness 500 Hz–8 kHz (partials and noise filling the gaps; partial density per ERB, §RESEARCH).
- Punch = 64 Hz–1 kHz transient peak (first 30 ms after an onset) over the body 60–120 ms later.
- Tail = level 150–400 ms after each onset vs its peak. Steady = minus the spread (P90−P10) of the 50 ms level.
- Onsets: in any of 4 broad bands, a 6 dB rise over the last 60 ms that stays up for 30 ms (legato notes count; the
  pitch-period pulses of low notes and steady noise don't).
- Loudness: BS.1770 (K-weighted, gated), for the static make-up.

**Steps**: a knob is −10…+10; a full turn = this sound's safe maximum of that measure (e.g. Air +6 dB, +3 if already
airy; cuts get more room), × 0.6–1.0 by certainty. Stage pushes are in the same steps.

**Solver** (`brain.h`): Levenberg-Marquardt on finite-difference sensitivities (then Broyden updates, re-probed when the
model is off), regularised by the variation mix's tool costs, plus a cost on loudness change (a tool must not hide
behind make-up). The solver only moves tools that serve an asked quality (`TOOL_FOR`): Weight → low shelf; Warm →
bell 300 Hz, saturation; Bite → bell 3 kHz, saturation; Air → high shelf, tilt (D only); Texture → saturation;
Punch → transient attack, compressor; Tail → transient sustain, compressor, expander; Steady → compressor,
expander. An asked quality with no tool in the mix (grain in "Clean") is reported unreached, not faked. Reach =
the share of what was asked that arrived, weighted by how much was asked. The closed loop renders ≤ 6 s of the takes.

**Variations** = tool costs (A Clean: EQ + transparent dynamics, no saturation; B Rich: even harmonics, glue;
C Bold: presence, attack, odd harmonics; D Vintage: tilt, soft harmonics, slow; E Tight: control) + a compressor
style and saturation evenness each; mixes are geometric means of costs. Saturation costs 9× more on noisy sounds.
Default mix A 70 % / C 30 % (the mockup's).

**Stage**: role pushes from the mockup (Lead: Bite +1, Air +1; …), scaled by Identity↔Fit (0.4–1.0); distance:
Bite/Air/Punch ±1.5, Tail ∓1.5, Steady +|f|; edge: Texture +3·edge, Steady +1.5·edge; level 0…−3 dB.
Role starting places revised from the mockup: open ranges (Lead 60 Hz–20 k, Rhythm 25 Hz–20 k, Bass 25 Hz–16 k, …;
the mockup's 9 k top for Lead would have cut a vocal's air), roles that own their lows never start above 0.8 × the
sound's lowest content, and edge starts at 0–0.15 (saturation is never a default move).
Width 0.5 = as the sound is (side gain 0.25 + 1.5·width); widening capped where mono would suffer; **Clean** also
narrows a sound that loses > 1.5 dB in mono (the widened Juno stem). Stereo stage on a Linkwitz-Riley split at 120 Hz:
lows never widened, true pan moves only the highs (lows stay put), fades in/out so a neutral stage is the input.

**Chain** (`chain.h`): low cut → EQ (SVF) → tilt → saturation (tanh + bias, ADAA residual added to the untouched
input: zero latency, exact at 0, aliasing < −40 dBc at full drive) → compressor/expander (Giannoulis, peak envelope,
60 Hz HP detector, attack/release ≥ ½ / 5 periods of the lowest note) → transient shaper (fast/slow envelope
difference, 50–1500 Hz detector) → high cut → stereo stage → make-up + stage level. Every value glides (two one-pole
stages, 15 ms). CPU with every module working: ~1.9 % of one core (Linux CI machine, 48 kHz).

**16b changes** (the user's Live test of 16a):
- **No take audio is kept.** The first solve probes every tool on the takes and saves the sensitivities with the plan
  (state format 2 + the raw zero `Zraw`); after a reopen the knobs and A–E use the fast path (Gauss-Newton steps on
  the saved sensitivities). Re-listening refines.
- **A–E are characters of rising strength**, not strategies: A Polish, B Warm, C Bold, D Vintage, E Driven. Each is a
  push on the qualities (e.g. Warm: Warm +2.5, Steady +1.5; Driven: Bite +2, Punch +1.5, Steady +4) + tool costs +
  a compressor style + **colour floors** (saturation / compression / attack minimums, e.g. Driven sat 0.75, comp 0.5).
  The floors are the character's sound: the result with the floors defines the new zero, and the solver works from
  there (it doesn't fight the colour). Mixing: weighted, the neutral share = 1 − Σ weights. **All at 0 % = no
  character** (the default; was A 70 / C 30).
- **Weak bands:** a band below −40 dB gets a gentle lift (≤ 3 steps); only < −60 dB is "nothing to shape". Air +6
  (+3 if already airy), Weight +6 (+3 if heavy).
- **Texture** = flatness relative to the 1/3-octave envelope (whitened), so EQ doesn't change it.
- Saturation residual high-passed at 30 Hz (2nd order; no sub thump); saturation costs ×31 on noise.
- Strip 706 px; Stage role order Bass, Rhythm, Hook, Lead, Support, Bed, FX; Moves pages on click.
- Bench (10 stems, no character): reach 25–99 %; a full first solve 0.5–3.1 s, each character 0.2–3.5 s (background).

**16c: the rest of the sound tools, Scale, Dyn | Avg** (`modules.h`, `chain.h`, `brain.h`):
- Chain: low cut → EQ → tilt → **resonance tamer** → **dynamic EQ** → saturation → compressor/expander →
  **multiband** → transient shaper → **high smoothing** → **saturation (post)** → high cut → **ambience** → stereo
  stage → make-up + stage level → (Scale > 100 %) ceiling. All zero latency; each new module at 0 is
  exactly the input. (17a: the make-up is corrected live, see below.)
- Every dynamic module computes a *unit* gain (its gain at amount 1, from its detector); applied = amount × unit
  (linear in the amount: Scale and the solver rely on it). **Average** holds amount × the unit's average, measured on
  the takes at the end of every solve (one extra render with every dynamic module at 1) and saved in the plan
  (`P_AVG_*`); no detectors run, the multiband becomes two shelves. Switching glides (no clicks).
- **Dynamic EQ** (tools `T_DWARM`, `T_DBITE`): a bell at 300 Hz / 3 kHz cuts up to 12 dB × amount when its band's
  share of the sound rises above its usual share (the median, measured at Listen: `Profile::dynThr`), full cut 6 dB
  above it. Serves Warm/Bite *down* only.
- **Resonance tamer** (Clean, not a solver tool): up to 4 fixed peaks found at Listen in the long-term spectrum
  (1/24 octave > 5 dB over its octave, 200 Hz–10 kHz, not a partial of ≥ 10 % of the voiced frames); each a notch
  (Q 6) that dips up to 8 dB when a narrow detector (Q 8) rises over a wide one (Q 1) at the same frequency. Depth =
  Clean. Synthetic test: a +15 dB ring at 1.5 kHz in noise found and dipped 6.8 dB, 600 Hz untouched.
- **High smoothing** (`T_SMOOTH`, Texture *down* only): the highs above 1.2 kHz (split by subtraction) have their
  spikes rounded off at once above 1.5 × the highs' usual peak level (a running 80th percentile of 1/3 ms peaks):
  steady partials pass (a clean saw changes by −51 dB), crackle and grit drop. Modest by nature: noise inside a held
  note can't be separated without latency (the later tonal/noisy split).
- **Multiband** (`T_MBLO/MID/HI`, Steady *up*, low band also Weight *down*): lo | mid | hi at 2.5 × the lowest
  content (120–300 Hz) and 2.5 kHz, split by subtraction (the bands always sum to the input: continuous from amount 0;
  an all-pass crossover made the measures jump at the first touch), a compressor per band (ratio 3, knee 6 dB,
  threshold = the band's usual upper level, measured at Listen).
- **Saturation (post)** (`T_SAT2`): the same ADAA saturator after the dynamics; Vintage and Driven lean on it.
- **Ambience** (placement, not a tool): early reflections (6 taps a side, from a plain delay line) + a short dense
  tail: 16 delay lines (19–55 ms, mutually prime) mixed through a 16×16 Hadamard matrix, each with a loss filter so
  the highs die faster (17a; 16c's tail was a single line feeding itself, a comb that rang at fixed pitches for over a
  second). From distance: none in the front half, mix up to 0.4 at the very back; distance changes only the decay
  (~0.15–0.4 s measured) and the darkness, never the line lengths (no zipper while dragging). At the very back the
  room sits about −13 to −15 LU under a sustained sound and −16 to −20 LU under drums (RESEARCH: mixes with reverb
  at −9 LU or more were judged too wet). The tail rings like decaying noise, not at pitches (tested). The back no
  longer asks for more Tail (the room brings it).
- **Live level match** (17a, audio thread only): both sides K-weighted, 100 ms blocks counted only while the input
  sounds; after a new plan 0.4 s of sound give a fresh estimate, then a ~4 s window. The plan's make-up acts at once,
  only the measured correction glides (two 0.25 s stages). Off in the solver's renders (they measure loudness
  themselves); cleared by a pass-through plan, so no takes = exactly the input. Why: the fast path's make-up after a
  character move missed by up to +3.3 / −7 dB on real stems; with the match, within ±1 dB once settled.
- **Scale** (0–400 %, `scale`): multiplies the shaping values (EQ, tilt, both saturations, compressor, expander,
  transients, dynamic EQ, smoothing, multiband) within wider limits (EQ ±24 dB, drive to +26 dB, compressor ratio to
  13), not the placement (cuts, Clean, stage, ambience). The make-up follows up to 100 %; past it a slow level match
  (1.5 s, gated) takes over and a soft ceiling (from −1 dBFS, never over 0 dBFS) fades in by 200 %. Moves show the
  scaled values; reach is still measured at 100 %.
- **Roles don't pan**: a role sets distance, width, edge and range; the pan is the user's.
- **Directions**: `TOOL_FOR` says which way a tool may serve a quality (both / only up / only down); a cut can't be
  used to "add" and the smoothing can't be used for grain.
- **Solver robustness** (with 17 tools): sensitivities are clamped to 15 dB per typical step; Broyden updates only
  from steps of ≥ 0.05 typical steps, in step units; small effects of a tool on a quality it doesn't serve (< 0.75 dB
  per step) are treated as noise in the step; a failed step is tried at ½ and ¼ before the model is blamed; an ask
  the enabled tools can't get near (all their sensitivities × 2 steps < half of it) is reported unreached instead of
  chased; per-step scale never finer than 0.5 dB; λ 0.1. Characters' steadiness pushes halved (Vintage 1.5, Driven
  2.5): already steady sounds can't get much steadier.
- **Speed**: the solve set is 4.5 s; the first solve probes only the tools the ask enables (the plan plays sooner),
  then the rest are probed in the background before the characters. Bench (10 stems): first plan 0.3–4.5 s, then
  ~1–5 s per character (this machine).
- **CPU** (every module working, 48 kHz): 3.2 % of one core (Dynamic), 3.0 % (Average); the test fails above 5 %.
  Saturation was made cheaper (each logcosh computed once, tanh of the bias cached).
- **State**: plans and tools are appended, so 16a/16b sets read as a prefix and play exactly as before; the reading
  gains 9 values (resonances, dynamic EQ shares, multiband levels); older readings get defaults (no resonances).
- Bench (no character / characters): reach 26–97 % / 17–94 %.

**Not yet** (next builds): specificity detection for Identity (Identity↔Fit only scales the role's pull and protects
unasked qualities for now), the Moves pop-out (on/off, amount, method swap); later: the tonal/noisy split (needs
latency); tuning by ear on the stems.

## 16. Rejected / superseded (one line each)

- Device decides the role: guesses wrong; same sound can be lead or background FX.
- Role changes by dragging into a zone: a lead pushed back is still a lead; it would lose its starting point.
- Absolute dials set to the reading / ghost markers / "dial away from reading = target": replaced by relative knobs.
- Stage turning the knobs (even capped): eats the user's travel; the stage shows as an arc instead.
- Exactly 6 dials: Bright mixed harshness with air; Thick mixed weight with mud; no tail, no depth.
- Wide knob and Depth dial: duplicate the stage.
- Character dial (strength of timbre steering): replaced by Identity ↔ Fit and Amount.
- Field back↔front × carry↔drive with landmarks: replaced by the stage (shape whose size/edge mean something).
- Per-dial XY pads, multi-dot "how" pad, Stage puck, 2D character map, Colour knob: too abstract or duplicated the dials.
- Triangle pad to blend three variations: abstract; replaced by a strength slider per variation.
- Blending only two variations: superseded; any mix at any strengths.
- Order as a choice per variation: can't be mixed and would click; fixed order with two slots where needed.
- Blending by audio crossfade: double CPU, comb filtering/phasiness.
- Output true-peak limiter / loudness normalisation per track: mastering tools; colours dynamics.
- Distance by level alone, or by a large level drop: the fader owns level; −3 dB max, brightness and attack do the rest.
- "Input changed" light: not needed; the user re-listens when the part changes.
- Sidechain listening to other tracks: the user hears the mix and steers.
- Motion (character reacting to playing): another device's job; breaks the workflow.
- "Like this" (reference sound as target): not part of the user's workflow.
- Learning the user's taste: hard to do well, not worth it.
- Live matching of takes while playing; saturation as a default move.
- Rule-only planning without testing the plan on the take: can't know if the target was hit.
- gen~ engine: compiles silently to zero; C++ is faster and testable offline (proven in wanderer-synth).

**17b: taming through the reading, shaping only on request** (`profile.h`, `chain.h`, `brain.h`, the view):
- The user's rule: the device tames what is excessive by itself (through Clean); it beautifies only when asked
  (knobs, A–E, the stage's distance). So **roles only place** the sound (distance, width, range): the role quality
  pushes are gone (Lead's Bite/Air, Rhythm's Punch, Bass's Weight…). Distance keeps its cues (back = less presence,
  air, attack; smaller level jumps); 16c's extra steadiness at the front was unintended and is gone.
- **Edge removed** from the stage (the diamond): it duplicated Texture and Steady. Its Live parameter stays, unused.
- **Identity strength** replaces Identity ↔ Fit on the strip (same Live parameter "Identity Fit"; strength = 1 − fit).
  It weighs holding the qualities nobody asked to change (keep weight 0.1 … 0.6), and from 25 % it enables their own
  two-way EQ bands as holders, so a move's side effect is held back (Air +8 on a lead: the other qualities moved 23 %
  less at 100 % than at 0 %). Build 2 adds the sound's specialities on top.
- **Excess** (the reading): what stands above BOTH its neighbours (5/6–2 octaves away) in the sound's own long-term
  spectrum, 2–8 kHz, weighted to 2.5–5.5 kHz; noisy sounds judged against their natural ~3 dB/octave rise; narrow
  rings left to the resonance tamer. Judged only against the sound's own shape, never a typical sound. Variants tried
  and rejected: the average of the sides and a whole-sound tilt fit (both flagged the voice's natural presence).
  On the test sounds: voice, strings, piano, sax, clarinet, trumpet, an evenly bright hat, the jazz mix: nothing found.
- **Clean tames it** (excess tamer, after the resonance tamer): a dynamic bell at the excess, cutting what the band
  stands above its own usual share: at its usual level the excess beyond 1.5 dB, more (1:1, up to 12 dB) when it
  builds. Depth = Clean, and 0 when presence is asked (Bite up from the knob or a character: taming never fights an
  ask). Average mode holds its average. A piercing hat (+9 dB at 4.3 kHz): −1 dB in the 2.5–5.5 kHz zone at Clean
  40 %, −2.5 dB at 100 % (the bell itself −4 dB).
- **A–E faders**: each letter a 26 × 28 px fader (drag up/down, shift fine, double-click 0; a click without moving
  auditions).
- Measured on 11 stems vs 17a: first plan 2.4 → 0.9 s, solve 2.9 → 1.7 s, character reach A 71 → 72, B 71 → 77,
  C 74 → 80, D 66 → 74, E 62 → 67 %.

