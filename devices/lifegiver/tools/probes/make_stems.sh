#!/bin/sh
# Test stems for the probes (CC-licensed, from github.com/librosa/data; never commit the WAVs).
#   git clone --depth 1 https://github.com/librosa/data /tmp/librosa-data   (GIT_LFS_SKIP_SMUDGE=1 in this environment)
#   sh make_stems.sh /tmp/librosa-data/audio <out dir>
# Then: python3 make_hats.py <out dir>   (synthetic hats: hat_pierce = +9 dB bump at 4.3 kHz, hat_even = evenly
# bright, hat_harsh = rings at 4.2/7.5 kHz + a 3.8 kHz zone; hat_real = the hats of drumbass, high-passed at 5 kHz)
A="$1"; O="$2"; mkdir -p "$O"
cv() { ffmpeg -v error -y -i "$A/$1" $3 -t 20 -ar 44100 -ac 2 -c:a pcm_s24le "$O/$2.wav"; }
cv admiralbob77_-_Choice_-_Drum-bass.hq.ogg drumbass
cv Kevin_MacLeod_-_Vibe_Ace.hq.ogg jazzmix
cv "Hungarian_Dance_number_5_-_Allegro_in_F_sharp_minor_(string_orchestra).hq.ogg" strings
cv sorohanro_-_solo-trumpet-06.hq.ogg trumpet
cv 198-209-0000.hq.ogg voice
cv snare-accelerate.hq.ogg snare
i=0; for n in sax clarinet trumpet2 piano synthstrings; do cv drese-midi.hq.ogg $n "-af pan=mono|c0=c$i"; i=$((i+1)); done
ffmpeg -v error -y -i "$O/drumbass.wav" -af "highpass=f=5000:poles=2,highpass=f=5000:poles=2,volume=12dB" -ar 48000 "$O/hat_real.wav"
