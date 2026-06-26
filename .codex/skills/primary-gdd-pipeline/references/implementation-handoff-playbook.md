# Implementation Handoff Playbook

Load this file only when preparing a GDD-to-implementation handoff, next-chat
prompt, first playable slice packet, or production-ready task list.

## Handoff Goal

The next agent should be able to start implementation without rereading the
whole design history. The handoff must name the source-of-truth files, the first
playable slice, exact exclusions, validation commands, and manual proof.

## Source-Of-Truth Order

List files in the order the implementation agent should read them:

1. Current decision log or `handoff_status.md`.
2. Concept/GDD summary.
3. Balance/economy JSON.
4. UI flow/components JSON.
5. Asset manifest and visual proof.
6. Runtime/build/test docs.

Mark stale files explicitly. Do not make the implementation agent guess which
document wins.

## First Playable Slice Packet

Write the slice as implementation work, not design prose:

- player starts at:
- screen shown first:
- available actions:
- currencies/stats:
- first activity/job:
- first reward:
- first upgrade:
- first visual/status change:
- save/load expectation:
- out of scope:

If any bullet is vague, the slice is not ready.

For RPG/adventure/survival/tactics slices, also include:

- first enemy/obstacle:
- player actions and exact effects:
- enemy/check actions and exact effects:
- win/loss/retreat outcomes:
- recovery path:

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

## Build And Test Packet

Include commands exactly as discovered locally:

```markdown
## Commands
- Build:
- Unit/data validation:
- Native/desktop run:
- Web run if needed:
- Visual/input audit:
```

If commands are unknown, write how to discover them and what files to inspect.

## Next-Chat Prompt Template

```markdown
Use the already-loaded project rules. Implement the first playable slice from:

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

## Handoff Failure Modes

- Too many phases before the player can click the core action.
- Lore is clear but state/economy is not.
- Visual target exists but asset ids/paths are missing.
- Commands are aspirational instead of discovered locally.
- Acceptance criteria say "feels good" without a screenshot or input proof.
- The next prompt omits forbidden files or ignored generation folders.
- Combat/challenge is named in UI but has no numbers or outcomes.
