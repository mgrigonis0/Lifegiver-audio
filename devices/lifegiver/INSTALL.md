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
lifegiver: engine 2026-10-17b ready, device ...
lifegiver: patch 2026-10-17b
```

and, a moment later, `lifegiver: new device, id ....` (or `state restored, id ....` in a saved set).
A red `lifegiver~` box or no sound: copy the red lines from the Max window and send them.

## This build (2026-10-17b): what to check

New since 17a: **Clean now tames harshness** it reads in the sound (what sticks out of the sound's own spectrum in
the harsh range), **roles only place** the sound (they no longer add presence, punch or weight), the stage's **Edge
diamond is gone**, the top bar in Out is now **Identity** (how strongly the sound's unasked qualities are held), and
**A–E are big faders**.

1. **Harsh sound:** put Lifegiver on a harsh hi-hat or cymbal loop (or anything piercing), Listen, then move **Clean**
   from 0 to 100 %. The piercing part should calm down; Moves shows "Taming harshness" with its frequency. On a sound
   that isn't harsh (a voice, a pad, an evenly bright hat), Clean should change nothing in the highs.
2. **Edge you ask for wins:** on the harsh sound, turn **Bite** up: the taming steps back (no fight).
3. **Roles:** pick Lead, then Rhythm, then Bass at the same distance: the sound itself shouldn't change any more,
   only its place (distance, width, range).
4. **Identity:** turn a knob far (e.g. Air +8), then move Identity between 0 and 100 %: at 100 % the rest of the sound
   stays closer to how it was.
5. **A–E faders:** drag up/down on a letter (Shift = fine), double-click = 0, a plain click auditions it.
6. **A set saved with 17a:** reopen it: it plays as before; Clean's taming starts after you Listen again.

Send the Max window lines if anything looks wrong, and say what you hear.
