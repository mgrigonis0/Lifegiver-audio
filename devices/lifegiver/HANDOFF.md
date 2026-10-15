# Lifegiver — Handoff

_Last updated: 2026-10-15_

## What it does
A Max for Live audio effect that listens to a sound (up to 4 short takes), reads what it is, and helps the user shape it
for its job in the mix. The user places the sound on a **stage** (role, pan, distance, width, frequency range,
intensity), adds their own **relative knob** turns on top, and mixes **strategies A–E** at any strengths; a **Moves**
console shows what it does and the measured result. An assistant, not an autopilot.

## Current state
- **Phase 0 done:** mockup v2 approved by the user ("good for a first go"): device strip at real 169 px (order Listen →
  Tone · Dynamics → Stage → Moves → Out, ~1200 px wide, all in the strip) + Moves pop-out window.
  https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv (Design canvas, private to the user; v1 kept on it as history).
- **Phase 1, build 2026-10-15b**: stereo pass-through with Amount (Live parameter), saved state (device id), version
  printed by engine and patch, offline tests (all pass, 0 warnings), CI → release `lifegiver-latest`.
- **Live test 1 (15a)**: loads, both version lines, Amount click-free, two devices get their own ids: all OK.
  Save → reopen FAILED (both came back as new devices): the pattr had saved only a dict *name*. 15b stores the state
  itself as a number list in the hidden parameter (`state.h`), asks 3× at load. **15b save → reopen not yet verified.**
- Docs: `DESIGN.md` (full design), `BUILD_PLAN.md`, `RESEARCH.md`, `REVIEW.md` (why the design changed),
  `design/ui-reference/` (PondSynth, Wanderer: the UI style the user likes).

## Structure
- `source/engine/` (header-only C++17, no Max deps): `version.h` (the one version string), `state.h` (StateDoc +
  number-list codec), `ring.h` (SPSC ring), `smooth.h` (two-pole smoother), `processor.h` (audio thread: Amount blend
  + make-up stage), `controller.h` (main thread: messages, load/save logic, Host interface).
- `source/projects/lifegiver_tilde/lifegiver_tilde.cpp`: thin Max wrapper (2 signal inlets, outlets L/R/info,
  getstate/statelist to the patch, 15 ms tick). Works on Max's 64-bit buffers directly: nothing allocated in perform.
- `tools/build_device.py` → `Lifegiver/Lifegiver.amxd/.maxpat` (never hand-edit; type `aaaa` = audio effect; reads
  `version.h`; checks wiring, layout, `---` names, unique parameter names). `tools/tests/` (harness + engine_test).
- `INSTALL.md`; CI `.github/workflows/lifegiver.yml` (tests + generator on Linux → universal mac build, lipo check →
  zip → release).
- Flow: plugin~ → lifegiver~ → plugout~; Amount → `prepend amount`; live.thisdevice → init / version print / controls
  re-output; lifegiver~ "getstate" → pattr bang → `prepend statelist` → lifegiver~; "statelist <numbers>" → [route] →
  [pattr state] (hidden blob parameter, stored in the set).
- Planned chain (fixed order): Clean → EQ/tilt → dynamic EQ → saturation pre → dynamics → saturation post →
  pan/width → short ambience → make-up. Audio = chain + take capture; main = control, state, fast path;
  worker = analysis + closed-loop solver.

## Parameters (Phase 1)
| Name | Range | Purpose |
|---|---|---|
| Amount | 0–100 % (init 100) | Blend: 0 % = the input bit for bit; 100 % = full plan (now: unity) |
| State | hidden blob | The engine's saved state as a number list (format, device id, saving build) |

## Decisions (and why)
- Assistant, not autopilot; the role is the user's choice, never guessed, never changed by dragging.
- Knobs relative to the reading and never moved by the stage: full travel both ways; the stage shows as an arc.
- One solver (target distance + measured artifacts + identity loss); variations = tool preferences, mixes solved to
  still hit the target: one chain, no extra CPU, no comb filtering.
- Closed loop on the takes: the only way to know a plan hit the target; also gives exact static make-up and measured
  safety (e.g. intermodulation before saturating).
- Fast path (sensitivity table) for drags/automation; slow refine on the worker, debounced, smoothed hand-over.
- Fixed module order (order as a choice can't be mixed); two saturation slots where order matters.
- Zero latency (patcher `latency` 0): M4L latency is fixed, so every plan must have the same latency.
- No output limiter; level = static make-up + stage level (0 to −3 dB); the fader owns the rest.
- Saved: reading, plan, overrides, brain version, sensitivity table (not takes); reload plays the saved plan exactly.
- Engine on doubles, straight on Max's buffers: bit-exact dry path, no conversion buffers to allocate.
- Amount/make-up reach the audio thread as atomics (a ring could drop the newest value while audio is stopped).
- First values before audio starts apply at once (a set saved at 0 % never fades in processed sound on load).
- Smoothing = two one-pole stages, 4 ms each (S-shaped, ≈ 13 ms 90→10 %, exact after ≈ 77 ms): stays smooth when
  automation retargets mid-glide, which a smoothstep ramp does not.
- Amount is a Live parameter, so Live saves it; the engine state holds only what Live can't (and skips identical
  saves: no extra undo steps).
- Saved state = a number list inside the hidden parameter (magic + codec + entries, text as char codes): follows the
  device, no shared names, no symbol per save; malformed lists are refused whole. Load logic lives in the controller
  (asks at 0/300/800 ms, new device at 1.5 s, nothing saved before) so it is tested offline.
- C++ engine like wanderer-synth (offline-testable, no silent gen~ failures); the brain lives in C++.
- Earlier decisions still in force: input only, analysis only on takes; ERB-spaced analysis; soft antialiased low-order
  saturation; reference compressor design with time constants from the lowest F0.

## Rejected approaches
Full list with reasons: DESIGN.md §16. Most likely to be re-proposed: sidechain listening, Motion, "like this" reference,
taste learning, stage turning the knobs, role change by drag zones, order as a choice, output limiter, "input changed"
light, audio-crossfade blending, triangle blend pad, gen~, sending knob values through the command ring, saving a dict
name in the pattr (15a: lost on reload), a fixed-name or `---` dict for the state.

## Known issues
- 15b save → reopen not yet verified in Live. Open: does Live keep the list at full precision (the codec's own numbers
  survive 32-bit rounding; later plan values may need checking).
- Sets saved with 15a hold a dict name: 15b treats them as new devices (tested) and saves properly from then on.
- Wanderer's wrapper allocates on the audio thread (resize in perform), and its saving uses the 15a dict-name pattern
  (likely broken in Live the same way): never copy those parts. Not fixed there (other repo).

## Next step
Live test 2 (15b): save → close → reopen must say `state restored, id ...` with the same ids; recheck that both devices
keep their own ids after reopening. Then Phase 2
(offline test bench and yardsticks): ask the user for ~10 short dry stems (vocal, synth lead, pad, bass, kick, snare,
guitar, pluck, noisy FX, something stereo; ideally a backing vocal).
