# Lifegiver — Handoff

_Last updated: 2026-10-16 (build 2026-10-16c)_

## What it does
A Max for Live audio effect that listens to a sound (Arm, up to 4 short takes), reads what it is, and shapes it for its
job in the mix. The user places it on a **stage** (role, pan, distance, width, edge, frequency range), turns relative
**quality knobs** on top, and blends **characters A–E** (Polish, Warm, Bold, Vintage, Driven: subtle → strong, all 0 =
none). **Moves** lists what it does and how much of the ask it reached. **Amount** blends with the original, **Scale**
(0–400 %) multiplies the processing, **Dyn | Avg** lets the dynamic tools follow the sound or hold their average.

## Current state
- Working offline (all tests pass, CI green, release `lifegiver-latest`): the whole chain, the solver, characters,
  Scale, Dyn | Avg, saving/reopening without takes, the strip view (706 × 169).
- Verified in Live: loading, version lines, saving (15e); 16a's reading/plan restore. **16b and 16c are not yet
  tested in Live** — the user is testing 16c now (INSTALL.md "This build (2026-10-16c)", it folds in 16b's checks).
- Not started ("build 2" of the user's two-build plan): the Moves pop-out (on/off, amount, method swap per move;
  needs a mockup first) and Identity specificities (what's special about a sound is protected). Then tuning by ear.
- Later (needs latency): the tonal/noisy split.

## Structure
- `source/engine/` header-only C++17, no Max deps: `dsp.h` (SVF, saturator), `modules.h` (dynamic bell, high
  smoothing, multiband, ambience), `chain.h` (Plan = flat array; the chain; Scale; Average; level match + ceiling),
  `measure.h` (8 measures, loudness, onsets), `profile.h` (the reading, resonances, the dynamic modules' usual
  levels), `brain.h` (stage/characters → targets; one solver; tools/directions `TOOL_FOR`; fast path; moves),
  `processor.h` (audio thread), `controller.h` (main thread, Listen flow, worker jobs, saving), `capture.h`, `state.h`.
- `source/projects/lifegiver_tilde/lifegiver_tilde.cpp`: Max wrapper + worker thread.
- `Lifegiver/lifegiver.strip.js`: the whole view (jsui, ES5, ASCII). `tools/build_device.py` → `.amxd/.maxpat`
  (never hand-edit).
- Tests: `tools/tests/engine_test.cpp` (+ `dsp_tests.h`, `module_tests.h`; `engine_test modules|brain` runs a part),
  `tools/ui/check_view.js` (+ `render_view.py` → PNG). Bench: `tools/render bench <stems> <out>` on the user's 10 stems
  (outside the repo; ask for them again).
- Flow: Listen → analyse (worker) → solve (asked tools probed) → plan plays → remaining tools probed → 5 characters
  solved in the background. Knob/stage/character changes: fast step on the saved sensitivities, re-solve 0.4 s later
  if takes exist. Saved: reading, last solve (sensitivities), playing plan + inputs; never audio.

## Parameters (Live, appended over builds)
| Name | Range | Purpose |
|------|-------|---------|
| Weight … Steady (8) | −10…10 steps | the user's turn per quality; 0 = as read; a full turn = this sound's safe max |
| Role | Lead…FX | stage preset: distance, width, edge, range (never pan) |
| Stage Pan / Distance / Width / Edge | −1…1 / 0…1 / 0…1 / 0…1 | placement; distance also brings level, darkness and room |
| Range Lo / Hi | 20 Hz–20 kHz | the band the sound may own (cuts) |
| Identity Fit | 0…1 | how hard the role pulls; Identity end protects unasked qualities |
| Amount | 0–100 % | dry/processed blend (0 = bit-exact input) |
| Clean | 0–100 % | rumble cut, mono safety, resonance tamer depth |
| A Polish … E Driven | 0–100 % | character strengths, blendable |
| Scale | 0–400 % | multiplies the shaping, not the placement; >100 % level-matched + soft ceiling |
| Dynamics | Dynamic / Average | dynamic tools follow the sound or hold their measured average |

## Decisions (and why)
- Measures are relative to the sound, selections fixed on the input take: a change is the processing, not the window.
- One closed-loop solver (renders the takes, measures) for everything; reach is reported honestly, never faked.
- Tools serve only listed qualities and directions (`TOOL_FOR`): no "more bite" by cutting the rest, no grain from
  the smoother, no warmth from a dynamic cut.
- Small off-role sensitivities (< 0.75 dB per step) are noise; sensitivities clamped to 15 dB/step; Broyden only from
  real steps; failed steps tried at ½, ¼; asks the tools can't approach are reported unreached instead of chased.
- Every dynamic module: gain = amount × unit (linear in amount) — Scale and the solver rely on it; Average holds the
  unit's average measured at solve time (`P_AVG_*`), no detectors, multiband as two shelves.
- Splits by subtraction (multiband, smoothing): bands sum to the input, so amounts are continuous from 0.
- Characters = pushes + costs + colour floors; the floors define the new zero, so the solver doesn't fight the colour.
- Saturation is an ADAA residual added to the dry signal: zero latency, exact at 0. Everything zero latency.
- View messages are numbers only (no new Max symbols per update); the view composes the words.
- Old sets: plans/tools/parameters are append-only (CONVENTIONS), 16a/16b sets play exactly as saved.

## Rejected approaches
- Saving take audio: clutters the user's computer; the sensitivities are saved instead.
- All-pass (Linkwitz-Riley) multiband crossover: measures jumped at the first touch, the solver chased it.
- Control-rate spike softener / plain soft clip for smoothing: too late for clicks / touched clean partials.
- Unbounded Broyden updates: sensitivities of hundreds of dB per dB.
- Letting every tool serve every quality; masking sensitivities fully by `TOOL_FOR` (stalled the solver).
- Tilt as a counterweight in every character; a one-pole split for "lows stay put" (leaked lows into the pan).
- Roles setting the pan: the pan is the user's call. gen~; knob values through the command ring.

## Known issues
- Reach on the stems: no character 26–97 %, characters 17–94 %; D Vintage and E Driven reach least (strong floors,
  steadiness asks on already steady sounds). Tune by ear.
- Smoothing is modest: it can't separate steady noise inside a held note without latency.
- First plan 0.3–4.5 s after Listen on CI; maybe 2× on the user's 2015 Intel Mac (8 GB).
- CPU every module on: ~3.2 % of a core (CI), Average ~3.0 %; test fails above 5 %. Real numbers from Live pending.
- Wanderer's pattr/wrapper mistakes (box-level `parameter_enable`, allocation in perform): never copy them.

## Next step
1. Get the user's 16c Live results (INSTALL.md list: characters A→E, Scale 300–400 %, Dyn|Avg clicks, pan kept,
   room at the back, reopen, CPU with 4–6 instances, Max window lines). Fix what they report.
2. Build 2: propose the Moves pop-out as a mockup (artifact) → approval → build (`lifegiver.strip.js`,
   `controller.h` messages, `brain.h` per-move overrides); and Identity specificities (`profile.h`, `brain.h`).
   Open question: how a move's "method swap" is chosen and shown.
3. Then tune by ear on the stems (`tools/render bench`).
Process (CONVENTIONS): short plain digests; propose first, build after "go"; mockup before UI; offline tests first;
commit every build, the user's go means push.
