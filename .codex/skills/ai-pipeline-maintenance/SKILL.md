---
name: ai-pipeline-maintenance
description: "Use when maintaining or improving the reusable AI development pipeline in this repo: reducing hot Markdown/context, splitting agent docs or skills, removing duplicated rules, adding/updating validators, fixing skills sync/eval, taskboard/profiling/product-gate workflow, portable export, or implementing post-review pipeline improvements."
---

# AI Pipeline Maintenance

Use for AI workflow/tooling changes, not ordinary playable game work.

## Load Only What Applies

- `references/pipeline-maintenance-playbook.md`: audit flow, source-of-truth,
  Mechanical Guard Pattern, Validation Matrix, Report Shape.
- `references/skill-placement.md`: Update Existing Skill, Create New Skill, and
  Keep Hot Docs Thin decisions.
- `ai_studio/README.md`: portable AI Studio workflow and routing map.
- `ai_studio/core_harness/workflow/README.md`: context and work-loop routing.
- `ai_studio/core_harness/workflow/orchestration/README.md`: early split rule
  for broad read-heavy work.
- `docs/ai-pipeline/quality-validation.md`: gates and repeated strict/product
  failure policy.
- `docs/ai-pipeline/profiling-reuse.md`: profiling, closeout, assets, export.

## Default Workflow

1. Inspect with `git status --short --untracked-files=all` and
   `node ai_studio/taskboard/cli.mjs summary --json`.
2. Pick one scope: hot docs, skill entrypoints, validators, profiling,
   taskboard, product gates, export, or post-review cleanup.
3. Move repeated procedure behind a reference, skill, task rule, or tool.
4. Put mandatory rules in validators/tests where practical, then run the
   narrow proof and `node ai_studio/core_harness/validation/pipeline_validate.mjs`.

## Placement Rules

- Policy/boundaries: `AGENTS.md`; portable map: `ai_studio/README.md`.
- Reusable method: `docs/ai-pipeline/` or skill `references/`.
- Repeatable agent behavior: `.codex/skills/<skill>/`.
- Current game routing: `GAME_PROJECT.md`; work evidence: task logs and evidence files.
- Mechanical invariants: `tools/*` validators/tests.

## Stop Conditions

- Do not mark broad pipeline work done from one green check.
- Do not move project facts into reusable skills/knowledge.
- After changing `.codex/skills`, run `node tools/skills_sync.mjs` or `--check`.
