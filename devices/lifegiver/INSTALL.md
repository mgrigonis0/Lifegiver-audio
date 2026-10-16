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
lifegiver: engine 2026-10-17a ready, device ...
lifegiver: patch 2026-10-17a
```

and, a moment later, `lifegiver: new device, id ....` (or `state restored, id ....` in a saved set).
A red `lifegiver~` box or no sound: copy the red lines from the Max window and send them.

## This build (2026-10-17a): what to check

New since 16c: a new **room** (the ringing notes behind the drums are gone), the output now **always matches the
input's loudness** (A–E at 100 % no longer jumps up), and a **Gain** bar under Compare.

1. **Room:** put a drum loop at the back of the stage. You should hear a short, soft space behind it: no held notes,
   no metallic ring. Drag it back and forth while it plays: no clicks.
2. **A–E at 100 %:** turn one up while playing. The level should stay about where it was. Right after a big move it
   can take up to a second to settle; that's normal.
3. **Gain:** drag the bar under Compare (centre = 0 dB, −12 … +12 dB); double-click puts it back to 0.
4. **A set saved with 16c:** reopen it. It plays the new room and is level-matched; knobs and A–E still work.
5. **CPU:** Lifegiver on 4–6 tracks, and what Live's CPU meter says.
6. Anything from the 16c list you didn't get to (Scale 300–400 %, Dyn | Avg switching, the Moves lines).

Send the Max window lines if anything looks wrong, and say what you hear.
