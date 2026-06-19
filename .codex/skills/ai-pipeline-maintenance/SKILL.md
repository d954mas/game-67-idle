---
name: ai-pipeline-maintenance
description: "Use when maintaining or improving the reusable AI development pipeline in this repo: reducing hot Markdown/context, splitting agent docs or skills, removing duplicated rules, adding/updating validators, tuning context budgets, fixing skills sync/eval, taskboard/profiling/product-gate workflow, portable export, or implementing post-review pipeline improvements."
---

# AI Pipeline Maintenance

Use this skill for workflow/tooling changes to the AI-first game-development
base. It is not for ordinary game feature work; use `game-feature-iteration`
for playable changes and `chat-session-reflection` for pure retrospectives.

## Load Only What Applies

- `references/pipeline-maintenance-playbook.md`: audit flow, split decisions,
  source-of-truth placement, validator updates, validation matrix, and report
  shape.
- `AI_PIPELINE.md`: hot workflow map and high-level operating rules.
- `docs/ai-pipeline/agent-workflow.md`: context policy, Markdown shape, and
  multi-agent boundaries.
- `docs/ai-pipeline/quality-validation.md`: gate taxonomy, validation routing,
  and repeated strict/product failure policy.
- `docs/ai-pipeline/profiling-reuse.md`: passive profiling, prototype closeout,
  asset routing, and portable export.

## Default Workflow

1. Check current state: `git status --short --untracked-files=all`,
   `node tools/taskboard/cli.mjs summary`, and `node tools/context_budget.mjs`.
2. Pick one maintenance scope: hot docs, skill entrypoints, validators,
   profiling, taskboard, product gates, export, or post-review cleanup.
3. Move repeated procedure behind a reference, skill, task rule, or tool. Keep
   hot files as maps and decision rules.
4. When a rule becomes mandatory, encode it in a validator or test instead of
   adding more prose.
5. Run the narrow proof first, then `node tools/pipeline_validate.mjs` before
   committing pipeline changes.

## Placement Rules

- Project policy and boundaries: `AGENTS.md`.
- Portable workflow map: `AI_PIPELINE.md`.
- Detailed reusable procedure: `docs/ai-pipeline/` or skill `references/`.
- Repeatable task-specific agent behavior: `.codex/skills/<skill>/`.
- Work status and durable evidence: `tasks/STATUS.md` and task logs.
- Mechanical invariants: `tools/*` validators/tests.

## Stop Conditions

- Do not mark the broad pipeline goal done from one green check; prove each
  changed contract with current files and commands.
- Do not move project-specific facts into reusable skills or knowledge.
- Do not leave `.claude/skills` stale after changing `.codex/skills`; run
  `node tools/skills_sync.mjs` or `--check`.
