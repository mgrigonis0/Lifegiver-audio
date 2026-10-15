# Lifegiver — Design

_2026-10-15 (revised after the pre-build review, `REVIEW.md`). The full design as decided. `HANDOFF.md` holds the
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
- Level, in two parts: **make-up** (static, from the closed loop) removes any level change the processing causes by
  accident; **intended level** is only the stage's distance (0 to about −3 dB), shown in the plain-words line. No
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
13. **Output**: static make-up + stage level; K-weighted loudness (BS.1770) is the measuring method.

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
