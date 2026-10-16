# Lifegiver — Research notes

_2026-10-15 (corrected after `REVIEW.md`: punch, distance, loudness, timbral models); 2026-10-17: mix critiques, reverb amount, dynamics preference, pleasantness. Theory the design must respect.
Paraphrased; sources at the end._

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
- Perceived **punch** is modelled from transient energy in the 64 Hz–1 kHz octaves; punch fell as band frequency rose up to a pivot near 1 kHz. The 2–8 kHz bands trended upward again and were left for further work (not "presence"). Longer onsets lower punch, more so in upper bands. [11]

**For Lifegiver:** Punch acts on low/low-mid transients with short onsets; what happens above 1 kHz is untested, so
Bite/presence stays a separate control and any upper-band punch effect is measured on our stems. Time constants on low frequencies must stay longer than the period of the lowest note (the reading gives F0) or the dynamics distort.

## Loudness and today's standards
- Loudness is measured per ITU-R BS.1770 (K-weighting, LUFS). Spotify normalises to −14 LUFS and asks for true peak under −1 dBTP. Louder is not a win after normalisation. [12] These are delivery specs for the finished master, not per-track goals: Live mixes tracks in floating point, so a channel device needs no limiter.
- Commercial recordings converge on an industry target spectral curve. [13]
- Popular music since 1986 is less noisy than before but still far more inharmonic than orchestral music (layering, distortion); producers seem conservative in the most sensitive ear range. [14]
- Modern quality baseline (industry practice): oversampled or antialiased nonlinearities, dynamic (signal-dependent) EQ, gain-matched A/B, mono-safe lows.

**For Lifegiver:** BS.1770 is the measuring method. Make-up gain (static, from the closed loop on the takes) removes
accidental level changes, so any change is judged on tone, not level; Compare is always loudness-matched.

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

- They are absolute 0–100 regressions; an independent check (DAGA 2025) covered only sharpness (r ≈ 0.73 against
  brightness ratings) and roughness (r ≈ 0.67); the others are unverified. [22]

**For Lifegiver:** close to the knob set, but a starting point only: each relative measure is verified on the test
stems; expect warmth and depth to need our own definitions.

## Targets per instrument, resonances
- Automatic EQ toward instrument-specific average spectra (zero-mean dB, ±12 dB limit) was preferred about 2:1 over the original in a small blind test; the authors argue one flat target can't suit all instruments, but didn't test it. [19]
- Resonance taming as a separate step: detect spectral resonances, attenuate by an amount; a network learned the amount engineers preferred. [20]

**For Lifegiver:** the role sets what the target is; resonance taming is a cleanup step with one amount.

## Distance (depth)
- In rooms at 1–6 m, listeners judged distance mainly from overall level; direct-to-reverberant ratio (DRR) was an unreliable cue; broadband sounds were judged best; strong low-frequency reverb misleads. [21] Caveat: level varied with distance there, as in a real room, and level and DRR were highly correlated, so the study can't separate them.
- When both cues are available, discrimination is best; level is generally more accurate, but DRR is useful for far sources and in reverberant rooms, and works when level is not available. [23]

**For Lifegiver:** level is kept small (0 to −3 dB; the fader owns level), so "back" leans on lower presence, darker top,
softer attack, smaller level jumps and a little short room; not just more reverb. Depth is a stage position, not a knob.

## What engineers' mixes and critiques show (added 2026-10-17)
- Peer reviews of real mixes (1,397 comments): about three quarters are criticisms. Most mentioned: the mix as a whole,
  then vocals, then drums (snare and kick most). Of space-related remarks, most are about reverb. Recurring complaints:
  too much reverb, vocal level, muddy low-mids, thin or dark drums and bass, kick lacking punch. [24]
- Reverb amount across many mixes is predicted by two numbers: reverb loudness relative to the dry mix, and early
  decay time. Mixes at −9 LU reverb or more were all judged too wet; the authors suggest about −14 LU as the more desirable level.
  The window is narrow (the just-noticeable step is ~5–6 dB). Long tails read as too wet even at low level; strong
  but short reverb read as dry. [25]
- Mixers reduce masking by panning, mirrored EQ, or splitting the spectrum; in a test with one-slider tools, people
  were most satisfied with the ones that panned the two sounds apart. [26] Mix clarity drops when noise-like content
  is masked or adds masking; added reverb and noise lowered predicted clarity. [27]
