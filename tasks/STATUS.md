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
Skill/process regression checks now live in `tools/skills_eval.mjs` and are
included in portable-base export. The eval now covers `task-manager`,
`game-runtime-automation`, and `primary-gdd-pipeline`.
Taskboard validation now rejects empty active actionable tasks and empty active
epics, while allowing raw `idea` items to stay lightweight until refined.
`taskboard validate` now prints remediation hints for common problems so agents
can recover faster when validation fails.

Sources: `tasks/archive/E003/T0038-add-active-archive-task-store-structure.md`,
`tasks/archive/unassigned/T0010-retire-implementation-tasks-json-in-favor-of-tas.md`,
`tasks/archive/E003/T0018-add-activation-output-evals-for-key-skills-task-.md`,
`tasks/archive/E003/T0016-eval-primary-gdd-pipeline-behavior-after-trim-sk.md`,
`tasks/archive/E003/T0019-recurring-entropy-cleanup-stale-docs-unused-skil.md`,
`tasks/archive/E003/T0020-cli-mjs-validate-add-remediation-hints-to-proble.md`,
`tasks/README.md`, `AI_PIPELINE.md`.

## Current Gate

Current gate: no actionable pipeline task is currently ready. If pipeline work
continues, refine one raw idea from `node tools/taskboard/cli.mjs list --ideas`
or create a new scoped task from the user's next direction.

Source: `tasks/README.md`.

## Required Validation

```powershell
node tools/taskboard/cli.mjs list
node tools/skills_eval.mjs
node tools/taskboard/cli.mjs validate
node --test tools/taskboard/test.mjs
node tools/bootstrap/export_base.mjs --target tmp/export-...
cd tmp/export-...
node tools/skills_eval.mjs
node tools/taskboard/cli.mjs validate
node --test tools/taskboard/test.mjs
```

Sources: `tasks/archive/E003/T0038-add-active-archive-task-store-structure.md`,
`tasks/archive/E003/T0018-add-activation-output-evals-for-key-skills-task-.md`,
`tasks/archive/E003/T0016-eval-primary-gdd-pipeline-behavior-after-trim-sk.md`,
`tasks/archive/E003/T0019-recurring-entropy-cleanup-stale-docs-unused-skil.md`,
`tasks/archive/E003/T0020-cli-mjs-validate-add-remediation-hints-to-proble.md`.

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

Latest skill/process evidence: T0018 added `tools/skills_eval.mjs`; current
repo and fresh export passed skill eval and task validation.
T0016 extended that eval to `primary-gdd-pipeline`; current repo and fresh
export passed skill eval and task validation.
T0019 added anti-entropy validation for actionable task bodies and active epic
scope bodies; current repo and fresh export passed taskboard tests, taskboard
validation, and skill eval.
T0020 added CLI remediation hints for common taskboard validation failures;
current repo and fresh export passed taskboard tests, taskboard validation, and
skill eval.

Sources: `tasks/archive/E003/T0018-add-activation-output-evals-for-key-skills-task-.md`,
`tasks/archive/E003/T0016-eval-primary-gdd-pipeline-behavior-after-trim-sk.md`,
`tasks/archive/E003/T0019-recurring-entropy-cleanup-stale-docs-unused-skil.md`,
`tasks/archive/E003/T0020-cli-mjs-validate-add-remediation-hints-to-proble.md`.

## Blocking Work

None.

## Non-blocking Debt

No actionable backlog remains for the current pipeline gate. Raw ideas are
available via `node tools/taskboard/cli.mjs list --ideas`: T0012 board UX and
T0014 `gamedesing` typo decision. They must be refined before implementation.

Historical game/testbed debt is archived with E001, including T0025.

Sources: `tasks/archive/E001/T0025-replace-temporary-wasm-release-workaround-with-o.md`.

## Next Priorities

1. Choose the next user-directed pipeline improvement and create/refine one
   scoped task.
2. If continuing from raw ideas, inspect `list --ideas` and refine exactly one
   item before implementation.
3. Keep default task context focused on E003 until a new active game or
   pipeline epic is selected.
