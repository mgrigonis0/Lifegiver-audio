# Lifegiver — Handoff

_Last updated: 2026-10-16_

## What it does
A Max for Live audio effect that listens to a sound (up to 4 short takes), reads what it is, and helps the user shape it
for its job in the mix. The user places the sound on a **stage** (role, pan, distance, width, frequency range,
intensity), adds their own **relative knob** turns on top, and mixes **strategies A–E** at any strengths; a **Moves**
console shows what it does and the measured result. An assistant, not an autopilot.

## Current state
- **Phase 0 done:** mockup v2 approved ("good for a first go"): device strip at real 169 px (Listen → Tone · Dynamics
  → Stage → Moves → Out, ~1200 px wide, all in the strip) + Moves pop-out window.
  https://claude.ai/artifact/W62NjUfMmqUUmjdp4ap7Fv (Design canvas, private to the user; v1 kept as history).
- **Phase 1, build 2026-10-15e** (pushed, CI green, release `lifegiver-latest`): stereo pass-through, Amount (Live
  parameter), version printed by engine and patch, saved state (device id), offline tests all pass, 0 warnings.
- **Verified in Live:** loads, both version lines, Amount click-free and saved (37 % came back), two devices get their
  own ids, and **saving works** (15e: after reopen both devices said `state restored from the pattr` with their own
  ids; both stores answered at 0 ms).
- Cause of the 15a–15d failures: the generator put `parameter_enable` on the box only; for a pattr/dict it must be in
  `saved_object_attributes` (CONVENTIONS "Saving in the Live set"). Found by reading the set file: Live had stored an
  empty blob. 15d's Initial Enable change was not the cause (kept off: it is part of the proven combination).
- Docs: `DESIGN.md`, `BUILD_PLAN.md`, `RESEARCH.md`, `REVIEW.md`, `design/ui-reference/` (UI style the user likes).

## Structure
- `source/engine/` (header-only C++17, no Max deps): `version.h` (the one version string), `state.h` (StateDoc +
  number-list codec), `ring.h`, `smooth.h` (two-pole smoother), `processor.h` (audio thread: Amount blend + make-up
  stage), `controller.h` (main thread: messages, load/save logic incl. two stores, late restore, re-sync, trace).
- `source/projects/lifegiver_tilde/lifegiver_tilde.cpp`: thin wrapper (2 signal inlets, outlets L/R/info; `getstate`,
  `statelist`, `dictionary <name>` reads key "state"; 15 ms tick). Nothing allocated in perform.
- `tools/build_device.py` → `Lifegiver/Lifegiver.amxd/.maxpat` (never hand-edit; type `aaaa`; reads `version.h`;
  checks wiring, layout, `---` names, parameter names). `tools/tests/` (harness with two simulated stores + tests).
- `INSTALL.md`; CI `.github/workflows/lifegiver.yml` (tests + generator on Linux → universal mac build → release).
- Save flow: lifegiver~ "getstate" → `t b b` → bang [pattr state] (→ `prepend statelist` → lifegiver~) and the
  unnamed [dict] (→ "dictionary <name>" → lifegiver~). "statelist <numbers>" → [route] → pattr, and
  `prepend replace state` → dict. Both hidden params: type 3, Stored Only, no initial value, parameter mode in `saved_object_attributes`.

## Parameters (Phase 1)
| Name | Range | Purpose |
|---|---|---|
| Amount | 0–100 % (init 100) | Blend: 0 % = the input bit for bit; 100 % = full plan (now: unity) |
| State | hidden blob (pattr) | Saved state as a number list (format, device id, saving build) |
| State Dict | hidden (unnamed dict) | Same list under key "state"; works too, to be removed (one store is enough) |

## Decisions (and why)
- Design-level: assistant not autopilot; role by button only; knobs relative, never moved by the stage (stage arc);
  one solver, variations = tool preferences, mixes re-solved; closed loop on the takes; fast path + slow refine; fixed
  module order; zero latency; no limiter; level = static make-up + stage (0 to −3 dB). Full list: DESIGN.md.
- Engine on doubles straight on Max's buffers: bit-exact dry path, no work buffers.
- Amount/make-up as atomics (a ring could drop the newest value while audio is stopped); first values before audio
  starts apply at once (no fade-in of processed sound on load).
- Two one-pole smoothing stages, 4 ms each: S-shaped, smooth when automation retargets mid-glide.
- Saved state as a number list (no shared names, no symbol per save, malformed lists refused whole); load logic in
  the controller so it is tested offline: asks at 0/300/800 ms, "new device" at 1.5 s, first save at 4.5 s, a valid
  state up to 10 s late still wins, disagreeing stores are re-synced, identical saves skipped (no undo steps).

## Rejected approaches
DESIGN.md §16 for design. Build-level: a dict name in the pattr (15a: only the name is saved); a fixed-name or `---`
dict (global/crosstalk, or renamed on reload); `parameter_enable` on the box only for a pattr/dict (15a–15d: ignored,
empty blob saved); knob values through the command ring; gen~.

## Known issues
- Wanderer's State pattr has the same mistake (box-level `parameter_enable` only, initial enable on): the fix and the
  `.als` check are in CONVENTIONS "Saving in the Live set" (another chat is fixing Wanderer).
- Wanderer's wrapper allocates on the audio thread (resize in perform): never copy that part.

## Next step
1. Clean-up build (15f, proposed, waiting for the user's "go"): keep the pattr store only (remove the dict, its
   `dictionary` path in the wrapper and the second store in controller/tests), turn the load/save trace off (keep one
   line: `state restored` / `new device`), offline tests, CI. One short Live check (save, reopen: `state restored`).
2. Then Phase 2 (test bench; ask the user for ~10 short dry stems).
