# Lifegiver — Handoff

_Last updated: 2026-10-17 (build 2026-10-17b; 17c specified, not started)_

## What it does
A Max for Live audio effect that listens to a sound (Arm, up to 4 short takes), reads what it is, and shapes it for its
job in the mix. The user places it on a **stage** (role, pan, distance, width, range), turns relative **quality knobs**,
and blends **characters A–E** (Polish, Warm, Bold, Vintage, Driven; all 0 = none). **Moves** lists what it does and how
much of each ask arrived. Out: **Identity** (strength), **Clean**, **Amount**, **Scale** (0–400 %), **Dyn | Avg**,
**Compare**, **Gain**. The output always matches the input's loudness.
**The user's rule: taming through the reading, automatically (Clean); shaping only when asked** (knobs, A–E,
distance). Roles only place. Nothing may push sounds toward an average (no sameness).

## Current state
- **17b in Live (user):** UI good; Clean audible on a hi-hat but gentle; Identity barely noticeable; hi-hat got clicky
  on transients; width is set by roles (the user always wants "as the sound is"); the narrowest bubble squeezes its
  handles; wants a room/reverb control; sometimes the sound shifts / knobs grey mid-tweak. **CPU: 4 instances + a few
  light synths ≈ 15–19 %** on the 2015 Intel Mac (≈ 3–4 % per instance): headroom for later builds.
- 17a verified in Live (room, level match, Gain). Offline: all tests pass, CI green, release `lifegiver-latest` = 17b.
- **Next: build 17c** (spec below, the user said go). Then 17d, 17e (Road).

## Structure
- `source/engine/` header-only C++17: `dsp.h`, `modules.h` (DynBell, HiSmooth, Multiband, Ambience FDN),
  `chain.h` (PlanIdx/Plan, Chain, `LevelMatch`), `measure.h` (8 measures, BS.1770), `profile.h` (the reading incl.
  `findExcess`), `brain.h` (stagePush, basePlan, `TOOL_FOR`, makeProblem, solve, fastStep, movesOf), `processor.h`
  (audio thread: Amount, Compare, Gain), `controller.h` (main thread, debounce `SOLVE_DEBOUNCE_MS`, saving).
- `Lifegiver/lifegiver.strip.js` (the view, ES5/ASCII), `tools/build_device.py` (→ .amxd/.maxpat, never hand-edit).
- Tests `tools/tests/engine_test.cpp` (parts: `modules|brain|level`), `tools/ui/check_view.js` + `render_view.py`.
  Probes + stem recipes: `tools/probes/` (README). Mockups: `design/mockups/`.

## Parameters (Live, append-only)
Weight…Steady (−10…10) · Role · Stage Pan / Distance / Width (0…1, **0.5 = own width**, g = 0.25 + 1.5·w) / Edge
(unused since 17b) · Range Lo/Hi · Identity Fit (shown as Identity = 1 − fit) · Amount · Clean · A…E · Scale ·
Dynamics · Gain (±12 dB). Every name stays (old sets, automation); new ones go at the end of `PARAMS`.

## Decisions (and why)
- Closed-loop solver on the takes; reach reported honestly; tools serve only listed qualities (`TOOL_FOR`); every
  dynamic module linear in its amount; splits by subtraction; zero latency everywhere.
- Live level match: plan make-up at once, measured correction glides (whole-make-up glide made a −7 dB dip).
- Room: reflections + 16-line Hadamard FDN, fixed line lengths (1 line rang; 8 still peaked).
- Excess = above BOTH neighbours (5/6–2 oct) in the sound's own spectrum, 2–8 kHz weighted to 2.5–5.5 kHz, noisy
  sounds judged against their ~3 dB/oct rise; not a sound family (the user: by frequency composition). Leaves voice,
  strings, piano, winds, even hats alone. Clean's taming steps back at Bite +4 asked.
- Roles only place (no quality pushes); distance keeps its cues. Identity enables holder EQ bands from 25 %.
- Bite/Air stay at absolute frequencies (brightness follows the absolute centroid: RESEARCH [45]).

## Rejected approaches
Saving take audio; all-pass crossover; unbounded Broyden; every tool for every quality; roles setting pan; gen~;
pleasantness guard (side effects ≤ 0.8 dB, `TOOL_FOR` covers it); key/scale selector; lowest-note setting (parked);
excess by average-of-sides or whole-tilt fit (flagged the voice); release from note spacing (unvalidated: [46]);
transformer/tube "warmth" claims ([51]); instrument labels/families; mix awareness (a separate device); following the
song over time (CPU).

