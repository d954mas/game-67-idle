# Task Store

Single source of truth for project work items and live project status.

- Current task files live in `active/`.
- Historical `done`/`dropped` task files live in `archive/<epic-id>/` or
  `archive/unassigned/`.
- Epic files live in `epics/`.
- `STATUS.md` is the live project-status index.

The visual board and CLI read/write current tasks by default. Archived tasks
are history; include them only when investigating evidence or past decisions.

## Run the board

```powershell
node tools/taskboard/server.mjs
```

Open `http://127.0.0.1:8070/`.

## CLI

```powershell
node tools/taskboard/cli.mjs summary
node tools/taskboard/cli.mjs list
node tools/taskboard/cli.mjs context
node tools/taskboard/cli.mjs list --ideas
node tools/taskboard/cli.mjs list --review
node tools/taskboard/cli.mjs list --archive
node tools/taskboard/cli.mjs new task --title "..." --epic E001 --priority P1
node tools/taskboard/cli.mjs set T0001 --status doing
node tools/taskboard/cli.mjs show T0001
node tools/taskboard/cli.mjs validate
```

For a fresh game prototype, prefer the Stage 0 kickoff command instead of
hand-creating the first wiki/task/status skeleton:

```powershell
node tools/game_context/new_prototype.mjs --game-id bubble-bay --title "Bubble Bay" --brief "Bright casual bubble fishing prototype with simple upgrades."
```

## Live Status

`STATUS.md` is the short operational project-status index. It answers what the
current goal is, which playable path matters now, what gate is being targeted,
which validation commands prove it, what is blocking, what is non-blocking
debt, where the last good evidence lives, and what to do next.

Keep `STATUS.md` current, but not large. It is an index to the truth, not a
replacement for task logs, epics, design docs, runtime state schemas, or audit
reports. Every concrete claim should point to a task, epic, design doc,
command, or evidence path.

`STATUS.md` has a hard live-context budget of 6000 characters enforced by
`node tools/taskboard/cli.mjs validate`. When a prototype closes, replace
detailed status sections with durable pointers to `tasks/archive/` and
`gamedesign/projects/<game-id>/` instead of keeping old evidence inline.

Agents must read `STATUS.md` at the start of long-running project work and
update it after changes to:

- current goal or release gate
- active playable path
- blocker or non-blocking debt list
- required validation commands
- last known good evidence
- next priorities

## Minimal Current Context

For long-running project work, resumes, planning, multi-agent work, or code/doc
changes, agents first load only the current working context:

1. `AGENTS.md`
2. `node tools/taskboard/cli.mjs context`
3. `node tools/taskboard/cli.mjs list` only when the digest is not enough

Then read only task files directly relevant to the decision:

- P0/P1 active blockers
- tasks named by the user
- tasks named in `STATUS.md` as blockers, debt, evidence, or next priority
- active epic only when the work changes that epic's scope or gate

Do not load completed tasks, review queues, old logs, P3 ideas, unrelated
epics, or broad design docs by default. Follow those links only when reviewing
or closing old work, debugging a regression, checking evidence, or making a
concrete decision that needs them.

Read `tasks/STATUS.md` directly only when changing it, auditing a specific
status claim, or following a section/evidence path from the context digest.
When `cli.mjs context` reports `status_warning: large`, treat full-status reads
as high context cost and prefer showing a specific task or evidence file.

When asked "where are we" or "what next", start from `STATUS.md`, then summarize
only relevant active epic progress, `doing`/`review` items, top backlog by
priority, and unresolved ideas that need the user.

## Context Budget

Context windows overflow and compact when sessions over-load them. Keep the
standing footprint small so the agent compacts less:

- Default load is only: `AGENTS.md`, `node tools/taskboard/cli.mjs context`, the
  active task file, and the ONE skill that matches the work. Nothing else by
  default.
- Skills are intentionally lean. Heavy method detail lives on demand in each
  skill's `references/` and in `gamedesign/knowledge/` (e.g.
  `reference_deconstruction.md`); load it only when the task needs that depth,
  not every session.
- Prefer summaries over dumps. Use `Grep`/`Glob` and `Read` with an offset/limit
  to fetch the few relevant lines; do not paste whole files, `find`/`ls -R`, or
  full `cat` output into context. Tool output should be the conclusion, not the
  raw listing.
