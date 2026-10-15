# Lifegiver — Design

_2026-10-15. The full design as decided. `HANDOFF.md` holds the current state; this file holds the what and the why.
Theory and sources: `RESEARCH.md`. Build order: `BUILD_PLAN.md`._

## 1. What the device is

A Max for Live audio effect that listens to a sound, reads what it is, and helps the user shape it for the job it has in
the mix. An **assistant, not an autopilot**: it gives an educated starting point and a set of strategies; the user decides
by ear. It must **work**: results over flashiness.

What it is **not** (keep it this way): not a modulation/motion device, not a reverb/delay effect, not a mix analyser.
It hears only its own track; the user hears the mix and steers.

## 2. The three layers

| Layer | What it is | Who drives it |
|---|---|---|
| **Clean** | Technical fixes: resonances, harsh high notes, rumble/DC | Automatic; one amount control |
| **The sound, as read** (8 knobs) | What the sound *is*; the user's direct corrections | Listening sets each knob's zero; the user turns |
| **Stage** | What the sound should *become*: role, place, size, range, intensity | The user; this is the main way to steer |

Plus: **Moves** (what the device is doing about it), **Variations A–E** (five strategies, any two blendable),
**Identity ↔ Fit**, **Amount**, **Compare**.

## 3. Listen (Step 1, unchanged from earlier decisions)

- Listen = **Arm**: press Arm, then play (or it starts at once on running audio). Captures ~4 s of non-silent audio,
  skips silence, stops itself. **Re-listen** any time (e.g. on another part of the song).
- One take for now; designed so more takes can be added later.
- Analysis runs only while listening (CPU); no live tracking.
- It reads the **input**, never the output: it must not chase its own processing.
- High notes: educated guess from the harmonic make-up; level-triggered taming catches what the guess misses.

## 4. The knobs: relative, zero = "as the sound is"

- 8 knobs: **Weight · Warm · Bite · Air** (tone) · **Texture** · **Punch · Tail · Steady** (dynamics).
- Listening does **not** move the knobs. It sets each knob's **zero point** to the reading. Every knob sits in the
  middle at 0 = "as the sound is now"; right = more of that quality, left = less, relative to *this* sound.
  ("Warm +2" = warmer than this sound already is.) The user has the full travel both ways.
