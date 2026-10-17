# Lifegiver — Handoff

_Last updated: 2026-10-17 (build 2026-10-17b)_

## What it does
A Max for Live audio effect that listens to a sound (Arm, up to 4 short takes), reads what it is, and shapes it for its
job in the mix. The user places it on a **stage** (role, pan, distance, width, edge, frequency range), turns relative
**quality knobs** on top, and blends **characters A–E** (Polish, Warm, Bold, Vintage, Driven: subtle → strong, all 0 =
none). **Moves** lists what it does and how much of the ask it reached. **Amount** blends with the original, **Scale**
(0–400 %) multiplies the processing, **Dyn | Avg** lets the dynamic tools follow the sound or hold their average,
**Gain** (±12 dB) is the user's trim after everything. The output always matches the input's loudness.
**The user's rule (17b): taming through the reading, automatically (Clean); shaping only when asked** (knobs, A–E,
distance). Roles only place the sound. **Identity** (strength) holds what nobody asked to change.

## Current state
- Working offline (all tests pass, view check passes): the whole chain, solver, characters, Scale, Dyn | Avg, saving,
  the strip view (706 × 169), and 17a's new room, live level match and Gain bar.
- Verified in Live: loading, version lines, saving (15e), 16a's restore; **17a tested by the user: nothing out of the
  ordinary** (room, level match, Gain). CPU with several instances not reported yet. **17b is not yet tested in Live**
  (INSTALL.md "This build (2026-10-17b)").
- 17b (built, offline green): Clean tames the reading's excess; roles only place; Edge removed; Identity strength
  with holder tools; A–E faders. The user approved the A–E fader mockup; the full 17b strip render was shown for OK.
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
| Stage Pan / Distance / Width / Edge | −1…1 / 0…1 / 0…1 / 0…1 | placement; distance also brings level, darkness, room; Edge unused since 17b |
| Range Lo / Hi | 20 Hz–20 kHz | the band the sound may own (cuts) |
| Identity Fit | 0…1 | shown as **Identity** strength = 1 − fit (17b): holds unasked qualities (holder EQ bands from 25 %) |
| Amount | 0–100 % | dry/processed blend (0 = bit-exact input) |
| Clean | 0–100 % | rumble cut, mono safety, resonance tamer, **excess tamer** (17b) depth |
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
- **Excess** = stands above BOTH neighbours in the sound's own spectrum (5/6–2 oct away), 2–8 kHz weighted to
  2.5–5.5 kHz, noisy sounds judged against their ~3 dB/oct rise. Not a sound family or label (the user: adaptive by
  frequency composition, the role can hint). Rejected: average-of-sides and whole-tilt fits (flagged the voice).
- Clean's taming steps back fully at Bite +4 asked (knob or character pushes); depth = Clean.
- Identity holders: unasked qualities' two-way EQ tools enabled from 25 % strength (Air +8 on a lead: side effects
  −23 %). Without them the slider did nothing measurable (the solver only moved asked qualities' tools).
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
- Clean's resonance tamer on the snare (rings at 206 / 1288 Hz) lifts the highs' share by +1.5 dB at 40 % (it cuts
  low rings): existing behaviour, judge by ear.
- A Polish still pushes Air/Bite up on harsh sounds (reach 27–50 % on snare/hats): fixed by 17c (characters relative).
- The excess threshold/strength are first drafts (piercing hat: −1 dB zone at Clean 40 %, −2.5 at 100 %): by ear.
- After a big move on the fast path the level settles within ~1 s (correction measured afresh).
- Reach: D Vintage and E Driven reach least. Smoothing modest without latency. First plan 0.3–4.5 s after Listen.
- CPU every module on: ~4.5 % of a core here (busy plan incl. room; 16c was ~4.2 %); test fails above 5 %.
- Wanderer's pattr/wrapper mistakes (box-level `parameter_enable`, allocation in perform): never copy them.

## Road (agreed 2026-10-17, research-backed: RESEARCH.md "Backing for the adaptive brain")
1. **17c, adaptive brain:** Warm follows the sound's body (Bite/Air stay absolute: brightness is absolute); both
   crossovers placed between the sound's strong peaks; compressor timing from the sound's transients (crest factor)
   with a two-stage release (NOT from note spacing: unvalidated); attack chosen with Punch in mind; cuts narrower than
   boosts; single compressor default, 3-band as specialist; characters relative to the sound (A Polish must not
   brighten an already harsh sound; on the hats it still does). One-region sounds (all-top hats): adjacent band asks
   fight under relative measures (Bite − with Air −): study a fix here.
2. **17d, colour:** character colours on solid ground: per-band saturation (cleaner, backed) and tape-like softening
   of the highs with drive (physics); no transformer/tube claims; distance as more room energy after ~50 ms.
3. Tuning by ear on the user's material. 4. Build 2: Identity specialities + Moves pop-out (mockup first).
5. Better tools only if CPU allows (many-band resonance smoothing, optional latency mode).
Out of scope: mix awareness (a separate device), following the song over time (CPU).

## Next step
1. The user's 17b Live results (INSTALL.md list: harsh sound + Clean, Bite wins, roles only place, Identity, faders,
   a 17a set reopened). Ask for a CPU reading with 4–6 instances. Fix what they report, then 17c (Road above).
2. Build 2 (after 17c/17d and tuning): Moves pop-out mockup → approval → build; Identity specialities. Open
   question: how a "method swap" is chosen and shown.
Process (CONVENTIONS): short plain digests; propose first, build after "go"; mockup before UI; offline tests first;
commit every build, the user's go means push.
