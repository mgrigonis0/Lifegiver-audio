# Lifegiver — Handoff

_Last updated: 2026-10-17 (build 2026-10-17c built and tested offline; not yet tested in Live)_

## What it does
A Max for Live audio effect that listens to a sound (Arm, up to 4 short takes), reads what it is, and shapes it for its
job in the mix. The user places it on a **stage** (role, pan, distance, width, range, **room** = the back wall), turns
relative **quality knobs**, and blends **characters A–E** (Polish, Warm, Bold, Vintage, Driven; all 0 = none).
**Moves** lists what it does and how much of each ask arrived. Out: **Identity** (strength), **Clean**, **Amount**,
**Scale** (0–400 %), **Dyn | Avg**, **Compare**, **Gain**. The output always matches the input's loudness.
**The user's rule: taming through the reading, automatically (Clean); shaping only when asked** (knobs, A–E,
distance). Roles only place. Nothing may push sounds toward an average (no sameness).

## Current state
- **17c built, all offline tests pass, strip render approved (`design/mockups/17c-strip-render.png`), pushed (CI
  publishes the release). Waiting for the user's Live test (INSTALL.md list).**
- 17b in Live (user): UI good; CPU 4 instances ≈ 15–19 % on the 2015 Intel Mac. 17c answers the rest of that test:
  width by roles, room control, gentle Clean, hi-hat clicks, invisible Identity, sound shifting/knobs greying mid-tweak.
- Next: the review, then Balance (Next step); then 17d (adaptive brain), 17e (colour) — Road.

## Structure
- `source/engine/` header-only C++17: `dsp.h`, `modules.h` (DynBell with `floor`, HiSmooth, Multiband, Ambience FDN),
  `chain.h` (PlanIdx/Plan, Chain, `LevelMatch`, tamer/shaper constants), `measure.h` (8 measures, BS.1770),
  `profile.h` (the reading incl. `findExcess`), `brain.h` (stagePush, basePlan, `TOOL_FOR`, makeProblem, solve, fastStep,
  movesOf), `processor.h` (audio thread), `controller.h` (main thread, `SOLVE_DEBOUNCE_MS`, `LANDING_TAU`, saving,
  `updateAdjustable`).
- `Lifegiver/lifegiver.strip.js` (the view, ES5/ASCII), `tools/build_device.py` (→ .amxd/.maxpat, never hand-edit).
- Tests `tools/tests/engine_test.cpp` (parts: `modules|brain|level|17c`), `tools/ui/check_view.js` + `render_view.py`.
  Probes + stem recipes: `tools/probes/` (README; judge clicks by clickhat's **rel** column). Mockups: `design/mockups/`.

## Parameters (Live, append-only)
Weight…Steady (−10…10) · Role · Stage Pan / Distance / Width (0…1, **0.5 = own width, the default**; g = 0.25 + 1.5·w)
/ Edge (unused since 17b) · Range Lo/Hi · Identity Fit (shown as Identity = 1 − fit) · Amount · Clean · A…E · Scale ·
Dynamics · Gain (±12 dB) · **Room** (0…1, 0.5 = as before; 17c). Names never change; new ones go at the end of `PARAMS`.
Saved inputs (`inputsVector`) and plans (`PlanIdx`) only grow at the end (17b sets read as a prefix).

## Decisions (and why)
- Closed-loop solver on the takes; reach reported honestly; tools serve only listed qualities (`TOOL_FOR`); every
  dynamic module linear in its amount; splits by subtraction; zero latency everywhere.
- Solver: a small gain stops it only once its sensitivities were measured in this solve; a warm start (another
  character mix) is re-measured once first (it stalled E at 17 % on piano; cold solves are unchanged).
- Live level match: plan make-up at once, measured correction glides (whole-make-up glide made a −7 dB dip).
- A slow solve's plan lands with a ~0.4 s S-glide (`LANDING_TAU` 70 ms/stage via the plan command); direct moves keep
  15 ms. Re-solve 0.8 s after the last change. Knob availability from the reading (bands: off < −60 dB, on ≥ −56) and
  the Range, never from a solve (a solve's zero point can hover at a limit).
- Roles set distance and range only; width (and pan, room) are the user's.
- Room: back wall → mix = ambMixFor(depth)·(0.4 + 1.2·room) (chain caps 0.5), size = 0.4 + 0.8·room; distance still
  decides how much room is heard (none in the front half). Large room at the very back ≤ −10.5 LU (limit −9).
  Reflections + 16-line Hadamard FDN, fixed line lengths (1 line rang; 8 still peaked).
- Clean's excess tamer: full depth at 67 % (40 % → 0.6); the usual excess is a **standing cut** (`P_EXC_FLOOR`, so a
  hit's start is cut like its body); above usual +0.5 dB per dB (steeper ticks: a dynamic cut always lands after a
  hit starts; 1.5 dB/dB gave +1 dB rel on real hats). Clean 40 %: hat_pierce zone −1.9 dB (17b −1.0), real hats −1.3
  (−0.85); voice, strings, piano, sax, even hat: 0.
- Excess = above BOTH neighbours (5/6–2 oct) in the sound's own spectrum, 2–8 kHz weighted to 2.5–5.5 kHz, noisy
  sounds judged against their ~3 dB/oct rise; not a sound family. Clean's taming steps back at Bite +4 asked.
- Punch is read only when 64 Hz–1 kHz holds energy within 25 dB of the whole sound (a hat reads "—": nothing to
  sharpen). The transient shaper's gain acts below 2.5 kHz only (4th-order split by subtraction): with character C on a
  drum loop the highs' onset jump fell from +8.4 to +2.6 dB. 1.5 kHz thinned the shaper inside the Punch band (solver
  ~3× the renders).
