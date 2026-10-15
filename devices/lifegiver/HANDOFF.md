# Lifegiver — Handoff

_Last updated: 2026-10-15_

## What it does
A Max for Live audio effect that listens to a sound for a few seconds, reads what it is, and helps the user shape it for
its job in the mix. The user places the sound on a **stage** (role, pan, distance, width, frequency range, intensity),
corrects the reading with **relative knobs**, and picks or blends **strategies (A–E)** the device plans; a **Moves**
console shows what it does. An assistant, not an autopilot.

## Current state
- Design phase **complete**; nothing built (no engine, wrapper, patch or CI yet).
- Full design: `DESIGN.md`. Build order: `BUILD_PLAN.md`. Theory + sources: `RESEARCH.md`.
- Mockup v1 exists but is outdated (no pan, old absolute dials, no Moves console):
  https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv (Design canvas, private to the user).

## Structure (planned, see BUILD_PLAN.md Phase 1)
- `devices/lifegiver/source/engine/`: header-only C++17 engine (modules, analysis, brain), no Max deps.
- `devices/lifegiver/source/projects/lifegiver_tilde/`: thin Max wrapper, external `lifegiver~`.
- `devices/lifegiver/tools/build_device.py` (generates `.amxd`), `tools/tests/`, `tools/render` (offline WAV renders).
- CI `.github/workflows/lifegiver.yml` → release `lifegiver-latest` (`lifegiver-mac.zip`). Copy Wanderer's setup.
- Flow: input → Clean → tone → harmonics → dynamics → space → output (loudness-matched). Audio thread = chain;
  main thread = brain; worker = analysis after a Listen take.

## Controls (detail in DESIGN.md)
| Control | Purpose |
|---|---|
| Arm / Re-listen | ~4 s take of non-silent input; sets every knob's zero |
| Weight, Warm, Bite, Air, Texture, Punch, Tail, Steady | Relative: 0 = as read; ± = more/less of it for this sound; range scaled per sound |
| Stage shape | Drag: pan (L/R), distance (front/back); stretch: width; edge: smooth ↔ driven |
| Range strip | Low/high limit of the spectrum the sound may own |
| Role: Lead, Hook, Rhythm, Bed, FX | Start position + the device's priorities |
| Identity ↔ Fit | Exaggerate what makes the sound itself ↔ fit the role |
| A–E + blend | Five strategies (Clean, Rich, Bold, Vintage, Tight); blend any two |
| Moves console | Chosen actions in plain words: amount, on/off, method swap |
| Amount | Strength and A/B: 0 % = original |
| Compare, Clean | Level-matched before/after; cleanup amount |

## Decisions (and why)
- Assistant, not autopilot; the role is the user's choice, never guessed (the same sound can be lead or background FX).
- Relative knobs, zero = the reading: descriptors mean different processing per sound and per person (RESEARCH §Descriptors).
- The stage is the main way to steer; knob offsets add on top of the stage target.
- Smartness = per-sound strategy: the same request uses different tools by sound type (DESIGN §7), within safety rules.
- Identity ↔ Fit separates it from an auto-corrector (identity lives in a sound's specificities).
- Five variations blended in the plan, re-solved to hit the target: one chain, no extra CPU, no comb filtering.
- Moves console: visible, changeable strategy (opacity was the main complaint about assisted compressors).
- Depth and width live on the stage, not as knobs (no duplicate controls). Distance = level + brightness cues, not reverb.
- Modular engine: 13 tools + 10 methods (DESIGN §11–12); each must earn its place with a measured offline improvement.
- C++ engine like wanderer-synth: faster, testable offline, no silent gen~ failures; the brain lives in C++ too.
- Earlier decisions still in force: Listen = Arm, ~4 s, skips silence, input only, analysis only while listening,
  one take for now; ERB-spaced analysis; soft antialiased low-order saturation, little/none on dense material;
  reference compressor design with time constants from F0; output loudness-matched.

## Rejected approaches
Full list with reasons: DESIGN.md §15. Most likely to be re-proposed: sidechain listening (user hears the mix), Motion
(another device's job), reference "like this" and taste learning (not the workflow), absolute dials or ghost markers,
Wide/Depth knobs, triangle blend pad, audio-crossfade blending, gen~.

## Known issues
- None (nothing built). Open: 169 px device height vs stage size (compact vs pop-out window).

## Next step
Phase 0 of BUILD_PLAN.md: update the mockup to v2 (pan, 8 relative knobs with readouts, Moves console with A–E and
blend, Identity ↔ Fit, Amount, Compare, Clean) and settle the 169 px question; get the user's approval. Then Phase 1
(pipeline skeleton copied from wanderer-synth). Ask the user for ~10 short dry test stems before Phase 2.
