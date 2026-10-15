# Lifegiver — Build plan

_2026-10-15. Order of work from design to a playing device. Each phase ends with something testable. Design: `DESIGN.md`.
Pipeline and rules: `CONVENTIONS.md`. Reference implementation of the pipeline: `mgrigonis0/wanderer-synth`
(`devices/wanderer/`: CMake, wrapper, `tools/build_device.py`, `tools/tests/`, `.github/workflows/wanderer.yml`)._

Rules for every phase: discuss and propose first, build after the user's "go"; one step at a time; simulate and test
offline before asking for a Live test; commit every build, push when the user agrees; plain-language digest to the user,
full detail in the docs.

## Phase 0: UI mockup v2 (UI first; approve before any final build)

Update the mockup (https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv, Design canvas) to the decided design:
- Stage: add pan (drag left/right); keep depth, width handles, range strip, edge, role buttons/zones, plain-words line.
- Knobs: 8 relative knobs, centre = 0, with the reading readout and certainty dots; Wide removed.
- Moves console: move list (amount, on/off, method swap), A–E buttons, blend slider between two.
- Identity ↔ Fit, Amount, Compare, Clean.
- Decide the 169 px question: compact in the strip vs pop-out window (show both if unclear).
Exit: the user approves the mockup.

## Phase 1: pipeline skeleton (copy Wanderer's proven setup)

- `devices/lifegiver/`: `CMakeLists.txt`, `source/engine/` (header-only C++17), `source/projects/lifegiver_tilde/`
  (Max wrapper → external `lifegiver~`), `tools/build_device.py` (generates `.maxpat`/`.amxd`; never hand-edit),
  `tools/tests/` (harness + tests), `INSTALL.md`, CI `.github/workflows/lifegiver.yml` (offline tests on Linux → mac
  universal build → package → release `lifegiver-latest`, `lifegiver-mac.zip`).
- Engine at this stage: stereo pass-through with an S-shaped bypass fade and the version string printed at load.
- Offline tests: pass-through is bit-clean, no clicks on bypass, CPU.
Exit: CI green; Live test 1: device loads, passes audio, version shows, two instances don't crosstalk.

## Phase 2: offline test bench

- `tools/render`: process WAV files through the engine with given settings (like pond-synth's `tools/render.cpp`).
- Measures as reusable functions: K-weighted loudness (BS.1770), true peak, ERB-band spectrum, spectral centroid/tilt,
  crest factor, transient energy by band, aliasing check (sine sweeps), clicks, stereo correlation, CPU per block.
- Test audio: synthetic signals with known answers + a small set of real dry sounds. Ask the user for ~10 short dry
  stems (vocal, synth lead, pad, bass, kick, snare, guitar, pluck, noisy FX, something stereo).
Exit: measures validated against known signals.

## Phase 3: Listen and analysis (the reading)

- Capture: Arm → ~4 s of non-silent input (skip silence, stop itself) on the audio thread into a preallocated buffer;
  analysis on a worker thread.
- Profile: F0 and pitch range, harmonicity/noisiness, ERB spectrum, the 8 qualities (relative definitions; borrow Audio
  Commons methods where they fit), plucked↔sustained, density, stereo correlation, resonances, specificities, and a
  certainty per value.
- Tests: synthetic signals with known F0/brightness/attack give the expected readings; real stems give sensible ones.
Exit: readings printed for the test stems, reviewed with the user in plain words.

## Phase 4: modules, one at a time (each with tests and a measured "earns its place")

Order (A/B honesty first, then the most used):
1. Output: loudness match + true-peak safety; Amount and Compare plumbing.
2. EQ (SVF shelves/bells) + tilt + low cut/DC.
3. Dynamic EQ + resonance tamer.
4. Compressor (feedforward, decoupled peak, soft knee, RMS option, filtered detection, upward/downward, parallel mix).
5. Transient shaper + attack/body split.
6. Crossovers (Linkwitz-Riley, 2–3 bands, phase-coherent) + multiband dynamics.
7. Saturation (ADAA + 2×, even/odd, gain staging) + band saturation + soft clip + HF softening.
8. Pan, mid/side width, decorrelation, mono check.
9. Short ambience.
Each module: unit tests (no clicks, no NaN/denormals, CPU budget), parameter smoothing, plus one measured improvement.
Exit per module: tests pass; offline renders sent to the user for listening when useful.

## Phase 5: the brain

- Targets: role and stage position/width/range/edge → target per quality; knob offsets added; relative range scaling per
  sound; Identity ↔ Fit.
- Strategy rules per sound type (DESIGN.md §7), safety rules, limits.
- Five variations A–E planned per sound; method substitution when a method would damage the sound; plain-word labels.
- Plan-space blending of any two, re-solved to still hit the target.
- Moves list (plain words, amounts, on/off, method swap) as a report to the UI.
- Tests: each variation measurably moves the sound toward the target; blends hit the target; safety rules hold on all
  test stems.

## Phase 6: device UI

Build the approved mockup in the device (jsui views + Live parameters for automation): Listen, stage, knobs with
readouts, Moves console with A–E and blend, Identity ↔ Fit, Amount, Compare, Clean. Saved state in the set.

## Phase 7: Live testing and tuning by ear

Install from the release; test with the user on real material; tune role targets and strategy rules by ear.

## Later (not in the first version)

Tonal/noisy split (latency), more Listen takes.