- Identity enables holder EQ bands from 25 %; an EQ band moved for an unasked quality shows as "Holding <quality>".
- Bite/Air stay at absolute frequencies (brightness follows the absolute centroid: RESEARCH [45]).

## Rejected approaches
Saving take audio; all-pass crossover; unbounded Broyden; every tool for every quality; roles setting pan or width;
gen~; pleasantness guard; key/scale selector; lowest-note setting (parked); excess by average-of-sides or whole-tilt
fit (flagged the voice); release from note spacing (unvalidated: [46]); transformer/tube "warmth" claims ([51]);
instrument labels/families; mix awareness (a separate device); following the song over time (CPU); a passage-level
(slow) tamer detector (didn't remove the onset tick); transient split at 1.5 kHz (solver cost).

## Known issues (to keep in mind)
- **E Driven clicks on hats** (clickhat rel up to +12 dB, either split): its compressor/multiband floors at a 5 ms
  attack. 17d (attack and timing per sound).
- Mean A–E reach within a few points of 17b, per stem ± (`tools/probes/reachcheck.cpp`); snare A/E lower.
- A Polish still pushes Air/Bite up on harsh sounds (17d). Bite− with Air− fight on all-top sounds (17d).
- Notes much lower than the take can be thinned (Clean down or listen again).
- Snare: the resonance tamer's low rings lift the highs' share +1.5 dB at Clean 40 % (judge by ear). hat_harsh goes
  through the resonance tamer, not the excess tamer, so 17c's firmer Clean doesn't change it (−1.0 dB zone).
- A reopened 17b set plays its plan with 17c's tamer curve (no standing cut until it re-plans): close, not identical.

## Next step
1. The user's Live test of 17c (INSTALL.md list). Open questions: is Clean at 40 % clearly audible on their hat; does
   a large room at the back feel right (mix cap 0.5); any knob greying or sound jumps left.
2. **Before building anything else: review the whole build** for logic errors, redundancies and design flaws (the
   user asked; engine, view, generator, tests). Report findings with a recommendation each, one decision at a time.
   Already found (user agreed it's needed): **the front distance cue fights Clean**: `stagePush` lifts Bite +1.5·f in
   front, and Clean's excess cut sits in the same zone (trumpet as Lead: zone change +0.0 dB, Bite reach 0.2). Fix:
   in front the cue lifts Air only (no Bite); further back Bite still drops. Clean does NOT step back for the stage
   (that would just weaken Clean on every Lead). Check `module_tests` "roles only place" (expects Punch 0.3 at 0.6).
   Rejected: a "new sound? re-listen" hint (the user: nothing reads unless asked; redundant).
3. **Then build 18a: Balance** (user approved the idea). A slider, 0 = as read, pivots the sound's low↔high balance
   around ~1 kHz, relative to the sound (no sameness). It adds matched pushes to the tone knobs (Weight/Warm one way,
   Bite/Air the other), shown as their teal arcs: the solver reaches it with its tools, Identity doesn't fight it,
   knob turns add on top. New Live parameter appended; a thin bar under the 8 knobs, centre mark, double-click = 0.
   Open: smooth tilt (recommended) vs seesaw (lows and top only). Mockup first, build after "go".

## Road
- **17d, adaptive brain** (RESEARCH "Backing for the adaptive brain"): Warm follows the body; crossovers placed
  between the sound's peaks; compressor timing from crest factor + two-stage release; attack chosen for Punch (and
  E's hat clicks); cuts narrower than boosts (today one Q); single compressor default; characters relative to the
  sound; the one-region fix.
- **17e, colour:** per-band character saturation; tape-like top softening with drive; distance = more room energy
  after ~50 ms ([54]). Then tuning by ear; build 2 (Identity specialities + Moves pop-out, mockup first); better
  tools only if CPU allows.
Process (CONVENTIONS): short plain digests, one decision at a time; propose, build after "go"; mockup before UI;
offline tests first; commit every build; the user's go means push.
