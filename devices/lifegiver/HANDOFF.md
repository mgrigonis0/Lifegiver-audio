# Lifegiver — Handoff

_Last updated: 2026-10-17 (build 2026-10-17a)_

## What it does
A Max for Live audio effect that listens to a sound (Arm, up to 4 short takes), reads what it is, and shapes it for its
job in the mix. The user places it on a **stage** (role, pan, distance, width, edge, frequency range), turns relative
**quality knobs** on top, and blends **characters A–E** (Polish, Warm, Bold, Vintage, Driven: subtle → strong, all 0 =
none). **Moves** lists what it does and how much of the ask it reached. **Amount** blends with the original, **Scale**
(0–400 %) multiplies the processing, **Dyn | Avg** lets the dynamic tools follow the sound or hold their average,
**Gain** (±12 dB) is the user's trim after everything. The output always matches the input's loudness.

## Current state
- Working offline (all tests pass, view check passes): the whole chain, solver, characters, Scale, Dyn | Avg, saving,
  the strip view (706 × 169), and 17a's new room, live level match and Gain bar.
- Verified in Live: loading, version lines, saving (15e), 16a's restore. The user heard 16c (UI liked; found the
  ringing room and A–E getting louder: both fixed in 17a). **17a is not yet tested in Live** (INSTALL.md "This build").
