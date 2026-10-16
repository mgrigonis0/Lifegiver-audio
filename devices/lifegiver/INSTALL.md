# Lifegiver — install and first test

For Live 11 with Max 8 on a Mac (Intel or Apple Silicon).

## 1. Download

On the repository's Releases page, open **"Lifegiver — latest build"** and download **lifegiver-mac.zip**. Unzip it.
You get a folder `Lifegiver` with:

- `lifegiver~.mxo`: the engine
- `Lifegiver.amxd`: the Live device (an audio effect)
- `lifegiver.strip.js`: the device's display (keep it next to the `.amxd`)

If you're updating, **quit Live completely first** (Live keeps a loaded engine in memory), replace the whole old
`Lifegiver` folder, and load a fresh `Lifegiver.amxd` from it (a copy saved inside a set keeps its old layout).

## 2. Put the folder where Max can find it

1. In Finder press **Cmd+Shift+G** and go to `~/Documents/Max 8/Library` (create `Library` if it isn't there).
2. Copy the whole `Lifegiver` folder into it.

## 3. Let macOS open the engine (once per download)

Open **Terminal**, paste this and press Enter (nothing is printed when it works):

```
xattr -dr com.apple.quarantine ~/Documents/Max\ 8/Library/Lifegiver
```

## 4. Load it in Live

Drag `Lifegiver.amxd` from that folder onto an audio track that plays something.

The Max window (open the device editor, then Cmd+M) shows two lines when everything loaded:

```
lifegiver: engine 2026-10-16a ready, device ...
lifegiver: patch 2026-10-16a
```

and, a moment later, `lifegiver: new device, id ....` (or `state restored, id ....` in a saved set).
A red `lifegiver~` box or no sound: copy the red lines from the Max window and send them.

## This build (2026-10-16c): what to check

New since 16b: the rest of the sound tools (dynamic EQ, resonance tamer, smoothing, multiband, a second saturation
after the dynamics, a short room for distance), **Scale** (0–400 %) next to Amount, a **Dyn | Avg** switch, and roles no
longer move the pan. 16b wasn't tested on its own, so its checks are folded in here.

1. **Listen:** press **Arm**, play the part (it stops by itself after ~4 s of sound). The sound changes a few seconds
   later; the rest of the tools are measured in the background right after.
2. **Characters A–E** (A Polish, B Warm, C Bold, D Vintage, E Driven: subtle → strong). All at 0 % = no character.
   Compare E at 100 % with all at 0 %, then A–D one at a time: do they get stronger from A to E? A letter auditions it
   alone (click again to stop).
3. **Scale:** 0 % = no shaping (your placement stays), 100 % = as planned, up to 400 %. Past 100 % the level is
   matched to the input and the peaks never pass 0 dB. Try 300–400 % for the "collapse" sounds.
4. **Dyn | Avg** (under the knobs): Avg holds the dynamic tools at their average. Switch while playing: no clicks.
5. **Stage:** pick a few roles: the pan stays where you put it. Drag a sound to the back: you should hear a little room.
6. **Knobs:** Air on a dull sound (bass, pad) moves a little; Texture down smooths grit a little (it can't take
   steady noise out of a held note yet).
7. **Moves:** new lines (e.g. "Easing harshness", "Taming 2 rings", "Holding lows", "Room"); click the list for the
   next page. In Avg the dynamic ones say "held".
8. **Save and reopen:** the knobs, A–E and Scale still work after reopening, without listening again.
9. **CPU:** put Lifegiver on 4–6 tracks and tell me what Live's CPU meter says.

Sets saved with 16a/16b open and keep their sound exactly (the new tools start off until you change something or
listen again). Send the Max window lines if anything looks wrong, and say what you hear.
