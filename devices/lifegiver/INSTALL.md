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

## This build (2026-10-16a): what to check

The first build that shapes sound: Listen, the reading, the stage, the knobs, the tools, Moves and the A–E mix.

1. **Before Listen:** the sound passes untouched; the stage says "Listen first".
2. **Listen:** press **Arm**, play the part (it starts at the first sound and stops by itself after ~4 s of sound).
   Up to 4 takes; a take's **x** removes it. Listen shows the notes heard and how sure it is; the knobs show the
   reading in words.
3. **Role and stage:** click a role (Lead, Bass, …): the shape drops into its place. Drag the shape (pan, distance),
   its side handles (width), the top handle (edge), the range strip's handles. Moves lists what it does.
4. **Knobs:** drag up/down (Shift = fine, double-click = 0). The teal arc is what the stage asks.
5. **A–E:** the bars set each strategy's strength; a letter auditions it alone (click again to stop).
6. **Amount** (0 % = the original), **Compare** (the original, level-matched), **Clean**.
7. **Save and reopen:** the reading and the sound come back exactly (the takes don't: "re-listen to refine").

Send the Max window lines if anything looks wrong, and say what you hear.
