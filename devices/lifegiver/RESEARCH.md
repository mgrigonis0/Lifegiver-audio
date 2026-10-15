# Lifegiver — Research notes

_2026-10-15. Theory the design must respect. Paraphrased; sources at the end._

## How timbre is perceived
- Timbre-space studies agree on three robust dimensions: **attack** (log attack time), **brightness** (spectral centroid) and, less stable, **spectral flux** (how the spectrum changes over time). Besides these, many sounds carry **specificities**: unique features (hollow, metallic, a pinched end) that no shared dimension explains. [1]
- Semantic studies (Greek and English listeners) found three word dimensions: **luminance** (bright/dark), **texture** (smooth/rough), **mass** (thick/thin, heavy/light). Texture follows how energy is spread over the harmonics; thickness and brilliance follow inharmonicity and centroid movement; F0 feeds mass or luminance. [2]
- Producers' own words (SAFE dataset, 263 producers): most used are warm, bright, punch, air, crunch, room, smooth, clear. EQ words map to consistent curves (warm ≈ low-mid lift with top roll-off; thin/clean ≈ lift around 5 kHz). Compression vocabulary is largely separate from EQ vocabulary; distortion words split along dissonance (fuzz, harsh vs subtle, growl). [3]

**For Lifegiver:** "poetic" labels are not vague if they follow these dimensions. Tone, texture/mass and dynamics are separate perceptual families and should stay separate controls. Processing must not erase specificities: they are the sound's identity.

## Roughness, critical bands
- Two partials closer than a critical band beat and sound rough; roughness peaks at about a quarter of a critical band; beyond a full band they sound smooth. [4]
- Critical band width ≈ ERB (Glasberg & Moore): narrow in the lows (in Hz), wide in the highs. [5]

**For Lifegiver:** frequency resolution of analysis and processing should be ERB-spaced, not linear. Roughness/texture can be read as partial density per ERB. Anything that adds partials (saturation) must add them where they fuse (on harmonics), not inside a critical band of existing partials.

## Distortion and saturation
- Low orders (2nd, 3rd) are largely benign or liked; higher orders are much more audible, especially at low levels. THD does not predict perceived quality. Soft clipping stays acceptable far beyond the point where hard clipping sounds bad. [6]
- On a single tone a nonlinearity makes harmonics; on summed/complex signals it mainly makes **intermodulation** (inharmonic, "metallic", muddy). Compression of a sum also makes IMD; processing parts before summing lowers it. [7]
- Splitting into bands before waveshaping keeps each shaper near-monotonal and so lowers IMD; this is the published reason for multiband saturation. [8]
- Any nonlinearity at base rate aliases (folded partials, unrelated to the input). Fixes: oversampling (cost × factor) or antiderivative antialiasing (ADAA), best combined (ADAA + 2×). [9]

**For Lifegiver:** saturation only with soft, low-order curves, always antialiased; on inharmonic or dense material, little or none. Band-split saturation is valid *only* with clean (phase-coherent) crossovers and few bands.

## Dynamics
- Reference compressor design: feedforward, gain smoothing in the dB domain after the gain computer, **smooth decoupled peak** detector, adjustable soft knee. This gives the fewest artifacts and release time independent of the amount of compression. RMS detection tracks loudness better (levelling). [10]
- Perceived **punch** comes mostly from transient energy in the 64 Hz–1 kHz octaves with short onset times; 2–8 kHz transients read as presence or weight rather than punch. [11]

**For Lifegiver:** Punch acts on low/low-mid transients; Edge/presence is a separate thing. Time constants on low frequencies must stay longer than the period of the lowest note (the reading gives F0) or the dynamics distort.

## Loudness and today's standards
- Loudness is measured per ITU-R BS.1770 (K-weighting, LUFS). Spotify normalises to −14 LUFS and asks for true peak under −1 dBTP. Louder is not a win after normalisation. [12]
- Commercial recordings converge on an industry target spectral curve. [13]
- Popular music since 1986 is less noisy than before but still far more inharmonic than orchestral music (layering, distortion); producers seem conservative in the most sensitive ear range. [14]
- Modern quality baseline (industry practice): oversampled or antialiased nonlinearities, dynamic (signal-dependent) EQ, gain-matched A/B, mono-safe lows.

**For Lifegiver:** output gain-matched by K-weighted loudness, so any change is judged on tone, not level.

## Assistants vs automatic
- A decade of automatic mixing research: the problem is unsolved; systems range from black box to assistants (few controls, automating ballistics or make-up gain), semantic interfaces ("warm") and diagnostics. [15]

**For Lifegiver:** confirms the chosen role, an assistant that sets a starting point which the user owns.
- An assisted compressor that predicts settings from audio and lets the user tweak them: users liked the speed; amateurs' main complaints were opacity (couldn't see why it chose the settings) and fear of losing creativity. Make-up gain came from loudness (EBU R128), not from the model. [17]

**For Lifegiver:** show what the device is doing and why (Moves console); level-match by loudness.

