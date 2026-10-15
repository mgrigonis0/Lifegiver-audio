# Lifegiver — Build plan

_2026-10-15 (revised after `REVIEW.md`). Order of work from design to a playing device. Each phase ends with something
testable. Design: `DESIGN.md`. Pipeline and rules: `CONVENTIONS.md`. Reference implementation of the pipeline:
`mgrigonis0/wanderer-synth` (`devices/wanderer/`: CMake, wrapper, `tools/build_device.py`, `tools/tests/`,
`.github/workflows/wanderer.yml`)._

Rules for every phase: discuss and propose first, build after the user's "go"; one step at a time; simulate and test
offline before asking for a Live test; commit every build, push when the user agrees; plain-language digest to the user,
full detail in the docs.

## Phase 0: UI mockup v2 (UI first; approve before any final build)

Update the mockup (https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv, Design canvas) to the decided design, in the style
of `design/ui-reference/` (PondSynth, Wanderer):
- Listen: Arm, up to 4 takes with ✕ each, "Listen first" state.
- Stage: drag = pan + distance; width stretch; edge handle; range strip; 7 role buttons (Lead · Hook · Support · Rhythm ·
  Bass · Bed · FX) with faint zones; plain-words line (incl. level, 0 to −3 dB).
- Knobs: 8 relative knobs, centre = 0, stage arc on each ring, readout ("reads: soft · stage +1"), certainty dots.
- Moves console: move list (measured result, amount, on/off, method swap, marked overrides); A–E buttons each with a
  strength slider.
- Identity ↔ Fit, Amount, Compare, Clean.
- Decide whether everything fits 169 px or the Moves console pops out (show both if unclear).
Exit: the user approves the mockup.

## Phase 1: pipeline skeleton (copy Wanderer's proven setup)

- `devices/lifegiver/`: `CMakeLists.txt`, `source/engine/` (header-only C++17), `source/projects/lifegiver_tilde/`
  (Max wrapper → external `lifegiver~`), `tools/build_device.py` (generates `.maxpat`/`.amxd`; never hand-edit),
  `tools/tests/` (harness + tests), `INSTALL.md`, CI `.github/workflows/lifegiver.yml` (offline tests on Linux → mac
  universal build → package → release `lifegiver-latest`, `lifegiver-mac.zip`).
- Differences from Wanderer: stereo audio in (`plugin~` → 2 signal inlets); work buffers preallocated in `dsp64` from
  the max vector size (Wanderer resizes on the audio thread: don't copy that); Defined Latency set by `build_device.py`
  (0 unless the oversampler's constant delay needs it).
- Engine at this stage: stereo pass-through with an S-shaped bypass fade, the version string printed at load, and the
  pattr state save/restore (a dummy value).
- Offline tests: pass-through is bit-clean, no clicks on bypass, CPU.
Exit: CI green; Live test 1: device loads, passes audio, version shows, two instances don't crosstalk, **save the set,
reopen, state intact** (not yet verified in Live for Wanderer either).

## Phase 2: offline test bench and yardsticks

- `tools/render`: process WAV files through the engine with given settings (like pond-synth's `tools/render.cpp`).
- Measures as reusable functions: K-weighted loudness (BS.1770), true peak, ERB-band spectrum, spectral centroid/tilt,
  crest factor, transient energy by band, intermodulation, aliasing check (sine sweeps), clicks, stereo correlation and
  mono loss, CPU per block.
- **"Earns its place" yardstick**, fixed before any module: target-hit error (in each quality's own units), artifact
  budget (intermodulation, aliasing, clicks), CPU budget (≤ 3 % of one core per instance, Intel Mac, 48 kHz), plus a
  blind A/B on the stems by the user.
- Test audio: synthetic signals with known answers + a small set of real dry sounds. Ask the user for ~10 short dry
  stems (vocal, synth lead, pad, bass, kick, snare, guitar, pluck, noisy FX, something stereo; ideally a backing vocal).
Exit: measures validated against known signals.

## Phase 3: Listen and analysis (the reading)

- Capture: Arm → ~4 s of non-silent input per take (skip silence but keep decays, stop itself), up to 4 takes, on the
  audio thread into preallocated memory; analysis on the worker thread; takes kept for the solver.
- Profile: F0 and pitch range (lowest note), harmonicity/noisiness, ERB spectrum, the 8 qualities (relative definitions,
  verified on the stems), plucked↔sustained, density, stereo correlation, resonances, specificities (DESIGN §6 proxy),
  and a certainty per value (from notes/onsets heard).
- Tests: synthetic signals with known F0/brightness/attack give the expected readings; real stems give sensible ones;
  certainty vs number of takes measured (checks the 4-take limit).
Exit: readings printed for the test stems, reviewed with the user in plain words.

## Phase 3b: vertical slice (prove the core idea early)

Weight · Warm · Bite · Air only, with EQ + tilt + low cut; one role (Lead); variation A only; closed-loop solve on the
takes; static make-up; fast path (sensitivity table); Moves text with measured results; minimal test UI.
Tests: the solve hits the target on every stem within the yardstick; make-up keeps loudness within ±0.5 LU; no clicks
when the plan changes. Then an offline render (and, if useful, a Live test) for the user to judge.
Exit: the user hears read → target → solve working; lessons go back into DESIGN before more modules.

## Phase 4: modules, one at a time (each with tests and a measured "earns its place")

EQ, tilt and low cut already exist from Phase 3b. Order (A/B honesty first, then the most used):
1. Output: make-up, stage level, Amount (bit-exact at 0 %) and Compare plumbing.
2. Dynamic EQ + resonance tamer.
3. Compressor (feedforward, decoupled peak, soft knee, RMS option, filtered detection, upward/downward, parallel mix).
4. Transient shaper + attack/body split.
5. Crossovers (Linkwitz-Riley, 2–3 bands, phase-coherent) + multiband dynamics.
6. Saturation (ADAA + 2× minimum-phase IIR, even/odd, gain staging, pre and post slots) + band saturation + soft clip +
   HF softening.
7. True stereo pan, mid/side width, decorrelation, mono check.
8. Short ambience.
Each module: unit tests (no clicks, no NaN/denormals, CPU budget, zero added latency), parameter smoothing, plus one
measured improvement per the Phase 2 yardstick.
Exit per module: tests pass; offline renders sent to the user for listening when useful.

## Phase 5: the brain

- Targets: 7 roles and stage position/width/range/edge → target per quality; knob turns added; per-sound safe range,
  gentler at low certainty; Identity ↔ Fit.
- One solver (DESIGN §7): distance to target + measured artifact penalties + identity loss, within limits and the Range;
  closed loop on all takes; fast path + debounced slow path.
- Five variations A–E as tool preferences; method substitution; plain-word labels; merging near-duplicates; "reaches
  70 %" when a character can't reach the target.
- Mixing any number of variations at any strengths, solved to still hit the target.
- Moves list (plain words, measured results, amounts, on/off, method swap, persistent marked overrides).
- Saving (DESIGN §14): profile, plan, overrides, brain version, sensitivity table; reload plays the saved plan exactly.
- Tests: each variation and mix measurably hits the target; safety rules hold on all stems; save → reload → identical
  output.

## Phase 6: device UI

Build the approved mockup in the device (jsui views + Live parameters for automation: knobs, stage x/y/width/edge/range,
Identity ↔ Fit, Amount, variation strengths; not A–E audition clicks or Moves swaps). Saved state in the set.

## Phase 7: Live testing and tuning by ear

Install from the release; test with the user on real material; tune role targets and strategy rules by ear.

## Later (not in the first version)

Tonal/noisy split (latency).
