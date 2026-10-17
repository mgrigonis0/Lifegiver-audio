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
lifegiver: engine 2026-10-17c ready, device ...
lifegiver: patch 2026-10-17c
```

and, a moment later, `lifegiver: new device, id ....` (or `state restored, id ....` in a saved set).
A red `lifegiver~` box or no sound: copy the red lines from the Max window and send them.

## This build (2026-10-17c): what to check

New since 17b: the **width stays the sound's own** (roles no longer set it; the bubble's centre width is "own width"),
the stage's **back wall sets the room** (small / room / large), **Clean is firmer**, **hi-hats no longer click**,
Moves shows what **Identity holds**, and a re-plan after a tweak **glides in** instead of jumping.

1. **Hi-hat clicks:** the hi-hat that got clicky in 17b, as Lead: the hits should sound like the original (no added
   tick). The Punch knob reads "—" on a hat (there is no punch to shape there).
2. **Clean:** on the same hat, Clean 0 → 40 % → 100 %: the piercing part comes down clearly by 40 %. A voice, a pad or
   an evenly bright hat: still nothing changes.
3. **Width:** pick a few roles: the width stays where it is ("own width"). Drag a handle in or out: a faint outline
   shows the sound's own width; double-click a handle to go back to it.
4. **Room:** drag the sound to the back half, then drag the tab on the back wall: pulled forward = a small, drier room;
   pushed back = a large one. In the front half the room stays dry. Double-click the wall: back to "room".
5. **Smoother tweaks:** turn knobs and wait: when the device re-plans (about 1 s after you stop) the sound should
   settle smoothly, and no knob should turn grey while you tweak.
6. **Identity:** Air +8 with Identity at 100 %: Moves may show "Holding Bite" (or another quality) — that's Identity
   keeping it where the sound had it.
7. **A set saved with 17b:** it opens and plays; the room is as before.

Send the Max window lines if anything looks wrong, and say what you hear.
