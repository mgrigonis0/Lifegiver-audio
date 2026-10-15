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
- **Phase 1, build 2026-10-15c** (pushed, CI green, release `lifegiver-latest`): stereo pass-through, Amount (Live
  parameter), version printed by engine and patch, saved state (device id), offline tests all pass, 0 warnings.
- **Verified in Live:** loads, both version lines, Amount click-free, two devices get their own ids.
- **Saving does NOT work yet** (Live tests 1–3: every reopen says "new device"). The 15c trace shows:
  - during the session the hidden `[pattr state]` holds the 34-number list (it echoes it back after each save);
  - after reopening, the pattr answers its **initial value `0`** (at 0, ~300, ~800 ms) and the unnamed
    `dict @parameter_enable 1` answers **nothing**; no store ever outputs a restored value later.
  - So Live does not restore the stored value at all; timing and the codec are ruled out.
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
  `prepend replace state` → dict. Both hidden params: type 3, Stored Only, **initial enable 1, initial 0** (suspect).

## Parameters (Phase 1)
| Name | Range | Purpose |
|---|---|---|
| Amount | 0–100 % (init 100) | Blend: 0 % = the input bit for bit; 100 % = full plan (now: unity) |
| State | hidden blob (pattr) | Saved state as a number list (format, device id, saving build) |
| State Dict | hidden (unnamed dict) | Same list under key "state"; second route while saving is unproven |

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
dict (global/crosstalk, or renamed on reload); knob values through the command ring; gen~.

## Known issues
- Saving (above). Wanderer's State pattr has the same initial-enable setting, so it is probably affected too (another
  chat is fixing Wanderer; CONVENTIONS "Saving in the Live set" holds what is known).
- Wanderer's wrapper allocates on the audio thread (resize in perform): never copy that part.

## Next step
Fix saving, one Live test. Prime suspect: **Initial Enable on (initial 0) for the hidden state parameters**; Cycling '74
docs say the initial value is recalled when a device is instantiated, saved or edited, which would wipe the stored list
on load (both stores failed identically, and that setting is the one they share).
1. `tools/build_device.py`: for State (and State Dict) drop `parameter_initial` / set `parameter_initial_enable` 0
   (`valueof()` always sets it to 1 now: add an option). Bump to 15d. Keep the 15c trace for this test.
2. Live test: new set, two devices, wait for `[save]`, save, reopen. Expect `state restored from the pattr`.
3. If it still comes back `0`: check in Max (open the device, Inspector of `pattr state`) that Parameter Mode, Type
   Blob and Stored Only are really applied after loading the generated JSON; try `live.thisdevice` + the stored value
   via `pattr @autorestore` or a `[pattrstorage]`-free approach found by research. Don't build blind: research first.
4. When it works: keep the store that answered, remove the other and the dict code, turn the trace off, update
   CONVENTIONS ("proven"). Then Phase 2 (test bench; ask the user for ~10 short dry stems).
