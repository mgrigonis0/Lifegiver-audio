# Probes

Measurement programs used while building (not part of CI). Build from this folder:
`g++ -std=c++17 -O2 -I../../source/engine -o /tmp/x <probe>.cpp -lpthread`. Stems: `make_stems.sh`, `make_hats.py`.

- `clickhat.cpp <wav> [Role]`: added clicks at onsets (peak in the first 2 ms, >8 kHz energy in 3 ms, level-matched),
  for the whole plan and one module at a time. Found 17b's hi-hat click (Lead's front distance cue → Punch → transient
  shaper + compressor: worst hits +3…+5 dB in the highs).
- `reachcheck.cpp <stems dir>`: first-plan and solve time, A–E reach per stem (compare builds).
- `levelcheck.cpp <stems dir> [0|1]`: a character switched on mid-play on the fast path; output vs input loudness,
  settled and worst 400 ms (live level match on/off).
- `zone.py <before.wav> <after.wav>`: change of the 2.5–5.5 kHz zone, above and below it (Clean's taming).
- `render file <wav> <out> clean=40 role=Rhythm …` (tools/render.cpp) prints the reading incl. "sticks out".