- Why relative: the same descriptor means different processing for different sounds and people (Sabin et al.: one
  listener's "warm" for a voice was nearly opposite their "warm" for drums). A fixed knob-to-EQ mapping can't work;
  a step relative to the measured sound can.
- **Range adapts to the sound**: a full turn is scaled to a safe maximum for this sound (an already bright sound has
  less "+Air" room before harshness). No dead zones, no wall of distortion.
- The **reading stays visible**: a small readout per knob of what was heard (e.g. "reads: bright") plus how sure the
  guess is (certainty dots). The user can trust it or correct it.
- Each knob is defined as a **measured quantity relative to the sound** (e.g. Warm ≈ low-mid energy relative to the
  sound's own fundamental and spectrum), not as a fixed EQ shape. Measurement methods: borrow from the Audio Commons
  timbral models (hardness, depth, brightness, roughness, warmth, sharpness, boominess) where they fit.
- Knob offsets **add on top of the stage's target** (stage asks for more bite, Bite −2 → less than the stage would give).
- Width is **not** a knob (it lives on the stage; two controls for one thing would fight). Depth is not a knob (stage).

## 5. Stage (Step 3): the main way to steer

Seen **from above**: back wall at the top, the listener at the bottom. The sound is a shape on it.

- **Drag the shape**: left/right = **pan**; forward/back = **distance** (front = clear, present, tighter level, drier;
  back = softer attack, darker top, more room, smaller level jumps). Research: listeners judge distance mainly by level
  and brightness; reverb ratio is a weak cue, so "back" is not just more reverb.
- **Stretch it sideways** = **width** (mid/side; lows stay mono).
- **Range it owns**: a side strip with low and high handles (frequency, log scale) = which part of the spectrum the
  sound may claim (clear the lows below, ease the highs above). On its own strip because frequency drawn up/down on a
  top-down stage would clash with distance.
- **Edge smooth ↔ jagged** (handle above the shape) = **intensity**: calm/smooth ↔ driven (denser, grittier, held level).
- **Roles**: Lead · Hook · Rhythm · Bed · FX, shown as faint zones on the floor and as buttons. A role drops the shape
  into a sensible start **and changes the device's priorities** (Lead protects clarity/presence; Bed makes room, steadies,
  carves mids; Rhythm protects attacks; FX allows width and colour). The role is the user's choice, never guessed.
- A plain-words line under the stage says what is being asked ("up front: present, clear, tighter level, drier · narrow ·
  owns 110 Hz – 9 k · a little drive").
- Mockup v1 (no pan yet, old absolute dials): https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv (private to the user).

## 6. Identity ↔ Fit

One control. **Fit** = sit nicely in the role (move toward what the role needs). **Identity** = exaggerate what makes this
sound *this* sound. This is what separates the device from an auto-corrector, which pushes everything to an average.
Research: a sound's identity lives in its **specificities** (hollow, metallic, a pinched end), not in shared dimensions.
The reading must therefore detect specificities, not just the 8 qualities. Identity ↔ Fit also decides whether a
spectral peak is character to keep or a flaw to tame (resonance tamer), and applies to all variations.

## 7. How the device decides (where the smartness lives)

1. **What it is** (profile from Listen): the 8 qualities + facts: F0 / pitch range, harmonic ↔ inharmonic/noisy,
   plucked ↔ sustained, sparse ↔ dense, stereo correlation, resonances, specificities, certainty of each.
2. **What it needs** (target): role + stage position/size/range/edge → target values for the same qualities, plus the
   user's knob offsets, pulled toward Fit or Identity.
3. **How to close the gap** (strategy): the same request uses different tools depending on the sound. Examples:
   - More **bite**: clean harmonic synth → add upper harmonics (they fuse). Dense/noisy → dynamic EQ presence (saturation
     would make intermodulation: muddy, metallic). Already harsh → lower what masks the bite instead of boosting.
   - More **weight**: real low fundamental → gentle low lift. No fundamental → low-band harmonics the ear hears as bass
     (an EQ boost would only raise noise).
   - More **punch**: drums → transient shaping in 64 Hz–1 kHz (where punch lives). Pad → slower compression timed from F0
     (fast time constants distort low notes).
   - **Further back**: darker top, softer attack, smaller level jumps, a little room (how the ear judges distance).
4. **Safety rules** (from `RESEARCH.md`): saturation only soft, low-order, antialiased, little/none on inharmonic or dense
   material; compressor time constants longer than the lowest note's period; never erase specificities (unless the user
   goes toward Fit); each change has a limit per sound; output loudness-matched.

## 8. Moves console

Its own small console. Lists the actions the device chose, in plain words ("Adding upper harmonics for bite",
"Steadying level, slow", "Clearing lows below 110 Hz", "Softening attack"). Per move: **amount**, **on/off**, and a
**method swap** where more than one method exists ("bite via EQ instead"). Why: the strategy is visible and changeable;
the main complaint in a user study of an assisted compressor was opacity ("can't see why it chose this").

## 9. Variations A–E, any two blendable

For one goal the device plans **five strategies** that reach the same target with different tool families:

| | Name | Character | Main tools |
|---|---|---|---|
| A | Clean | clearer, no colour | EQ, dynamic EQ, transparent dynamics |
| B | Rich | fuller, warmer | harmonics (even), glue compression, parallel |
| C | Bold | edge, attack, forward | transient shaper, presence dynamic EQ, odd harmonics, attack/body split |
| D | Vintage | rounded, soft top, slow | soft saturation, HF softening, slow compression, low-band harmonics |
| E | Tight | controlled, focused | multiband dynamics, resonance control, soft clip, narrower |

- Calculated **per sound, every time**, not presets. A method that would damage this sound is swapped for one that works
  (e.g. no overtones on a noisy texture). Each variation is **labelled by what it actually does** ("clearer, no colour").
- **Blending: any two** (e.g. A ●────○ B at 30 %). Blending happens **in the plan, not the audio**: all variations use
  the same modules with different amounts, so a blend is one new plan, **re-solved so it still hits the target**
  (not a half-way undershoot). One chain → no extra CPU, no phase/comb problems.
- UI: buttons A–E in the Moves console; click to audition (instant, level-matched); select a second for the blend slider.
  The Moves list shows the active (blended) plan.
- All five are planned in the background (cheap); only the active plan runs audio.

## 10. Overall controls

- **Amount**: strength and A/B in one: 0 % = the original, 100 % = the full plan. Loudness-matched so level never fools.
- **Compare**: level-matched before/after.
- **Clean**: amount of automatic cleanup.

## 11. Under the hood: tools (modules)

Modular: any module can be added or removed later without redesign. Each must **earn its place by a measured
improvement** in offline tests on real audio.

1. **Resonance tamer**: narrow, signal-reactive cuts on spiky peaks (ERB-spaced detection). Identity ↔ Fit decides
   character vs flaw.
2. **Low cut / DC**: below the sound's real lows.
3. **EQ**: shelves and bells (SVF, minimum phase, instant response) for Weight, Warm, Bite, Air.
4. **Dynamic EQ**: bands that act only when needed (harsh loud notes, boomy low notes; de-essing is a case of this).
5. **Tilt**: one gentle slope over the spectrum (natural bright/dark; a distance cue).
6. **Saturation**: soft, low-order, antialiased (ADAA + 2× oversampling). Even = warm, odd = edgy.
7. **Band saturation** (2–3 bands, phase-coherent Linkwitz-Riley crossovers): lows → weight without boom; highs → exciter.
8. **Transient shaper**: attack (Punch) and sustain (Tail), level-independent (fast vs slow envelope difference),
   focused on low-mids.
9. **Compressor**: feedforward, dB-domain smoothing, smooth decoupled peak detector, soft knee, RMS option; time constants
   from F0.
10. **Multiband dynamics** (2–3 bands, same crossovers).
11. **Pan and width**: mid/side, lows mono; mono sources widened by decorrelation (not Haas).
12. **Short ambience**: early reflections + small darkened room, for distance, not as an effect.
13. **Output**: K-weighted loudness match (BS.1770) + true-peak safety.

## 12. Under the hood: methods (how tools are used)

1. **Parallel processing**: every move has a dry/wet mix (New York compression, parallel saturation).
2. **Attack/body split**: process the start of notes and their sustained part separately.
3. **Upward compression and downward expansion**: lift quiet detail (gentler Steady); lower what's between notes.
4. **Soft clipping**: shave the top of peaks for density/control (drums, plucks).
5. **Mid/side shaping**: EQ and saturation differ for centre and sides.
6. **Gain staging into colour**: level into saturation is measured and set, so "a little drive" is the same on any input.
7. **Filtered compressor detection**: the detector ignores deep lows (no pumping).
8. **Order as a choice**: per variation (cut before compressing vs colour after).
9. **HF softening** (tape-style): gentle level-dependent taming of the top.
10. **Mono safety check**: width only as far as the sound holds together in mono.

Consider later (cost: latency): **tonal/noisy split** (harmonic-percussive separation) to process tone and noise
separately (good for Identity and Texture).

Left out on purpose: long reverb, delay, chorus/modulation (effects, other devices); pitch/formant shift and octave-down
synthesis (change what the sound is); gates/noise reduction (repair, not shaping); linear-phase EQ (pre-ringing,
latency, CPU); more than 3 bands (artifacts for little gain).

## 13. Technology

- **C++ engine**, same setup as `wanderer-synth` (see `CONVENTIONS.md`): header-only C++17 with no Max deps, tested
  offline on Linux; thin Max wrapper `lifegiver~`; GitHub Actions builds a universal `.mxo` and publishes the release zip.
- Threads: audio thread = the processing chain (never allocates/locks); main thread = brain (planning, variations,
  blending, Moves); worker thread = analysis after a Listen take. Command/report rings between them.
- The brain lives in C++ (testable), not in ES5 JS. JS is for views only.

## 14. Open questions

- Live's device height is 169 px: keep the stage compact in the strip, or add a pop-out larger window? Decide on mockup v2.
- Exact knob definitions (what each measures, its unit and safe range): decided per knob in the build, with tests.
- Role target tables (what each role/position asks of each quality): first draft in the build, tuned by ear.
- Latency reporting to Live for oversampling/lookahead: verify how the wrapper reports it before adding lookahead.

## 15. Rejected / superseded (one line each)

- Device decides the role: guesses wrong; same sound can be lead or background FX.
- Absolute dials set to the reading / ghost markers / "dial away from reading = target": replaced by relative knobs (zero = reading).
- Exactly 6 dials: Bright mixed harshness with air; Thick mixed weight with mud; no tail, no depth.
- Wide knob and Depth dial: duplicate the stage.
- Character dial (strength of timbre steering): replaced by Identity ↔ Fit and Amount.
- Field back↔front × carry↔drive with landmarks: replaced by the stage (shape whose size/edge mean something).
- Per-dial XY pads, multi-dot "how" pad, Stage puck, 2D character map, Colour knob: too abstract or duplicated the dials.
- Triangle pad to blend three variations: abstract; two at a time covers real use.
- Blending by audio crossfade: double CPU, comb filtering/phasiness.
- Sidechain listening to other tracks: the user hears the mix and steers.
- Motion (character reacting to playing): another device's job; breaks the workflow.
- "Like this" (reference sound as target): not part of the user's workflow.
- Learning the user's taste: hard to do well, not worth it.
- Live matching of several takes while playing; saturation as a default move.
- gen~ engine: compiles silently to zero; C++ is faster and testable offline (proven in wanderer-synth).
