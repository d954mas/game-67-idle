# Implementation Handoff

Load this when preparing a GDD-to-implementation handoff, next-chat prompt,
first playable slice packet, or implementation-ready task list.

## Goal

The next agent should start implementation without rereading chat history. The
handoff must name source files, first playable scope, exclusions, validation
commands, and manual proof.

## Source Order

List files in the order the implementation agent should read them:

1. Current decision log or handoff status.
2. Concept/GDD summary.
3. Balance/economy JSON.
4. UI flow/components JSON.
5. Asset manifest and visual proof.
6. Runtime/build/test docs.

Mark stale files explicitly. Do not make the next agent guess which document
wins.

## First Playable Packet

Write implementation work, not design prose:

- player starts at;
- screen shown first;
- available actions;
- currencies/stats;
- first activity/job;
- first reward;
- first upgrade;
- first visual/status change;
- save/load expectation;
- out of scope.

For challenge slices, also include:

- first enemy/obstacle;
- player actions and exact effects;
- enemy/check actions and exact effects;
- win/loss/retreat outcomes;
- recovery path.

If any bullet is vague, the slice is not ready.

## Task Breakdown

Prefer 3-6 small phases:

1. Data/state schema.
2. Core loop reducer or model.
3. UI screen and controls.
4. Visual assets/manifest wiring.
5. Persistence or reset if in scope.
6. Automation, screenshots, and acceptance tests.

Each phase needs a visible or testable result.

## Acceptance Gates

Use concrete checks:

- player can perform the core action;
- reward changes a visible stat/currency;
- upgrade changes output or unlock state;
- UI explains next goal without external docs;
- first visual/status change happens in-session;
- restart/save behavior matches spec;
- screenshot proves nonblank readable UI;
- emulated input proves the main action path.

## Next Prompt Shape

```markdown
Implement the first playable slice from:

1. [source file]
2. [source file]
3. [source file]

Goal:
[one paragraph]

Scope:
- Must implement:
- Out of scope:

Validation:
- Run:
- Capture:
- Manual proof:

Do not edit:
- [engine/submodule/generated/raw folders]
```
