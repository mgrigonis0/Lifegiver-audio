# Lifegiver — Handoff

_Last updated: 2026-10-15_

## What it does
A Max for Live audio effect that listens to a sound (up to 4 short takes), reads what it is, and helps the user shape it
for its job in the mix. The user places the sound on a **stage** (role, pan, distance, width, frequency range,
intensity), adds their own **relative knob** turns on top, and mixes **strategies A–E** at any strengths; a **Moves**
console shows what it does and the measured result. An assistant, not an autopilot.

## Current state
- Design phase **complete and reviewed**; nothing built (no engine, wrapper, patch or CI yet).
- Full design: `DESIGN.md`. Build order: `BUILD_PLAN.md`. Theory + sources: `RESEARCH.md`. Why the design changed in
  the review: `REVIEW.md` (resolved).
- Mockup v1 is outdated (no pan, old absolute dials, no Moves console):
  https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv (Design canvas, private to the user).
- UI style reference the user likes: `design/ui-reference/` (PondSynth, Wanderer).

## Structure (planned, see BUILD_PLAN.md Phase 1)
- `source/engine/`: header-only C++17 engine (modules, analysis, solver), no Max deps.
- `source/projects/lifegiver_tilde/`: thin Max wrapper, external `lifegiver~` (stereo in, dry path inside).
- `tools/build_device.py` (generates `.amxd`, sets Defined Latency), `tools/tests/`, `tools/render` (offline WAV renders).
- CI `.github/workflows/lifegiver.yml` → release `lifegiver-latest` (`lifegiver-mac.zip`). Copy Wanderer's setup.
- Flow (fixed order): Clean → EQ/tilt → dynamic EQ → saturation pre → dynamics → saturation post/HF softening →
  pan/width → short ambience → make-up. Audio thread = chain + take capture; main = control, state, fast path;
  worker = analysis + closed-loop solver.

## Controls (detail in DESIGN.md)
| Control | Purpose |
|---|---|
| Arm / takes ×4 | ~4 s of non-silent input per take, each removable; sets every knob's zero |
| Weight, Warm, Bite, Air, Texture, Punch, Tail, Steady | 0 = as read; turn = the user's extra on top of the stage; stage arc on the ring |
| Stage shape | Drag: pan (true stereo) / distance (back = darker, softer, up to −3 dB); stretch: width; edge: smooth ↔ driven |
| Range strip | Hard low/high limit of the spectrum the sound may own |
| Role: Lead, Hook, Support, Rhythm, Bass, Bed, FX | Start position + priorities; changes only by button |
| Identity ↔ Fit | Exaggerate what makes the sound itself ↔ fit the role |
| A–E + strength each | Five strategies (Clean, Rich, Bold, Vintage, Tight), mixed at any strengths |
| Moves console | Chosen actions, measured result, amount, on/off, method swap |
| Amount, Compare, Clean | 0 % = bit-exact original; loudness-matched before/after; cleanup amount |

## Decisions (and why)
- Assistant, not autopilot; the role is the user's choice, never guessed, never changed by dragging.
- Knobs relative to the reading and never moved by the stage: full travel both ways; the stage shows as an arc.
- One solver (target distance + measured artifacts + identity loss); variations = tool preferences, mixes solved to
  still hit the target: one chain, no extra CPU, no comb filtering.
- Closed loop on the takes: the only way to know a plan hit the target; also gives exact static make-up and measured
  safety (e.g. intermodulation before saturating).
- Fast path (sensitivity table) for drags/automation; slow refine on the worker, debounced, smoothed hand-over.
- Fixed module order (order as a choice can't be mixed); two saturation slots where order matters.
- Zero latency: M4L latency is fixed in the patcher, so every plan must have the same latency.
- No output limiter; level = static make-up + stage level (0 to −3 dB); the fader owns the rest.
- Saved: reading, plan, overrides, brain version, sensitivity table (not takes); reload plays the saved plan exactly.
- C++ engine like wanderer-synth (offline-testable, no silent gen~ failures); the brain lives in C++.
- Earlier decisions still in force: input only, analysis only on takes; ERB-spaced analysis; soft antialiased low-order
  saturation; reference compressor design with time constants from the lowest F0.

## Rejected approaches
Full list with reasons: DESIGN.md §16. Most likely to be re-proposed: sidechain listening, Motion, "like this" reference,
taste learning, stage turning the knobs, role change by drag zones, order as a choice, output limiter, "input changed"
light, audio-crossfade blending, triangle blend pad, gen~.

## Known issues
- None (nothing built). Wanderer's wrapper allocates on the audio thread (resize in perform): don't copy that.
- Open: whether the full UI fits 169 px or the Moves console pops out (mockup v2).

## Next step
Phase 0 of BUILD_PLAN.md: mockup v2 in the style of `design/ui-reference/`, with every control in the table above; settle
the 169 px question; get the user's approval. Then Phase 1 (skeleton from wanderer-synth, incl. a save/reopen Live test).
Ask the user for ~10 short dry test stems before Phase 2.