- In rock premixes made on good monitoring, louder tracks were darker and quieter tracks brighter (mixers manage the
  whole mix's balance); on poor monitoring the pattern flipped. [28]

**For Lifegiver:** the room is the most criticised kind of processing, so it must be subtle and short: at most
around −14 LU relative reverb loudness (that study measured whole mixes; one track's room should be quieter still) and
a short decay; a long tail is the main fault, more than the level. Muddy
low-mids, thin drums and weak kick punch are the common complaints: Warm/Weight/Punch should aim there. Separation
through pan is the preferred fix, and pan is the user's call: the device can suggest, not move it.

## Dynamics: what listeners prefer (added 2026-10-17)
- Small amounts of compression were preferred when level was free to rise; heavy compression lowered quality even at
  matched loudness. [29]
- Lowering crest factor by limiting (15 → 9 dB) changed both loudness and quality ratings; how much louder correlated
  with better depended on genre (yes for rock and jazz, not for electronic). [30]
- Moderate compression on individual tracks was preferred over the same on subgroups or on the whole mix; heavy
  limiting on a sum was disliked most (it makes quiet parts suffer with the loud ones). [31]
- Experts asked to make sounds punchy with multiband waveshaping produced settings that correlated with punch ratings,
  supporting band-limited shaping as a punch tool. [32]

**For Lifegiver:** a per-track device is the right place for dynamics. Keep the dynamic amounts moderate by default;
large reductions should only come from an explicit ask (Scale, a strong character). Judge every change
loudness-matched: an unmatched louder result is preferred for the wrong reason.

## What the ear finds pleasant or unpleasant (added 2026-10-17)
- The most unpleasant sounds cluster at **2.5–5.5 kHz** with **slow modulation (1–16 Hz)**: the ear's most sensitive
  region, shared with screams. [33]
- **Sharpness** (energy weighted toward high frequencies, steeper upward envelope) is the strongest single timbre
  dimension of steady sounds; fine spectral detail matters little. [34] Psychoacoustic annoyance rises with loudness,
  with sharpness above ~1.75 acum, and with roughness and fluctuation. [35]
- **Roughness**: amplitude modulation of ~20–150 Hz, strongest near 70 Hz; below ~20 Hz it is heard as wavering
  (fluctuation, peak near 4 Hz); above ~150 Hz it fuses into a smooth tone. [36]
- **Harmonicity** predicts liking better than absence of beating: across 250+ listeners, people who preferred harmonic
  over inharmonic tones also preferred consonant chords; beating preference didn't predict it. Training strengthens
  the harmonicity preference. [37] Pleasant intervals depend on timbre (bell-like spectra have their own
  consonances), and listeners liked slight deviations from perfect tuning. [38]
- Clarity of a single sound follows its centroid relative to its own F0 and its "unpleasantness" from sharp spectral
  peaks; in a mix, how audible the target's peaks are also matters. [39]
- Music is liked most at moderate complexity: uncertain but resolving. [40] Very low bass (8–37 Hz, not consciously
  heard) increased dancing at a concert by ~12 %. [41]
- Natural-sounding reverb tails are dense (1,000+ echoes/s; ~10,000 for impulsive sounds), smooth in frequency with
  evenly spread modes, and decay faster in the highs. [42] Metallic ringing in feedback reverbs comes from strongly
  recirculating short delays and comb-like (diagonal) feedback; dense mixing matrices and even modal excitation give a
  colourless tail. [43][44]

**For Lifegiver:**
- **Not sameness.** These findings describe averages; character lives in the exceptions (grit, edge, a ring, a
  rough texture). Pleasantness is never a target every sound is pushed toward: it only stops the device from adding
  unpleasantness the user didn't ask for. What the sound already has stays (Identity), and asks for edge, grit or
  drive override it.
- A "pleasantness guard" is measurable: energy and slow modulation at 2.5–5.5 kHz, sharpness, roughness (20–150 Hz
  envelope modulation per band) and sharp spectral peaks. Characters and boosts should not raise these unasked; Polish
  can actively lower them. This extends the harshness tamer and the resonance tamer, not a new module.
- Saturation should keep harmonicity: low orders on harmonic sounds, little on inharmonic ones (already the rule);
  "a little imperfection" suggests no need to be clinical.
- Width/pan, low-mid mud and short room are where mix critiques land, so they deserve the most care.
- The current room's single feedback comb breaks every reverb quality rule above: it must become a small dense
  network with a short, darker-in-the-highs decay.

## Sources
1. McAdams et al. 1995, Perceptual scaling of synthesized musical timbres — https://three-mode.leidenuniv.nl/pdf/m/mcadams_etal1995pr.pdf
2. Zacharakis, Pastiadis, Reiss 2014, An interlanguage study of musical timbre semantic dimensions — https://sonicfield.org/library/an-interlanguage-study-of-musical-timbre-semantic-dimensions-and-their-acoustic-
3. Stables, De Man, Reiss et al., Semantic description of timbral transformations (SAFE) — https://qmro.qmul.ac.uk/xmlui/bitstream/123456789/22150/1/De%20Man%20Semantic%20description%20of%20timbral%202016%20Accepted.pdf
4. Plomp & Levelt 1965, Tonal consonance and critical bandwidth — https://www.mpi.nl/publications/item66382/tonal-consonance-and-critical-bandwidth
5. Equivalent rectangular bandwidth (Glasberg & Moore 1990) — https://en.wikipedia.org/wiki/Equivalent_rectangular_bandwidth
6. Geddes interview on distortion perception — https://www.tnt-audio.com/intervis/geddes_e.html (to replace with the citable Geddes & Lee AES papers on perception of nonlinear distortion, 2003)
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
22. Czedik-Eysenberg & Reuter, DAGA 2025 (timbral feature check) — https://homepage.univie.ac.at/christoph.reuter/unterwegs/DAGA2025_Czedik-Eysenberg_Reuter_PAPER.pdf
23. Kolarik, Cirstea, Pardhan 2011, Perceiving auditory distance using level and direct-to-reverberant ratio cues — https://ueaeprints.uea.ac.uk/id/eprint/81360
24. De Man & Reiss 2015, Analysis of peer reviews in music production (J. Art of Record Production 10) — https://www.arpjournal.com/asarpwp/analysis-of-peer-reviews-in-music-production/
25. De Man, McNally, Reiss 2017, Perceptual evaluation and analysis of reverberation in multitrack music production (JAES) — https://www.academia.edu/31482005/Perceptual_Evaluation_and_Analysis_of_Reverberation_in_Multitrack_Music_Production
26. An investigation into the efficacy of methods commonly employed by mix engineers to reduce frequency masking (Huddersfield) — https://pure.hud.ac.uk/en/publications/an-investigation-into-the-efficacy-of-methods-commonly-employed-b/
27. Musical mix clarity prediction using decomposition and perceptual masking thresholds — https://arxiv.org/pdf/2103.12152
28. Deruty & Pachet, rock premixes: loudness vs brightness by monitoring conditions — https://www.francoispachet.fr/wp-content/uploads/2021/01/deruty-14c.pdf
29. Croghan, Arehart, Kates 2012, Quality and loudness judgments for music subjected to compression limiting (JASA) — https://labs.sonicfield.org/library/quality-and-loudness-judgments-for-music-subjected-to-compression-limiting
30. Wendl & Lee 2014, The effect of dynamic range compression on loudness and quality perception in relation to crest factor (AES 136) — https://pure.hud.ac.uk/en/publications/the-effect-of-dynamic-range-compression-on-loudness-and-quality-p/
31. Listener preferences for alternative dynamic-range-compressed audio configurations (UWL) — https://repository.uwl.ac.uk/id/eprint/4271/
32. Elicitation and objective grading of "punch" within produced music (Huddersfield) — https://pure.hud.ac.uk/en/publications/elicitation-and-objective-grading-of-punch-within-produced-music/
33. Kumar, von Kriegstein, Friston, Griffiths 2008, Features versus feelings: mapping unpleasantness of sounds (J. Neurosci); Kumar et al. 2012 — https://iro.uiowa.edu/esploro/outputs/journalArticle/Mapping-unpleasantness-of-sounds-to-their/9984304036602771 , https://www.ncl.ac.uk/press/articles/archive/2012/10/nastynoiseswhydowerecoilatunpleasantsounds.html
34. von Bismarck 1974, Sharpness as an attribute of the timbre of steady sounds (Acustica) — https://www.ingentaconnect.com/contentone/dav/aaua/1974/00000030/00000003/art00006
35. Zwicker & Fastl, psychoacoustic annoyance (summary) — https://www.johndcook.com/blog/2016/07/28/quantifying-how-annoying-a-sound-is/
36. Roughness and fluctuation strength (Fastl & Zwicker, summary) — https://metricgate.com/docs/roughness-asper/
37. McDermott, Lehr, Oxenham 2010, Individual differences reveal the basis of consonance (Current Biology) — https://mcdermottlab.mit.edu/papers/McDermott_Lehr_Oxenham_2010_consonance_individual_differences.pdf
38. Marjieh et al. 2024, timbre and consonance (Nature Communications; Cambridge summary) — https://www.cam.ac.uk/research/news/pythagoras-was-wrong-there-are-no-universal-musical-harmonies-study-finds
39. Surrey IoSR, Metering the perceived quality of mixed music (clarity) — https://iosr.surrey.ac.uk/projects/mixQuality
40. Gold et al. 2019, Predictability and uncertainty in the pleasure of music (summary) — https://m.medicalxpress.com/news/2019-10-brain-favorite-music.html
41. Cameron et al. 2022, Undetectable very-low-frequency sound increases dancing at a live concert (Current Biology; summary) — https://www.technologynetworks.com/neuroscience/news/bass-gets-us-grooving-even-if-we-cant-actually-hear-it-368118
42. J. O. Smith, Desired qualities of late reverberation (PASP) — https://ccrma.stanford.edu/~jos/pasp/Desired_Qualities_Late_Reverberation.html
43. Dal Santo et al. 2024, Differentiable feedback delay network for colorless reverberation — https://arxiv.org/html/2402.11216v2
44. Heldmann & Schlecht 2021, The role of modal excitation in colorless reverberation (DAFx) — https://dafx.de/paper-archive/details/0K-_d1jqrD9QKWZOpfwqPA
