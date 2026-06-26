# Task Store Reference

Detailed task protocol for work that changes tasks, epics, or reporting shape.
Start from `ai_studio/taskboard/README.md`; load this file only when editing
or auditing the task store.

## Intent To Scope

Users describe work naturally; do not require task IDs.

Before broad edits or expensive validation, state:

- interpreted goal;
- selected task/epic or why no durable task is needed;
- first action;
- out of scope.

Ask one concise clarification only when multiple plausible scopes, project
direction changes, destructive task/status moves, or broad irreversible work are
in play.

## Task Lifecycle

Task statuses: `idea -> backlog -> todo -> doing -> review -> done`, plus
`dropped`.

Epic statuses: `idea -> active -> done`, plus `dropped`.

- `idea`: raw intake; do not implement from it.
- `backlog`: refined, checkable, ready to schedule.
- `todo`: selected next work.
- `doing`: active work.
- `review`: awaiting human or explicit cleanup review.
- `done`: acceptance criteria checked and evidence logged.
- `dropped`: intentionally closed; never delete task files to hide work.

Default `list --json` shows current work, including `review`. Use
`list --ideas --json` for raw intake and `list --archive --json` for history.

## Create Or Refine

Create/refine a task when work needs durable tracking beyond the current reply:

- distinct feature/fix/policy/validator/cleanup;
- deferred user idea that must not be lost;
- source-of-truth or reusable pipeline behavior changes;
- broad work that cannot finish immediately.

Do not create a task for a tiny direct edit, a validation command, or a duplicate
of existing work.

A backlog task must have:

- non-empty `## What`;
- checkable `## Done when`;
- clear scope boundaries or open questions.

## Done And Evidence

A task is done only when `## Done when` boxes are checked and `## Log` contains
the evidence that proves them.

Smallest reliable validation by change type lives in one place:
`docs/ai-pipeline/quality-validation.md`.

Repeated strict/product gate failures are validated by:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

If validation is too slow, unavailable, or fails for an unrelated environment
reason, record that explicitly. Do not silently mark the task done.

## Checkpoints

Leave the repo resumable without chat history.

Record a checkpoint in the task log when:

- task status changes;
- validation evidence changes;
- work pauses partially complete;
- another agent/session must continue.

Where information goes:

- task `## Log`: detailed evidence, command paths, decisions, unresolved issues;
- final response: concise human summary and validation.

## Format

```markdown
---
id: T0001
title: First playable action
status: backlog
epic: E001
priority: P2
tags: [state, core-loop]
created: 2026-06-11
updated: 2026-06-11
---

## What

## Done when

- [ ] checkable acceptance criterion

## Open questions

## Log
```

Prefer `node ai_studio/taskboard/cli.mjs new` over hand-created files so IDs do not
collide. Run `node ai_studio/taskboard/cli.mjs validate` after bulk edits.
