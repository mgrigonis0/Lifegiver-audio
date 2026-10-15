# Handoff Guidelines

Rules for maintaining per-device handoff files. Claude: follow these whenever asked to read, update, or rewrite a handoff.

## Repo layout

```
CONVENTIONS.md                  # stable rules shared by all devices (naming, JS style, Live API patterns)
HANDOFF_GUIDELINES.md           # this file
devices/<device-name>/HANDOFF.md  # current state of one device
```

## Core principle

A handoff describes **the device as it is right now**, so a fresh chat can continue work without any earlier conversation. It is a snapshot, not a log. Git holds the history.

## Starting a new chat

1. Read `CONVENTIONS.md` and the device's `HANDOFF.md`.
2. Read only the files listed under "Next step" and "Key files" that the task actually needs.
3. Do not read the rest of the repo unless the task requires it.
4. If the handoff contradicts the code, trust the code, and point out the mismatch.

## When to rewrite the handoff

Rewrite when the user asks, and suggest a rewrite at these moments:
- A feature is finished and working.
- A significant decision was made (architecture, data structure, Live API approach).
- The chat has grown long or messy and should be restarted.
- Ending a session.

Do **not** rewrite every few messages or for small tweaks.

## Rewrite rules

1. **Rewrite the whole file, don't append.** Produce a fresh version of the current state.
2. **Check against the repo first.** Verify file names, parameters, and behavior against the actual code before writing. Never write the handoff from conversation memory alone.
3. **Current state only.** No "first we tried…, then…". No session diary.
4. **Keep the why.** Every non-obvious decision gets one line of reasoning, especially fixes for timing, observer, threading, or init-order bugs.
5. **List rejected approaches briefly.** One line each: what and why not. This prevents re-proposing them.
6. **Remove what is resolved.** Fixed bugs and finished tasks are deleted, unless the fix created a rule worth keeping (then move it to Decisions).
7. **Reference, don't paste.** Point to files and functions by path and name. No large code blocks; a short signature or snippet only when essential.
8. **Promote shared rules.** If something applies to all devices, move it to `CONVENTIONS.md` instead of repeating it in each handoff.
9. **Stay short.** Aim for one screen (~60–100 lines). If it is longer, it contains history or detail that belongs in code comments.
10. **Be specific about the next step.** Name the task, the files involved, and any open question.

## Committing

- Commit the handoff together with the code it describes, or immediately after.
- Commit message: `handoff(<device>): <one-line summary of current state>`
  - e.g. `handoff(arp): scale-follow working, swing next`

## Template

```markdown
# <Device Name> — Handoff

_Last updated: YYYY-MM-DD_

## What it does
2–4 sentences. What the device is for and how the user interacts with it.

## Current state
- Working: ...
- Partially working: ...
- Not started: ...

## Structure
- `path/to/main.amxd`: role
- `path/to/logic.js`: role
- Signal/data flow in 2–3 lines.

## Parameters
| Name | Range | Purpose |
|------|-------|---------|

## Decisions (and why)
- Decision: reason.

## Rejected approaches
- Approach: why not.

## Known issues
- Issue: where it shows up, suspected cause.

## Next step
Task, files involved, open questions.
```

## Short prompts for the user

- **Start:** "Read CONVENTIONS.md and devices/<name>/HANDOFF.md, then let's continue."
- **End / restart:** "Rewrite the handoff per HANDOFF_GUIDELINES.md and commit it."
- **Quick restart mid-feature (nothing settled yet):** "Give me a short summary of where we are" — paste it into the new chat, and do the real handoff rewrite once the feature lands.
