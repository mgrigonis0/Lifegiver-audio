# Lifegiver — Handoff

_Last updated: 2026-10-16_

## What it does
A Max for Live audio effect that listens to a sound (up to 4 short takes), reads what it is, and helps the user shape it
for its job in the mix. The user places the sound on a **stage** (role, pan, distance, width, frequency range,
intensity), adds their own **relative knob** turns on top, and blends **characters A–E** (Polish, Warm, Bold, Vintage,
Driven: subtle → strong; all 0 = none); a **Moves**
list shows what it does and the measured result. An assistant, not an autopilot.

## Current state
- **Build 2026-10-16b** (pushed; CI: tests, view check, bench build, mac universal build, release `lifegiver-latest`).
  From the user's 16a Live test (saving and the reading worked): no take audio kept (sensitivities saved instead),
  A–E as characters of rising strength with colour floors, 706 px strip, Moves paging, weak bands shapeable, role
  order Bass→FX (DESIGN §17 "16b changes"). **Waiting for its Live test** (INSTALL.md "This build").
- 16a: the first build that shapes sound (Phases 2, 3, 3b, most of 4 and 5, and 6; BUILD_PLAN status).
- Verified in Live before it (15e): loads, version lines, Amount click-free, two devices get their own ids, **saving
  works** (the cause of 15a–15d: CONVENTIONS "Saving in the Live set").
- Offline: all engine tests pass (pass-through and Amount 0 bit-exact, S-fades, automation and Compare click-free, no
  allocation on the audio thread, plans swap without clicks, no denormals, true pan, saturation even/odd and aliasing
  < −40 dBc, BS.1770 loudness, onsets, the reading, the solver reaching knob turns, exact make-up, Clean variation never
  colours, no saturation on noise, mono safety, Listen → plan → save → reopen plays the exact plan); view check passes;
  CPU ~1.9 % with every module working (Linux CI machine).
- The user's 10 stems (kick, snare, bass, vocal, 2 Juno leads (one heavily widened), pad, guitar, pluck, FX) are the
  bench material: `tools/render bench <dir> <out>` (they live outside the repo; ask the user again if needed). First
  bench (16b, no character): reach 25–99 % per stem; characters 7–97 % (Driven reaches least: its floors are
  strong); not yet judged by ear.
- Docs: `DESIGN.md` (§17 = what 16a does, concretely), `BUILD_PLAN.md`, `RESEARCH.md`, `REVIEW.md`,
  `design/ui-reference/`, mockup https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv (approved v2).

## Structure
- `source/engine/` (header-only C++17, no Max deps):
  `version.h`; `state.h` (number-list codec); `ring.h`; `smooth.h`; `dsp.h` (SVF, one-pole, envelopes, ADAA saturator);
  `fft.h`; `measure.h` (Ref = selections fixed on the input, the 8 measures, loudness, onsets); `capture.h` (Listen on
  the audio thread, preallocated 2^19 frames); `profile.h` (the reading: qualities, pitch (YIN), harmonic, sustained,
  stereo, levels, certainty, words; `solveSetOf` ≤ 6 s); `chain.h` (Plan = flat array, the processing chain);
  `brain.h` (inputs, role presets and pushes, units, tools and `TOOL_FOR`, characters (pushes, costs, styles, floors),
  base plan, the solver, fast path, moves); `processor.h` (audio thread: capture, chain, Amount, Compare, plans by pointer);
  `controller.h` (main thread: parameters, Listen flow via `Host::post`, fast/slow paths, variations in the
  background, saving format 2, view messages as numbers).
- `source/projects/lifegiver_tilde/lifegiver_tilde.cpp`: wrapper + worker thread (jobs → done on the clock).
- `Lifegiver/lifegiver.strip.js`: the whole strip (jsui, ES5, ASCII). `tools/build_device.py` → `.amxd/.maxpat`
  (never hand-edit): 23 hidden Live parameters (live.dial) + State pattr, routing (its docstring).
- `tools/tests/` (`harness.h`, `signals.h` synthetic sounds, `dsp_tests.h`, `engine_test.cpp`), `tools/render.cpp`
  (bench), `tools/wav.h`, `tools/ui/check_view.js` + `render_view.py` + `ascii_js.py`.

## Parameters (Live, all automatable)
Weight Warm Bite Air Texture Punch Tail Steady (−10…10 steps, 0 = as read) · Role (enum) · Stage Pan (−1…1) ·
Distance (0 back … 1 front) · Width (0…1, 0.5 = as is) · Edge (0…1) · Range Lo / Hi (Hz) · Identity Fit (0…1) ·
Amount (%) · Clean (%) · A Polish … E Driven (%) · State (hidden blob). Not parameters: Arm, take removal, Solo, Compare.

## Decisions (and why)
- Design-level: DESIGN §1–§16. Build-level (16a): DESIGN §17. Highlights: measures relative to the sound with the
  selection fixed on the input; one solver for everything; tools only for the qualities they serve; reach reported
  honestly; saturation via ADAA residual (zero latency, exact at 0); stereo stage on an LR split that fades in/out;
  role ranges opened up (the mockup's 9 kHz Lead top would cut a vocal's air); Clean also does mono safety.
- View messages are numbers only (no new Max symbols per update); the view composes the words.
- A reopened set plays its saved plan exactly; knob turns then use the saved sensitivities ("re-listen to refine").

## Rejected approaches
DESIGN §16 for design. Build-level: a dict name in the pattr (15a); a fixed-name or `---` dict; box-level
`parameter_enable` on a pattr (15a–15d); knob values through the command ring; gen~; letting every tool serve every
quality (it "added bite" by cutting everything else, used an expander to change tone); tilt as a counterweight in
every variation (it fought the EQ); a one-pole split for "lows stay put" (it leaked lows into the pan).

## Known issues / next
1. **Live test of 16b** (INSTALL.md "This build"): characters audible and rising A→E, reopen without takes, Air on
   dull sounds, Moves paging, layout; CPU in Live on the user's Intel Mac; Max window lines.
2. Tune by ear with the user on the stems (words' thresholds, role pushes, safe maxima, variation styles).
3. Next modules: dynamic EQ + resonance tamer (Clean), multiband, post-saturation slot, short ambience; the Moves
   pop-out (on/off, amount, method swap); specificities for Identity; Texture − (smoothing).
- Wanderer's State pattr has the same box-level `parameter_enable` mistake (CONVENTIONS has the fix).
- Wanderer's wrapper allocates on the audio thread (resize in perform): never copy that part.
