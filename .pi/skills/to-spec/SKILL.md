---
name: to-spec
description: Turn the current conversation into a spec and create it as an epic in the AI Studio Taskboard — no interview, just synthesis of what was already discussed.
disable-model-invocation: true
---

# To Spec — Taskboard epic edition

Take the current conversation and codebase understanding and produce a spec.
**Do NOT interview the user** — synthesize what you already know. If the design
is still open, the conversation was not grilled enough; say so and stop.

The spec lands as an **epic** in the Taskboard, under the owning project. The
Taskboard is the single source of truth for current work — never publish specs
to `.scratch/`, GitHub issues, or a loose Markdown file.

Store selection: pass `--game <game-id>` on every command for game work;
private game stores are invisible without it.

## 1. Orient

```
node ai_studio/taskboard/cli.mjs context --json --game <game-id>
```

Find the owning project. Read one or two existing epics to match their voice
and granularity:

```
node ai_studio/taskboard/cli.mjs show E001 --json --game <game-id>
```

## 2. Explore the repo

Understand the current state of the code if you have not already. Use the
project's domain vocabulary throughout, and respect `CONVENTIONS.md`, the
game's `design/` docs, and any locked visual or design contracts you find.

## 3. Agree the seams

Sketch the seams at which the feature will be tested. Prefer existing seams to
new ones, and the highest seam available — the fewer seams across the codebase
the better, and one is ideal.

**Check with the user that these seams match their expectations** before
writing the epic. This is the one thing you ask about; everything else is
synthesis.

## 4. Create the epic

```
node ai_studio/taskboard/cli.mjs new epic --title "..." --project P00N --priority P2 --game <game-id>
```

Then write its body using the shape below. `Goal`, `In scope`, `Out of scope`
and `Log` match the existing epics; the two decision sections carry the parts
of a spec that would otherwise be lost.

<epic-body>

## Goal

The problem from the player's perspective, and the solution from the player's
perspective. Two or three paragraphs, no implementation detail.

## In scope

- The capabilities this epic delivers, as user-visible behaviour.
- Written so each bullet can become one or more tracer-bullet tickets.

## Out of scope

- What this epic deliberately does not cover, so later tickets do not drift
  into it.

## Implementation decisions

- Modules built or modified, and the interfaces that change.
- Architectural decisions, schema changes, contracts, specific interactions.
- Technical clarifications the developer gave during grilling.

No file paths and no code snippets — they go stale fast. Exception: a snippet
that encodes a decision more precisely than prose can (state machine, schema,
type shape); inline only the decision-rich part and note it came from a
prototype.

## Testing decisions

- The agreed seams, and why each was chosen.
- Which modules get tested, and prior art for those tests in this repo.
- What makes a good test here: external behaviour only, never internals.

## Log

- <YYYY-MM-DD>: Created from a grilling session.

</epic-body>

## 5. Hand off

Report the created epic ID. Do not create tickets here — that is `/to-tickets`,
which reads this epic and breaks it into tracer-bullet slices under it.
