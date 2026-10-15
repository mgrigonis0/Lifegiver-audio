# Lifegiver — interaction, 30 iterations

_2026-10-15. Start: Listen → place on function field → tweak dials → Colour + Space._
_Each step: what changed, and why._

1. **Your tweaks survive puck moves.** A dial tweak is stored as an offset from the function's target, so dragging the puck again keeps it. Otherwise every drag wipes your work.
2. **Error is shown as the width of the reading tick,** not a separate ring. Six dials with rings plus ticks was too busy.
3. **Carry ↔ drive stays,** even though it touches Punch: it *writes into* Punch's target, there is no parallel path.
4. **Dragging is playable.** The processing follows the puck live, with a ~100 ms glide, so you can move it while the loop plays and hear it.
5. **Confidence limits automatic moves.** When the reading is unsure, the puck moves the dials less. The device is honest about what it doesn't know.
6. **An Amount knob scales the gap,** not a wet/dry mix. Wet/dry with EQ'd signal causes comb filtering.
7. **Gain-matched A/B button** (hear the input at matched loudness). Without it, louder always wins.
8. **Hold-to-hear-difference:** plays only what the device changed. Possible because the chain has zero latency.
9. **Space is a short ambience** (early reflections, no long tail). Distance cues are direct-to-room ratio and top loss, so "back" adds a little automatically.
10. **Width follows the function too:** a front lead is focused in the centre, background is wider at the sides. Lows always stay mono.
11. **Layout fits Live's 169 px height:** pad on the left (square), dials in two rows of three, small knobs at the side.
12. **Colour knob removed.** It overlapped with Texture (smooth ↔ rich). One control per idea.
13. **The means follow one rule instead:** if your Texture target is richer than the reading, harmonics may be used (inside the safe zone); otherwise only clean EQ and dynamics.
14. **Steady follows the function:** a lead wants a steady level, a background part may breathe. You can still offset it.
15. **Landmark names sit faintly in the corners** (Lead, Hook, Bed, Texture). Double-click a name to jump there. No snapping.
16. **No third axis** on the field (e.g. size). It would duplicate Thick and Wide.
17. **Listen = arm, then capture ~4 s of signal,** ignoring silence and stopping on its own. A ring around the button shows progress.
18. **Big one: the reading places a ghost** where the sound naturally sits *now* (e.g. bright and percussive → front, drive). The puck starts on the ghost, so **nothing is processed until you move**. The distance from ghost to puck is the change.
19. **Dials also start as the reading,** so everything begins at "as it is" with zero gap. Consistent with 18.
20. **The ghost's size shows the uncertainty,** the same idea as the dial ticks.
21. **Amount stays,** as a quick global "less / more" of everything.
22. **Two maps, both depend on the reading:** field → dial targets, and dial targets → engine. This is where the "matrix" lives. Both must be smooth (no jumps) and monotonic (Bright up is always brighter).
23. **Listen always reads the input,** never the output, so it can't chase its own processing.
24. **The reading is saved with the Live set.** Reopening keeps it (a lesson from Wanderer, where files were lost).
25. **CPU:** one gen~ chain at run time. Analysis runs only while listening. Space uses a few allpasses, which is cheap.
26. **Duplicate devices never crosstalk** (`---` names).
27. **The precision page lives behind a small "…" button:** engine values marked auto or locked, plus the raw reading numbers.
28. **No spectrum drawn on the pad.** Just a small activity light per dial when its processing is reacting. The pad stays clean.
29. **One rule for every control:** auto value (from function + reading) plus your offset. This applies to Space too.
30. **Final check:** every control earns its place and none overlap. The only gesture you must make is moving the puck.

## Where it landed

- **Listen** → a ghost appears on the field, dials show the sound as it is, nothing is processed yet.
- **Drag the puck** from the ghost to where the sound should sit (back ↔ front, carry ↔ drive). The dials glide and you hear it live.
- **Tweak dials by ear:** Bright · Thick · Texture · Punch · Steady · Wide. Each is auto + your offset.
- **Space and Amount** knobs, gain-matched **A/B**, hold-to-hear-difference.
- **"…"** opens the precision page.