- Use `cli.mjs summary`/`context` (compact) instead of full `list`/`STATUS`
  reads; use `--verbose` profiler/validator output only when debugging those.
- Delegate broad multi-file investigation to a subagent and keep only its
  conclusion; do not read across many files in the main context.
- For long autonomous work, rely on durable state (task files, commits,
  `STATUS.md`) so a compaction can resume cleanly without re-reading history.
- At prototype close (or when `tmp/` grows large), clear disposable scratch with
  `node tools/tmp_sweep.mjs --list` then `--all-scratch` (keeps the newest
  pipeline-validate dirs; durable evidence already lives under
  `gamedesign/projects/<id>/`).

## Search Hygiene

Search current context before history.

For task/work planning, prefer:

- `tasks/STATUS.md`
- `node tools/taskboard/cli.mjs summary`
- `node tools/taskboard/cli.mjs list`
- relevant files in `tasks/active/`
- active epic files only when they affect the current scope

Do not search `tasks/archive/` by default. Search archived tasks only when a
current source links to them, when checking evidence, when debugging a
regression, or when recovering why a decision was made.

For code/design work, scope search to the relevant area first, such as `src/`,
`tools/`, `.codex/skills/`, or the active design concept. Use repo-wide search
only after a scoped search fails or when the target location is unknown.

Treat search results from `tasks/archive/`, old design handoffs, generated
files, and build outputs as historical or derived context until confirmed by
`STATUS.md`, active tasks, current source files, or validation evidence.

## Intent To Scope

Users usually describe work in natural language, not task IDs. The task store is
for agents; do not require the user to name task IDs.

Before editing files or running expensive validation, translate the request into
one explicit working scope and state:

- interpreted goal
- selected task/epic, or the task you will create/refine
- first action
- out of scope

Profiling is passive and optional. For long-running implementation, visual,
research, or tooling work you may start a profiler scope after selecting the
task, but it is never required to begin or finish work:

```powershell
node tools/ai.mjs start <task-id> <short-iteration-name>
```

`node tools/ai.mjs status` (and the `--require-current-scope-usable` guard) is
an optional health check; use it only when the task is explicitly about AI
workflow, profiler behavior, or a requested retrospective. If you do rely on
profiling evidence and coverage is low or broken, record the largest gaps and
do not make bottleneck or time-spend claims for unmeasured intervals.

If the request clearly maps to one current task or a small safe improvement,
proceed after that short confirmation update.

Ask one concise clarification question first when:

- multiple plausible scopes match
- the request could mean different subsystems
- the work would close, drop, archive, or rewrite tasks or docs
- the work changes project direction, release gate, or source-of-truth rules
- the likely implementation is broad or irreversible

Do not ask the user for task IDs. Ask about intent, priority, or scope. Do not
work across unrelated tasks just because the request is broad.

## Task Creation And Refinement

Do not create tasks for every tiny action. Create or refine a task when the work
needs durable tracking beyond the current message.

Use an existing task when:

- the request clearly matches its title, tags, `What`, or `Done when`
- the task is in `backlog`, `todo`, `doing`, or `review`
- the needed change fits that task's scope without changing its meaning

Create a new task when:

- the work will not be completed immediately
- the work is a distinct feature, fix, policy, validator, or cleanup
- the user states an idea that should not be lost
- the work changes source-of-truth rules or reusable pipeline behavior
- no existing active task matches after checking `STATUS.md` and the actionable
  list

Refine before implementation when:

- a task is still `idea`
- `Done when` is missing or not checkable
- scope, owner, target files, or validation are unclear
- multiple reasonable implementations have different tradeoffs

Do not create a new task when:

- the change is a small direct edit inside the current working scope
- the work is only a validation command or evidence update for the current task
- it duplicates an existing active task
- it belongs as a log entry in an existing task

When creating or refining a task, keep it small enough for one focused session
unless it is an epic.

## Done And Evidence Gates

A task is not done because files changed. It is done when its `Done when` boxes
are checked and its `## Log` contains evidence that proves those boxes.

Evidence should be the smallest reliable proof for the task:

- docs/process change: show the changed source-of-truth file and run
  `node tools/taskboard/cli.mjs validate`
- taskboard/tooling change: run `node --test tools/taskboard/test.mjs` and
  `node tools/taskboard/cli.mjs validate`
- product-gate/tooling change: run `node --test tools/product_gate/test.mjs`
  and `node tools/taskboard/cli.mjs validate`
