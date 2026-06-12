# Project Status

Operational project-status index. Rules for this file live in
`tasks/README.md`.

## Current Goal

Harden the reusable AI game-development pipeline so future agents start fast,
keep context small, and do not confuse current work with historical task logs.
The fantasy RPG slice is historical testbed/evidence, not active product work.

Sources: `tasks/epics/E003-ai-pipeline-hardening.md`,
`tasks/archive/E003/T0038-add-active-archive-task-store-structure.md`,
`AI_PIPELINE.md`.

## Active Work

Task store restructuring is complete. Current task context is now
`tasks/active/`; historical done/dropped evidence is in `tasks/archive/`; and
`tasks/README.md` is the single source of workflow rules. Default
`taskboard list` shows actionable work only; raw ideas require `list --ideas`.
`tasks/README.md` also defines Intent To Scope: agents translate natural
language into one explicit scope, state it before acting, and ask when ambiguous.
Task creation/refinement thresholds are also canonical there, so agents do not
create duplicate tasks or implement unrefined ideas.
Done/evidence gates are canonical there too; portable pipeline changes must be
validated in both the current repo and a freshly exported project.
Checkpoint/handoff discipline is canonical there as well: substantial work must
leave task logs and `STATUS.md` sufficient for resume without chat history.
`AI_PIPELINE.md` defines multi-agent work-packet discipline for delegated or
parallel work, plus scoped tool/search/validation discipline.
The old visual-GDD `data/implementation_tasks.json` duplicate task source is
retired; work tracking is only in `tasks/`.

Sources: `tasks/archive/E003/T0038-add-active-archive-task-store-structure.md`,
`tasks/archive/unassigned/T0010-retire-implementation-tasks-json-in-favor-of-tas.md`,
`tasks/README.md`, `AI_PIPELINE.md`.

## Current Gate

Current gate: choose and implement the next pipeline hardening task from the
short active task list. Default task listing must stay short and exclude
archived history.

Source: `tasks/README.md`.

## Required Validation

```powershell
node tools/taskboard/cli.mjs list
node tools/taskboard/cli.mjs list --ideas
node tools/taskboard/cli.mjs list --archive --all
node tools/taskboard/cli.mjs show T0037
node tools/taskboard/cli.mjs validate
node --test tools/taskboard/test.mjs
node tools/bootstrap/export_base.mjs --target tmp/export-active-archive-test-...
node tmp/export-active-archive-test-.../tools/taskboard/cli.mjs validate
```

Source: `tasks/archive/E003/T0038-add-active-archive-task-store-structure.md`.

## Last Known Good Evidence

Fantasy RPG web RC audit passed:
`build/captures/web_visual_qa_audit/2026-06-12T07-30-22-344Z/report.json`.

Summary: desktop and mobile portrait each captured 23 screenshots, drove 22
interactions through `23_shrine_attunement`, and reported zero console warnings
and zero page errors.

Source:
`tasks/archive/E001/T0037-poki-web-rc-full-path-browser-playtest-audit.md`.

Latest pipeline cleanup evidence: T0010 retired `implementation_tasks.json`;
visual GDD site validation, package validation, and taskboard validation passed.

Source:
`tasks/archive/unassigned/T0010-retire-implementation-tasks-json-in-favor-of-tas.md`.

## Blocking Work

None.

## Non-blocking Debt

No active non-blocking debt for the current pipeline gate.

Historical game/testbed debt is archived with E001, including T0025.

Sources: `tasks/archive/E001/T0025-replace-temporary-wasm-release-workaround-with-o.md`.

## Next Priorities

1. Add an explicit archive command only if automatic done/dropped movement is
   not enough.
2. Continue pipeline hardening from the short active task list.
3. Keep default task context focused on E003 until a new active game or
   pipeline epic is selected.
