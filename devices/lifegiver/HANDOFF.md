# Lifegiver — Handoff

_Last updated: 2026-10-15_

## What it does
A Max for Live audio effect that listens to a sound for a few seconds, reads what it is (tone, timbre, dynamics, width), and sets character dials from that reading. The user picks what job the sound does in the mix and tweaks the dials by ear; the device works out the processing. An assistant: it gives an educated starting point, the user decides.

## Current state
- Design phase. Nothing built; no patch, JS or gen~ code exists.
- Settled: the role, the flow, Step 1 (Listen), how dials behave (see Decisions).
- In progress: Step 2, which dials and what each reads and does.
- Not started: Step 3 (choosing the function in the mix), the processing design, UI mockup, build.

## Structure
- `devices/lifegiver/RESEARCH.md`: theory notes with sources (timbre dimensions, roughness/ERB, distortion and aliasing, compressor design, punch, loudness). Read it before designing processing.
- Planned flow: input → engine (one gen~ chain) → output. Analysis runs only while listening; the brain (JS) turns the reading plus dial settings into engine values.

## Decisions (and why)
- It must WORK: results over flashiness; not a showpiece for amateurs.
- Assistant, not autopilot: a device that "decides" ends up applying a few generic fixes.
- Flow: sound (read by the device) + function in the mix (the user's choice, never guessed: the same sound can be a lead or background FX) → processing.
- Listen = Arm: press Arm, then play by hand (or it starts at once on running audio); captures ~4 s of non-silent audio, skips silence, stops itself.
- One take for now; designed so more takes can be added later.
- Analysis only while listening, no live tracking: CPU.
- Listen reads the input, never the output: it must not chase its own processing.
- High notes: educated guess from the harmonic make-up, no high-note takes; level-triggered taming catches what the guess misses.
- The reading sets each dial's value directly (not a ghost marker); the user tweaks it. Each dial shows only an indicator of how unsure the guess is.
- Turning a dial away from the reading = the processing target (the gap is what gets processed).
- Front: poetic dial names, each tied to one perceptual dimension (research: luminance, mass, texture + attack, dynamics, space).
- Underneath: no fixed size; as many measurements and stages as accuracy needs. Each must earn its place by a measured improvement.
- A small amount of space (ambience) belongs in the device.
- Processing rules from theory: ERB-spaced analysis; saturation only soft, low-order, antialiased, and little or none on inharmonic/dense sounds; compressor = feedforward, dB-domain smoothing, smooth decoupled peak detector, soft knee, time constants longer than the lowest note's period; output gain-matched by loudness for honest A/B.

## Rejected approaches
- Device decides the role: guesses wrong and processes in too few ways.
- Live matching of several takes while playing: too much change before the core exists; CPU.
- Ghost markers instead of setting dials: user wants the dial set, then tweaks it.
- Saturation as a default move: intermodulation on complex sounds; must be conditional.
- Per-dial XY pads, a multi-dot "how" pad, a Stage puck, a 2D character map, a Colour knob: too abstract, or duplicated the dials.
- Exactly 6 dials: Bright mixes presence/harshness with air; Thick mixes weight with mud; attack without tail; no depth.

## Known issues
- None (nothing built).

## Next step
Finish Step 2, one question at a time; don't rush to Step 3.
1. Confirm the dial set. Current proposal (10, in four groups): Tone: Weight · Warm · Bite · Air | Texture | Dynamics: Punch · Tail · Steady | Space: Wide · Depth. User asked whether 6 was enough; this answers it, not yet confirmed.
2. For each dial: what it reads and what turning it does.
3. Character dial (how strongly the timbre reading steers processing): user wanted it as strength of behaviour; keep or drop.
Then Step 3: how the function in the mix is chosen (proposed: a field back ↔ front × carry ↔ drive with landmarks Lead, Hook, Bed, Background FX; not confirmed). Also paused, not rejected: the "stage + blob" idea (the sound drawn as a shape you reshape).