- AI profile/profiling tooling change: run the narrow facade/profile tests
  that cover the changed behavior, plus `node tools/taskboard/cli.mjs validate`
- skill/process change: run `node tools/skills_eval.mjs` when changing reusable
  skill activation, required outputs, or portable workflow rules
- reusable pipeline-base change: run quick validation with
  `node tools/pipeline_validate.mjs`; use `node tools/pipeline_validate.mjs --full`
  only for final portable-base/export/runtime gates
- portable pipeline/export change: validate the current repo, export a fresh
  project with `tools/bootstrap/export_base.mjs`, then validate the exported
  task store from inside the exported project
- game/runtime change: run the narrow native/web scenario that proves the
  changed behavior, and capture screenshots when the change is visual/playable
- visual, FTUE, audience-test, or first-screen gameplay change: run
  `node tools/ai.mjs gate` on the screenshot and record the generated gate
  artifact before expanding content; use `node tools/ai.mjs close-slice` before
  handoff/review
- release/build change: run the relevant build command and record the artifact
  or report path
- prototype slice before commit/review: run
  `node tools/product_gate/slice_hygiene.mjs --strict` with build/probe
  evidence, product gate, screenshot evidence, and any known red gates. A
  profiler guard (`--profile-guard`) is optional/advisory and never blocks the
  slice. Normal slices over 30 changed files should be split unless the lead
  explicitly asked for an end-of-experiment snapshot. Check push/upstream state
  before promising push.

If a validation command is too slow, unavailable, or fails for an unrelated
environment reason, record that explicitly in the task log and final report.
Do not silently mark the task done.

Profiling evidence is advisory for normal game work. Do not make a task wait
on fresh profile bundles, reflection packets, drafts, reviews, follow-ups, or
baselines unless the task is explicitly about AI workflow, profiler behavior,
or a requested retrospective.

When work changes the current goal, release gate, validation command set,
blockers, last known good evidence, or next priorities, update `STATUS.md` in
the same session. Keep detailed history in task logs, not in `STATUS.md`.

## Checkpoints And Handoff

For substantial work, agents must leave the repository resumable without reading
the chat history.

Create a checkpoint when:

- a task moves to `doing`, `review`, `done`, or `dropped`
- validation evidence changes
- the current goal, gate, blocker list, or next priorities change
- work pauses after a partial implementation
- another agent or future session must continue the work

Checkpoint content belongs in different places:

- task `## Log`: detailed evidence, commands, report paths, decisions, and
  unresolved issues for that task
- `STATUS.md`: short current index only: goal, gate, blockers, validation,
  last known good evidence, and next priorities
- final response: concise human summary of what changed, what was verified, and
  what remains

Do not put long iteration history in `STATUS.md`. Do not rely on chat history as
the only record of a decision, blocker, or validation result.

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

- [ ] checkable acceptance criteria

## Open questions

## Log
```

## Rules

- Task statuses: `idea -> backlog -> todo -> doing -> review -> done`, plus
  `dropped`. Epic statuses: `idea -> active -> done`, plus `dropped`.
- Default `list` shows actionable current work only: `backlog`, `todo`,
  `doing`, and `review`. Raw `idea` tasks are hidden by default; use
  `list --ideas` to inspect them. Use `list --all` for every active status.
- New and active task files live in `active/`. When a task is set to `done` or
  `dropped`, tooling moves it to `archive/<epic-id>/` or `archive/unassigned/`.
- `node tools/taskboard/cli.mjs list` intentionally omits `idea`, `review`,
  `done`, `dropped`, and archived tasks. Use `list --review` for review
  cleanup, `list --ideas` for raw ideas, and `list --archive` only for
  history/evidence review.
- `idea` means raw and unexamined; do not implement from an `idea` task.
  Refine it first: ask questions, research, split, then move to `backlog`.
- Never delete a task file to "remove" work; set `status: dropped` and note
  why in `## Log`. History is part of the value.
- Prefer `cli.mjs new` over hand-creating files so IDs never collide.
- Keep `## Done when` checkable; when finishing a task, tick the boxes and add
  a one-line evidence note (command, screenshot path, scenario name) in `## Log`.
- Priorities: `P0` blocking now, `P1` this iteration, `P2` normal, `P3` someday.
- Run `validate` after bulk edits.
