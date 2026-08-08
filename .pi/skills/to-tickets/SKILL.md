---
name: to-tickets
description: Break a plan, spec, or the current conversation into tracer-bullet tickets and create them in the AI Studio Taskboard, each declaring its blocking edges.
disable-model-invocation: true
---

# To Tickets — Taskboard edition

Break a plan, spec, or conversation into **tickets** — tracer-bullet vertical
slices, each declaring the tickets that **block** it — and create them in the
Taskboard.

**The Taskboard is this repo's single source of truth for current work.** Never
write tickets to `.scratch/` or any other parallel location. A second task store
fragments the work and both stores rot.

Store selection: pass `--game <game-id>` on every command when the work belongs
to a game (private game stores are excluded otherwise). Omit it only for Studio
work under `ai_studio/`.

## 1. Orient

Read the current state before adding anything:

```
node ai_studio/taskboard/cli.mjs context --json --game <game-id>
```

This gives current work (`todo`, `doing`, `review`) and the ready queue. Find
the owning project and epic. If the work needs a new epic, note that — do not
create it yet.

## 2. Explore the codebase

If you have not already explored the code, do so. Ticket titles and bodies must
use the project's domain vocabulary and respect the conventions in
`CONVENTIONS.md` and the game's `design/` docs.

Look for prefactoring that makes the implementation easier. "Make the change
easy, then make the easy change." Prefactoring goes first, as its own ticket.

## 3. Draft vertical slices

Break the work into **tracer bullet** tickets.

- Each slice cuts a narrow but COMPLETE path through every layer — vertical,
  NOT a horizontal slice of one layer.
- A completed slice is demoable or verifiable on its own.
- Each slice fits in a single fresh context window.

Give each ticket its **blocking edges** — the tickets that must complete before
it can start. A ticket with no blockers can start immediately.

**Wide refactors are the exception.** When one mechanical change breaks
thousands of call sites at once and no vertical slice can land green, sequence
it as expand–contract: add the new form beside the old, migrate call sites in
batches sized by blast radius (each batch its own ticket blocked by the expand),
then delete the old form in a final ticket blocked by every batch.

## 4. Quiz the user

Present the proposed breakdown as a numbered list. For each ticket show its
title, what it makes work end to end, and its blockers. Get approval before
creating anything. Do not create half the list and ask about the rest.

## 5. Create them

Create in dependency order — blockers first, so each later ticket can reference
real IDs.

Ensure the epic exists first if needed:

```
node ai_studio/taskboard/cli.mjs new epic --title "..." --project P00N --game <game-id>
```

Then one command per ticket:

```
node ai_studio/taskboard/cli.mjs new task --title "..." --project P00N --epic E00N --priority P1 --game <game-id>
```

Then fill each ticket's body with the template below. Record blockers by their
real Taskboard IDs, which you now have.

<ticket-body>

## What to build

The end-to-end behaviour this ticket makes work, from the player's perspective —
not a layer-by-layer implementation list.

## Acceptance criteria

- [ ] Criterion 1
- [ ] Criterion 2

## Blocked by

`T00NN`, `T00NN` — or "None — can start immediately".

</ticket-body>

Avoid specific file paths and code snippets; they go stale fast. Exception: a
snippet that encodes a decision more precisely than prose can (state machine,
schema, type shape) — inline the decision-rich part only.

Do not modify any parent epic or project item.

## 6. Hand off

Report the created IDs and which ones are on the **frontier** — every ticket
whose blockers are all closed. Those can start immediately; `/implement` picks
from there.

Mark a ticket in progress when work starts:

```
node ai_studio/taskboard/cli.mjs set T00NN --status doing --log "..." --json --game <game-id>
```
