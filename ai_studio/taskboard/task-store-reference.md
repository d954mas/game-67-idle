# Task Store Reference

Detailed task protocol for work that changes projects, epics, tasks, or
reporting shape.
Start from `ai_studio/taskboard/README.md`; load this file only when editing
or auditing the task store.

## Intent To Scope

Users describe work naturally; do not require task IDs.

Before broad edits or expensive validation, state:

- interpreted goal;
- selected project/epic/task or why no durable task is needed;
- first action;
- out of scope.

Ask one concise clarification only when multiple plausible scopes, project
direction changes, destructive task/status moves, or broad irreversible work are
in play.

## Task Lifecycle

Project statuses: `idea -> active -> done`.

Project kinds: `ai-studio`, `game`, `template`, `tooling`, `research`, `other`.

Projects are the top-level owner for work. Use one project for reusable AI
Studio work, one project per active game, one project per active template, and
lightweight tooling/research projects when they own durable cross-cutting work.

Task statuses: `idea -> backlog -> todo -> doing -> review -> done`.
Tasks cannot be created directly as `done`; create an active record, then close
it through the guarded update path so closure evidence is evaluated.

Epic statuses: `idea -> active -> done`.

- `idea`: raw intake; do not implement from it.
- `backlog`: refined, checkable, ready to schedule.
- `todo`: selected next work.
- `doing`: active work.
- `review`: awaiting human or explicit cleanup review.
- `done`: acceptance criteria checked and evidence logged.
- `done`: completed or intentionally closed; record the reason in `## Log`.

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

Epics should set `project: P###` once the owning project is known. Tasks should
set `project: P###` and `epic: E###` for scheduled work; task creation inherits
the project from the epic when possible. Validation rejects missing project
references and task project/epic mismatches.

## Done And Evidence

A transition of a task from any non-`done` status to `done` must satisfy both
presence checks below through the final markdown body passed to `updateDoc`:

1. `## Done when` contains at least one nonblank canonical `- [x] criterion`,
   every checkbox there is canonical and checked, or `## Log` contains:
   `Closure: waived; reason: <non-empty>; evidence: <non-empty>`.
2. `## Log` contains either
   `Quality: Q...=<outcome>[; Q...=<outcome>]; evidence: <non-empty>` or
   `Quality: not-applicable; reason: <non-empty>`.

Log decisions count only as canonical dated bullets such as
`- YYYY-MM-DD: Quality: ...` or `- YYYY-MM-DD: Closure: ...`, outside fenced
code blocks. Bare lines and documentation examples do not satisfy the gate.

Quality outcomes are `pass`, `block`, `review`, `skip`, or `unverified`. This
guard checks explicit shape only; it does not select rules, run checks, or infer
whether an outcome is sufficient for release. Projects, epics, non-`done`
transitions, and edits to already-`done` tasks are unaffected. Existing direct
markdown history is grandfathered and needs no migration.

The CLI can append canonical dated lines before calling the same `updateDoc`
path:

```powershell
node ai_studio/taskboard/cli.mjs set T0001 --status done --waiver-reason "superseded" --closure-evidence "E001 decision" --quality-not-applicable "planning-only" --json
node ai_studio/taskboard/cli.mjs set T0001 --status done --quality "QCLR_001=pass; QTECH_001=review" --quality-evidence "tests and review" --json
```

`--waiver-reason` pairs with `--closure-evidence`; `--quality` pairs with
`--quality-evidence`; `--quality-not-applicable` is mutually exclusive with
those quality options. Raw `--log` remains supported. Missing, malformed, or
conflicting input fails before write/archive with a concise message and a
machine-readable `problem` code/details in JSON CLI and HTTP API responses.

Smallest reliable validation by change type starts from the Quality rules:
`ai_studio/quality/README.md`.

Repeated quality failures should be visible in task logs and summarized with:

```powershell
node ai_studio/quality/profile.mjs
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
- delegated work is integrated or rejected.

Where information goes:

- task `## Log`: detailed evidence, command paths, decisions, unresolved issues;
- final response: concise human summary and validation.

## Format

Project:

```markdown
---
id: P001
title: AI Studio
status: active
kind: ai-studio
target: ai_studio
priority: P1
tags: [ai-studio]
created: 2026-07-01
updated: 2026-07-01
---

## Goal

## In scope

## Out of scope

## Log
```

Epic:

```markdown
---
id: E001
title: First playable vertical slice
status: active
project: P001
priority: P2
tags: [prototype]
created: 2026-07-01
updated: 2026-07-01
---

## Goal

## In scope

## Out of scope

## Log
```

Task:

```markdown
---
id: T0001
title: First playable action
status: backlog
project: P001
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

## Store Qualification

Each Taskboard store has its own local `P###`, `E###`, and `T####` counters.
The Studio store uses `storeId: studio`; game stores use `storeId: game:<id>`.
Rows exposed through CLI/API include `qualifiedId` such as
`game:fixture-game:T0001`.

Creation serializes only ID allocation and the initial markdown write through
`items/.allocation.lock`; reads and edits do not take this lock. The allocator
updates `.counters.json` by atomic replacement and creates the target markdown
with exclusive-create semantics. A failed create may consume an ID, but never
rewinds or corrupts the counter.

The allocator automatically reclaims a lock older than 30 seconds when its
recorded PID is no longer alive. If a create times out on a stale lock, inspect
`items/.allocation.lock/owner.json`; remove the lock directory manually only
after confirming that PID is gone and no Taskboard create command is running.
The next create scans existing documents before advancing the counter.

Use bare IDs only inside one selected store. When a reference intentionally
crosses stores, write it as a qualified ID so aggregate validation can resolve
the target unambiguously.
