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

## This build (2026-10-16b): what to check

Changes since 16a: no take audio is kept (nothing is written to your computer); A–E are characters of rising strength;
a narrower strip (706 px); Moves pages; Air and the other tone knobs work on dull sounds; Stage roles in a new order.

1. **Listen:** press **Arm**, play the part (it stops by itself after ~4 s of sound). The first solve measures what
   every tool does to this sound (a few seconds; the Max window says when it is done).
2. **Characters A–E** (A Polish, B Warm, C Bold, D Vintage, E Driven: subtle → strong). All at 0 % = no character
   (the default, only what the stage and knobs ask). Raise one to hear its colour; blend several. A letter auditions
   it alone (click again to stop). Try E at 100 % vs all at 0 %: the difference should be clear.
3. **Knobs:** Air on a dull sound (bass, pad) should now move (a gentle lift); only a truly empty band greys out.
4. **Stage:** roles from the top: Bass, Rhythm, Hook, Lead, Support, Bed, FX.
5. **Moves:** when there are more than fit, click the list to see the next page ("1/2 more ›").
6. **Save and reopen:** the knobs and A–E still work after reopening, without listening again.
7. The layout: readable, nothing clipped, TONE over the orange half and DYNAMICS over the blue half.

Sets saved with 16a keep their reading and sound. Send the Max window lines if anything looks wrong, and say what you
hear.