- Not started ("build 2"): the Moves pop-out (on/off, amount, method swap per move; mockup first) and Identity
  specificities (what's special about a sound is protected). Then tuning by ear.
- Later (needs latency): the tonal/noisy split.

## Structure
- `source/engine/` header-only C++17, no Max deps: `dsp.h`, `modules.h` (dynamic bell, high smoothing, multiband,
  ambience), `chain.h` (Plan = flat array; the chain; Scale; Average; `LevelMatch`; ceiling), `measure.h`,
  `profile.h` (the reading), `brain.h` (stage/characters → targets; solver; `TOOL_FOR`; fast path; moves),
  `processor.h` (audio thread: Amount, Compare, Gain), `controller.h` (main thread), `capture.h`, `state.h`.
- `source/projects/lifegiver_tilde/lifegiver_tilde.cpp`: Max wrapper + worker thread.
- `Lifegiver/lifegiver.strip.js`: the whole view (jsui, ES5, ASCII). `tools/build_device.py` → `.amxd/.maxpat`.
- Tests: `tools/tests/engine_test.cpp` (+ `dsp_tests.h`, `module_tests.h`; parts: `engine_test modules|brain|level`),
  `tools/ui/check_view.js` (+ `render_view.py` → PNG). Bench: `tools/render bench <stems> <out>` expects the user's 10
  stems (gone; the user doesn't have them). 17a was checked on CC-licensed recordings from `librosa/data` (drum+bass,
  jazz mix, strings, voice, snare, and the separate sax/clarinet/trumpet/piano/strings tracks of `drese-midi`),
  converted to WAV outside the repo: never commit them.
- Flow: Listen → analyse (worker) → solve → plan plays → remaining tools probed → 5 characters solved in the
  background. Knob/stage/character changes: fast step, re-solve 0.4 s later if takes exist. Saved: reading, last solve,
  playing plan + inputs; never audio.

## Parameters (Live, appended over builds)
| Name | Range | Purpose |
|------|-------|---------|
| Weight … Steady (8) | −10…10 steps | the user's turn per quality; 0 = as read |
| Role | Lead…FX | stage preset: distance, width, edge, range (never pan) |
| Stage Pan / Distance / Width / Edge | −1…1 / 0…1 / 0…1 / 0…1 | placement; distance also brings level, darkness, room |
| Range Lo / Hi | 20 Hz–20 kHz | the band the sound may own (cuts) |
| Identity Fit | 0…1 | how hard the role pulls |
| Amount | 0–100 % | dry/processed blend (0 = bit-exact input) |
| Clean | 0–100 % | rumble cut, mono safety, resonance tamer depth |
| A Polish … E Driven | 0–100 % | character strengths, blendable |
| Scale | 0–400 % | multiplies the shaping, not the placement; >100 % soft ceiling |
| Dynamics | Dynamic / Average | dynamic tools follow the sound or hold their average |
| Gain (17a) | −12…12 dB | user trim, the very last stage, on both sides of Compare; 0 dB = exactly ×1 |

## Decisions (and why)
- Measures relative to the sound; one closed-loop solver; reach reported honestly; tools serve only listed qualities
  (`TOOL_FOR`). Every dynamic module linear in its amount. Splits by subtraction. Everything zero latency.
- **Live level match** (`LevelMatch` in `chain.h`): the plan's make-up acts at once; the chain measures in/out
  (K-weighted, gated 100 ms blocks) and glides only the correction on top. Why: the fast path's make-up missed by up
  to +3.3 / −7 dB after a character move (16c); now within ±1 dB once settled. Gliding the whole make-up instead made
  a −7 dB dip for half a second. Off in solver renders; cleared by a pass-through plan; measured before the ceiling
  (measuring after it would chase the ceiling).
- **Room** = early reflections + 16-line Hadamard FDN with loss filters; line lengths fixed, distance changes only
  decay and darkness (no re-tuning while dragging). Level at the very back −13 to −15 LU under sustained sounds,
  −16 to −20 LU under drums (RESEARCH: −9 LU judged too wet). Sleeps 1.5 s after the last ask (CPU), wakes from
  empty lines.
- Pleasantness is a brake on *added* harm, never a target (no sameness: RESEARCH "Not sameness").
- Old sets: plans/parameters append-only; 16c sets play the new room and are level-matched (intended).

## Rejected approaches
- Saving take audio; all-pass multiband crossover; control-rate spike softener; unbounded Broyden; every tool serving
  every quality; tilt in every character; one-pole low split; roles setting the pan; gen~.
- Room as one line feeding itself (16c): a comb, rang 20–40 dB at fixed pitches for > 1 s. 8 lines: still some
  peaks (too few modes for the decay); 16 is where the tail measures like noise.
- **Pleasantness guard** (a solver cost against harsh-zone side effects): measured on 10 stems, non-edgy asks change
  the 2.5–5.5 kHz share by ≤ ±0.8 dB; `TOOL_FOR` already prevents it. Not built.
- **Key/scale selector** for unheard notes: a key doesn't say how low you'll play; resonances already skip the notes'
  partials and the tamer is dynamic. **Lowest-note guard / setting**: parked until a real complaint (Clean and Range
  Lo already cover it; listen again for a much lower part).

## Known issues
- Notes much lower than the take can be thinned (rumble cut, weight, low band split are placed from the lowest note
  heard). Workaround: Clean down or listen again.
- Punch +5 adds +1.2…+1.8 dB in the 2.5–5.5 kHz zone on drums/snare/sax (the transient shaper isn't band-limited).
  Tune when tuning by ear.
- After a big move on the fast path the level settles within ~1 s (correction measured afresh).
- Reach: D Vintage and E Driven reach least. Smoothing modest without latency. First plan 0.3–4.5 s after Listen.
- CPU every module on: ~4.5 % of a core here (busy plan incl. room; 16c was ~4.2 %); test fails above 5 %.
- Wanderer's pattr/wrapper mistakes (box-level `parameter_enable`, allocation in perform): never copy them.

## Next step
1. The user's 17a Live results (INSTALL.md list: room on drums at the back, A–E level, Gain, a 16c set reopened,
   CPU with 4–6 instances, leftovers from 16c). Fix what they report.
2. Build 2: Moves pop-out mockup (artifact) → approval → build (`lifegiver.strip.js`, `controller.h`, `brain.h`
   per-move overrides); Identity specificities (`profile.h`, `brain.h`). Open question: how a "method swap" is chosen
   and shown.
3. Tune by ear (incl. Punch's harsh-zone side effect).
Process (CONVENTIONS): short plain digests; propose first, build after "go"; mockup before UI; offline tests first;
commit every build, the user's go means push.
