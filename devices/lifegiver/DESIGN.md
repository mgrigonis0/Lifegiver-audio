# Lifegiver — Design decisions

_Living list of what is agreed. Newest at the bottom of each section._

## Principle
- It must WORK: good results over flashiness. Not a showpiece.
- Front is small and musical; underneath is as large as accurate processing needs. Every internal part must earn its place by a measured improvement.
- Theory first: see `RESEARCH.md`.

## Role
- An assistant: it reads the sound and sets a starting point; the user, who hears and composes, decides.
- Flow: sound (what it is, read by the device) + function in the mix (the user's choice) → processing.

## Step 1: Listen
- Arm: press Arm, then play (or it starts at once if audio is already running); captures ~4 s of non-silent audio, skips silence, stops itself.
- One take. More takes = a later possibility.
- Analysis runs only while listening; no live tracking (CPU).
- Reads the input, never its own output.
- Reads: tone (brightness, low weight), timbre (harmonics, inharmonicity, noisiness, roughness), dynamics (attack, tail, level evenness), stereo width, register.
- High notes: educated guess from harmonic make-up; taming tools catch what the guess misses.

## Step 2: The reading
- Sets 6 poetic dials directly (not ghost markers), each with an indicator of how unsure the guess is.
- Dials: Bright · Thick · Texture · Punch · Steady · Wide, each tied to one perceptual dimension.
- Underneath: no fixed size; as many measurements and processing stages as accuracy needs.

## Open
- Small Character dial (strength of timbre-driven behaviour): proposed, not confirmed.
- Step 3: how the function in the mix is chosen.
- A touch of space: agreed in principle; how it is controlled is open.