## Known issues (to keep in mind)
- A Polish still pushes Air/Bite up on harsh sounds (17d). Bite− with Air− fight on all-top sounds (17d).
- Notes much lower than the take can be thinned (Clean down or listen again).
- Snare: the resonance tamer's low rings lift the highs' share +1.5 dB at Clean 40 % (judge by ear).

## Next step: build 17c (user approved; mockup `design/mockups/17c-stage-width-room.png` approved)
1. **Width = own width by default.** Roles stop setting width (`ROLE_PRESETS` width → keep the user's; `pickRole` in
   `controller.h` no longer sends width); Live default 0.35 → 0.5 in `build_device.py`. View `geom()`: rx =
   (26 + 40·w)·scale (min no longer squeezes the handles); a faint outline at w = 0.5 when width ≠ 0.5; stage line
   "own width / narrower / wider" (`stageWords`).
2. **Room wall.** New Live param `room` 0…1 (default 0.5 = today's sound), appended. View: the back wall moves
   (wy = 4 + (1 − room)·14, floor top follows), a tab at its centre to drag, label SMALL ROOM / ROOM / LARGE ROOM,
   "small/large room" in the stage line. Engine (`brain.h` basePlan): mix = ambMixFor(depth)·(0.4 + 1.2·room), size =
   0.4 + 0.8·room (replacing ambSizeFor's depth term); distance still decides how much room is heard. Check the room
   level stays ≤ ~−9 LU at room 1 (probe as in the 17a room check; RESEARCH reverb amount). Controller message +
   `Inputs` field + save in `inputsVector` (append-only).
3. **Clean firmer.** Excess tamer: at the usual level cut the whole excess (not excess − 1.5), peaks 1.5 : 1: in
   basePlan `P_EXC_THR = excThr − excDb/1.5`, chain `exc.control(thr, 10, 15)`; depth = clean × 1.5 (40 % → 0.6),
   unit clamped −12. Verify with `zone.py` on hat_pierce/hat_harsh (clearly audible at 40 %) and that voice, strings,
   piano, even hat still get nothing.
4. **Hi-hat click fix** (diagnosed with `tools/probes/clickhat.cpp`): Punch is measured 64 Hz–1 kHz, but on a hat
   that band is nearly empty, yet Lead's front distance cue (+1.5·f Punch) drives the transient shaper and compressor
   on the whole band (worst hits +3…+5 dB above 8 kHz). Fix: (a) Punch valid only when the 64 Hz–1 kHz band holds real
   energy (e.g. within ~25 dB of the full band; `measure.h` finish → `valid[Q_PUNCH]`), so hats read "—" and nothing
   sharpens them; (b) the transient shaper's gain applies only below ~1.5 kHz (split by subtraction, highs pass:
   `chain.h` around `tsGain`), matching where punch lives (RESEARCH [11]). Test: clickhat worst ≤ +0.5 dB on the hats.
5. **Identity visible.** Moves lines "Holding Bite" etc. when Identity's holders move a tool for an unasked quality
   (new MoveKind appended; view `MOVE` words + render.cpp `MOVE_TEXT`).
6. **Smoother re-plan, stable knobs.** The slow solve's result glides in (~0.4 s S-curve, only for re-solve landings,
   not for direct knob moves: e.g. a longer `PLAN_TAU` for that hand-over) and `SOLVE_DEBOUNCE_MS` 400 → ~800. Knob
   availability (`adj` in `controller.h` sendUi) from the reading with hysteresis, not re-decided on every solve.
Version `2026-10-17c` (or the day's letter), all tests + view check, INSTALL.md test list, render the strip for the
user's OK before pushing.

## Road
- **17d, adaptive brain** (RESEARCH "Backing for the adaptive brain"): Warm follows the body; crossovers placed
  between the sound's peaks; compressor timing from crest factor + two-stage release; attack chosen for Punch; cuts
  narrower than boosts (today one Q); single compressor default; characters relative to the sound; the one-region fix.
- **17e, colour:** per-band character saturation; tape-like top softening with drive; distance = more room energy
  after ~50 ms ([54]). Then tuning by ear; build 2 (Identity specialities + Moves pop-out, mockup first); better
  tools only if CPU allows.
Process (CONVENTIONS): short plain digests, one decision at a time; propose, build after "go"; mockup before UI;
offline tests first; commit every build; the user's go means push.
