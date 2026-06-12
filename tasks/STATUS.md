# Project Status

Operational project-status index. Rules for this file live in
`tasks/README.md`.

## Current Goal

No active scoped goal is selected. The reusable AI pipeline hardening pass is
complete, and the fantasy RPG slice is historical testbed/evidence, not active
product work.

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
Current testbed keeps legacy `gamedesing/`; portable export maps reusable design
knowledge to corrected `gamedesign/knowledge/` for new projects.
Taskboard editor now has a side-by-side Markdown preview for task and epic
bodies, with a tested safe renderer for common task syntax.
Reusable pipeline validation now has a single command:
`node tools/pipeline_validate.mjs`.

Sources: `tasks/archive/E003/T0038-add-active-archive-task-store-structure.md`,
`tasks/archive/unassigned/T0010-retire-implementation-tasks-json-in-favor-of-tas.md`,
`tasks/archive/E003/T0018-add-activation-output-evals-for-key-skills-task-.md`,
`tasks/archive/E003/T0016-eval-primary-gdd-pipeline-behavior-after-trim-sk.md`,
`tasks/archive/E003/T0019-recurring-entropy-cleanup-stale-docs-unused-skil.md`,
`tasks/archive/E003/T0020-cli-mjs-validate-add-remediation-hints-to-proble.md`,
`tasks/archive/E003/T0014-decide-fix-gamedesing-typo-or-freeze-as-conventi.md`,
`tasks/archive/E003/T0012-board-ux-markdown-preview-manual-ordering-done-c.md`,
`tasks/archive/E003/T0042-single-command-portable-pipeline-validation.md`,
`tasks/README.md`, `AI_PIPELINE.md`.

## Current Gate

Current gate: wait for a new user-directed scope. If work continues, create or
refine exactly one task/epic before implementation.

Source: `tasks/README.md`.

## Required Validation

```powershell
node tools/taskboard/cli.mjs list
node tools/pipeline_validate.mjs
```

Sources: `tasks/archive/E003/T0038-add-active-archive-task-store-structure.md`,
`tasks/archive/E003/T0018-add-activation-output-evals-for-key-skills-task-.md`,
`tasks/archive/E003/T0016-eval-primary-gdd-pipeline-behavior-after-trim-sk.md`,
`tasks/archive/E003/T0019-recurring-entropy-cleanup-stale-docs-unused-skil.md`,
`tasks/archive/E003/T0020-cli-mjs-validate-add-remediation-hints-to-proble.md`,
`tasks/archive/E003/T0014-decide-fix-gamedesing-typo-or-freeze-as-conventi.md`,
`tasks/archive/E003/T0012-board-ux-markdown-preview-manual-ordering-done-c.md`,
`tasks/archive/E003/T0042-single-command-portable-pipeline-validation.md`.

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
T0014 froze `gamedesing/` as this testbed's legacy path and updated portable
export so new projects use `gamedesign/knowledge/`.
T0012 added taskboard Markdown preview; current repo and fresh export passed
taskboard tests, taskboard validation, skill eval, and HTTP static smoke.
T0042 added `node tools/pipeline_validate.mjs`; the command passed and validated
both this repo and a fresh export.
E003 is now done; no actionable backlog or raw ideas remain.

Sources: `tasks/archive/E003/T0018-add-activation-output-evals-for-key-skills-task-.md`,
`tasks/archive/E003/T0016-eval-primary-gdd-pipeline-behavior-after-trim-sk.md`,
`tasks/archive/E003/T0019-recurring-entropy-cleanup-stale-docs-unused-skil.md`,
`tasks/archive/E003/T0020-cli-mjs-validate-add-remediation-hints-to-proble.md`,
`tasks/archive/E003/T0014-decide-fix-gamedesing-typo-or-freeze-as-conventi.md`,
`tasks/archive/E003/T0012-board-ux-markdown-preview-manual-ordering-done-c.md`,
`tasks/archive/E003/T0042-single-command-portable-pipeline-validation.md`.

## Blocking Work

None.

## Non-blocking Debt

No actionable backlog, raw ideas, or active epic remain for the current pipeline
gate.

Historical game/testbed debt is archived with E001, including T0025.

Sources: `tasks/archive/E001/T0025-replace-temporary-wasm-release-workaround-with-o.md`.

## Next Priorities

1. Ask the user for the next concrete goal.
2. For any new goal, create/refine one scoped task or epic before implementation
   if it needs durable tracking.
