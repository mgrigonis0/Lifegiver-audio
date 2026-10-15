# Conventions

Stable rules for every device in this repo. Device-specific state lives in `devices/<name>/HANDOFF.md`.

## Target and delivery

- Live 11 on an Intel Mac: Max 8 (Live's bundled version), the old ES5 `js` engine (no `let`, arrow functions, classes, template strings).
- Devices are audio effects (audio in → audio out).
- A device ships as one flat folder: the `.amxd`, its JS, gen~ code, abstractions and `INSTALL.md`. Published as a zip on the GitHub release "Latest build" (as in pond-synth). The user downloads, unzips and drops the `.amxd` into Live.
- No third-party packages and no compiled externals: gen~ compiles to native code; unsigned Mac externals need Xcode builds and Gatekeeper workarounds.
- Every build bumps a version string printed to the Max console at load (`<device>: <part> <YYYY-MM-DD><letter>`), so the user can see which files Live loaded.
- After replacing files, the user deletes and re-adds the device: Live caches abstractions.

## Working with the user

- Discuss and propose first; build only after an explicit "go".
- Short replies, short bullets. One decision at a time, and settle each step fully before moving to the next: don't rush ahead.
- Answer the user's questions before doing anything else.
- The UI comes first: show a mockup and get it approved before any final build.
- Keep Live test requests minimal: simulate first; ask for a Live test only when nothing else can tell.
- Commit every build to GitHub; push when the user agrees.

## Naming and isolation

- Every buffer, dict, send/receive, Global and named object that is shared inside a device starts with `---` (Live replaces it with a per-device id). Nothing device-wide may use a fixed name: duplicate devices must never crosstalk.
- UI → `prepend <name>` → one receiver per role (e.g. `---brain`).

## gen~ rules (learned the hard way)

- gen~ can compile a whole engine to zero output with no console error. Keep new code in patterns already proven in a playing engine; if sound disappears, bisect against the last playing version.
- Functions first. Loops only at top level; a loop that should not run gets length 0, not an enclosing `if`.
- Inside loops, all peeks first, then logic.
- No nested ternaries. Declare every local once, at top level, before use.
- In Live, writes (`poke`, History) inside `if` blocks can be dropped. Make writes unconditional; choose the value or the target index with ternaries.
- Don't call user functions with side effects (e.g. an RNG) inside `if` blocks.
- Brain → engine requests: the brain counts a request number up; the engine acts once when the number changes. The engine never writes a slot the brain writes.

## Max JS rules

- Dicts turn 1-element arrays into scalars; no nested arrays, no `null` (use a converter like `toArr`).
- Use `Object.prototype.toString` checks, not `instanceof Array`.
- `save` is a reserved function name.
- Long work runs in chunks via `Task`, so Live never freezes.

## Patching

- A comma in a message box splits the message.
- `[t a a]` can send the symbol "a"; fan out with plain patch cords instead.
