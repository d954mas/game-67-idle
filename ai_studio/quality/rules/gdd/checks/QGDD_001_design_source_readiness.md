---
id: QGDD_001
name: Design Source Readiness
group: gdd
description: Use when a design source package is meant to guide implementation, review, continuation, or an explicit lead decision.
---

# QGDD_001 Design Source Readiness

## What It Checks

The design source package can act as the current source of truth for the next
reader: lead, designer, implementer, reviewer, or future agent.

A design source package can include markdown docs, tables, JSON/data contracts,
references, visual proof, web-GDD pages, and implementation handoff files.

## Use When

Use when writing, editing, or promoting GDD, concept, requirements, data
contracts, tables, implementation specs, handoff, or acceptance material that
another agent or human should implement, review, continue from, or decide on.

## Do Not Use For

- player-facing clarity;
- art direction or asset readiness;
- runtime/build behavior;
- detailed economy, balance, or progression validation;
- gameplay-loop quality by itself;
- scratch brainstorming that is not being promoted as project truth.

## Check

- entrypoint/source order is clear enough to know where the next reader starts;
- changed files have clear roles: current source of truth, handoff, draft,
  reference, data contract, table, proof, or scratch;
- build target and scope are explicit;
- target player or use case is clear enough to guide decisions;
- current decisions and open questions are separated;
- markdown, tables, JSON/data contracts, web-GDD, visual proof, and handoff do
  not contradict each other;
- numeric and structured facts have one clear owner when prose and data files
  both mention them;
- acceptance/proof states what would demonstrate success: screenshot, runtime
  behavior, playable slice, data file, review, or lead decision;
- implementation-ready claims do not depend on chat-only decisions.

## Evidence

Entrypoint path, changed source file, table/data path, decision/open-question
note, referenced proof, acceptance statement, or implementation handoff path.

## Not Enough

- A broad concept pitch with no build target, entrypoint, or next reader.
- A draft/table/data file is treated as source of truth without saying what is
  still open.
- Implementation notes hide unresolved design decisions.
- Markdown, table/data contract, web-GDD, proof, or handoff contradict each
  other.
- Acceptance criteria cannot be verified.
- "Ready for implementation" depends on decisions that exist only in chat.