## Descriptors depend on the sound and the listener
- Learning what a word means as an EQ curve (≈ 25 ratings per listener): "bright" was fairly consistent (positive tilt); "warm" varied wildly between people (one boosted lows, another cut them), and one listener's "warm" for a voice was nearly opposite their "warm" for drums. [16]

**For Lifegiver:** knobs can't be fixed EQ shapes. They are relative to the measured sound (zero = the reading) and defined as measured quantities.

## Measuring timbre qualities
- Audio Commons built listener-trained models for hardness, depth, brightness, roughness, warmth, sharpness, boominess (0–100 regressions) and reverberation (yes/no). [18]

**For Lifegiver:** close to the knob set; a starting point for how each knob is measured.

## Targets per instrument, resonances
- Automatic EQ toward instrument-specific average spectra (zero-mean dB, ±12 dB limit) was preferred about 2:1 over the original in a small blind test; the authors argue one flat target can't suit all instruments, but didn't test it. [19]
- Resonance taming as a separate step: detect spectral resonances, attenuate by an amount; a network learned the amount engineers preferred. [20]

**For Lifegiver:** the role sets what the target is; resonance taming is a cleanup step with one amount.

## Distance (depth)
- In rooms at 1–6 m, listeners judged distance mainly from overall level; direct-to-reverberant ratio was an unreliable cue; broadband sounds were judged best; strong low-frequency reverb misleads. [21]

**For Lifegiver:** "back" on the stage = lower presence, darker top, softer attack, smaller level jumps, a little room; not just more reverb. Depth is a stage position, not a knob.

## Sources
1. McAdams et al. 1995, Perceptual scaling of synthesized musical timbres — https://three-mode.leidenuniv.nl/pdf/m/mcadams_etal1995pr.pdf
2. Zacharakis, Pastiadis, Reiss 2014, An interlanguage study of musical timbre semantic dimensions — https://sonicfield.org/library/an-interlanguage-study-of-musical-timbre-semantic-dimensions-and-their-acoustic-
3. Stables, De Man, Reiss et al., Semantic description of timbral transformations (SAFE) — https://qmro.qmul.ac.uk/xmlui/bitstream/123456789/22150/1/De%20Man%20Semantic%20description%20of%20timbral%202016%20Accepted.pdf
4. Plomp & Levelt 1965, Tonal consonance and critical bandwidth — https://www.mpi.nl/publications/item66382/tonal-consonance-and-critical-bandwidth
5. Equivalent rectangular bandwidth (Glasberg & Moore 1990) — https://en.wikipedia.org/wiki/Equivalent_rectangular_bandwidth
6. Geddes interview on distortion perception — https://www.tnt-audio.com/intervis/geddes_e.html
7. Harmonic and intermodulation distortion of mixed signals under compression — https://repository.uwl.ac.uk/id/eprint/2033/
8. Multiband Waveshaping Distortion (DAFx) — https://dafx.de/paper-archive/details/T3H0_pgOrplVcowqobXvpg
9. Chowdhury, ADAA notebook — https://ccrma.stanford.edu/~jatin/Notebooks/adaa.html
10. Giannoulis, Massberg, Reiss 2012, Digital dynamic range compressor design — https://www.academia.edu/31481096/Digital_Dynamic_Range_Compressor_Design_A_Tutorial_and_Analysis
11. Fenton & Lee, A perceptual model of punch — https://eprints.hud.ac.uk/id/eprint/26288/9/AES%20139_Perceptual_Punch_Model_Fenton.pdf
12. Spotify loudness normalisation — https://support.spotify.com/bt/artists/article/loudness-normalization/
13. Pestana, Reiss et al. 2013, Spectral characteristics of popular commercial recordings 1950–2010 — https://repositorio.ucp.pt/entities/publication/814f2b66-7ebc-4815-99c4-73aaa1f0d533/full
14. The evolution of inharmonicity and noisiness in contemporary popular music — https://arxiv.org/pdf/2408.08127
15. De Man, Reiss, Stables 2017, Ten years of automatic mixing — https://www.open-access.bcu.ac.uk/4968/1/WIMP2017_DeManEtAl.pdf
16. Sabin, Rafii, Pardo 2011, Weighting-function-based rapid mapping of descriptors to audio processing parameters (JAES) — https://users.cs.northwestern.edu/~zra446/doc/Sabin-Rafii-Pardo%20-%20Weighting-Function%96Based%20Rapid%20Mapping%20of%20Descriptors%20to%20Audio%20Processing%20Parameters%20-%20JAES%202011.pdf
17. Intelligent control method for the dynamic range compressor: a user study — https://www.academia.edu/120756949/Intelligent_Control_Method_for_the_Dynamic_Range_Compressor_A_User_Study
18. Audio Commons timbral models — https://pypi.org/project/timbral_models/ (method details: Audio Commons deliverable D5.8)
19. Automatic equalization for individual instrument tracks using CNNs (DAFx 2024) — https://arxiv.org/html/2407.16691v1
20. Auto-adaptive resonance equalization using dilated residual networks — https://arxiv.org/abs/1807.08636v1
21. Auditory distance cues in rooms, Frontiers in Psychology 2017 — https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2017.00969/pdf
