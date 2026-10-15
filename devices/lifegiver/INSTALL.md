# Lifegiver — install and first test

For Live 11 with Max 8 on a Mac (Intel or Apple Silicon).

## 1. Download

On the repository's Releases page, open **"Lifegiver — latest build"** and download **lifegiver-mac.zip**. Unzip it.
You get a folder `Lifegiver` with:

- `lifegiver~.mxo`: the engine
- `Lifegiver.amxd`: the Live device (an audio effect)

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
lifegiver: engine 2026-10-15b ready, device ...
lifegiver: patch 2026-10-15b
```

and, a moment later, `lifegiver: new device, id ....` (or `state restored, id ....` in a saved set).
A red `lifegiver~` box or no sound: copy the red lines from the Max window and send them.

## First build: what to check

This build only passes the sound through; the shaping comes in later builds.

1. **Sound passes:** the track sounds exactly as without the device.
2. **Amount:** turning it changes nothing yet (there is nothing to blend in), and it never clicks.
3. **Version:** the two lines above show the same build.
4. **Two devices:** put a second Lifegiver on another track: each gets its own id in the Max window.
5. **Save and reopen:** note the id, save the set, close it, reopen it: the Max window says
   `state restored, id ....` with the same number.
