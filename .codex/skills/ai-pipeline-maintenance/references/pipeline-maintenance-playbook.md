# Pipeline Maintenance Playbook

Load this when the work changes AI workflow docs, skills, validators, profiling,
taskboard behavior, product gates, or portable export.

## Audit Flow

1. Inspect current state, not memory: `git status --short --untracked-files=all`,
   `node tools/taskboard/cli.mjs summary`, and `node tools/context_budget.mjs`.
2. Confirm profiling scope for non-trivial pipeline/tooling sessions with
   `node tools/ai.mjs status --require-current-scope-usable`; start or state
   unavailable if needed.
3. Identify the repeated failure or duplication: oversized hot doc, duplicated
   rule, root/export drift, stale skill pointer, weak validation routing, or
   missing source-of-truth.
4. Make one coherent change and keep unrelated game/content work out of scope.

## Split Decisions

- Keep `AGENTS.md`, `AI_PIPELINE.md`, and `tasks/README.md` as hot maps.
- Move detailed procedure, examples, and historical lessons into
  `docs/ai-pipeline/`, skill `references/`, task guides, or tools.
- Create or update a skill when the procedure is repeatable agent behavior with
  clear triggers. Do not create a skill for one-off project facts.
- Prefer updating an existing skill when the trigger already exists; create a
  new skill only when the workflow is distinct.

## Mechanical Guard Pattern

For every new mandatory rule, prefer:

1. validator or test in `tools/`;
2. narrow command in the relevant skill/doc;
3. quick pipeline inclusion when the check is cheap and general;
4. full pipeline inclusion only for export/runtime/deep asset coverage.

Common guards:

- `node tools/context_budget.mjs`
- `node tools/context_budget.mjs --review` for strict context-budget review
- `node tools/doc_reference_check.mjs`
- `node tools/skills_eval.mjs`
- `node tools/skills_sync.mjs --check`
- `node tools/product_gate/repeated_failure_guard.mjs`
- `node tools/taskboard/cli.mjs validate`
- `node tools/ai.mjs validate`
- `node tools/ai.mjs validate --review` for review-stage context pressure
- underlying implementation: `tools/pipeline_validate.mjs`

## Validation Matrix

- Hot docs or context budgets: context budget tests, doc reference check,
  `node tools/ai.mjs validate --review`.
- Skill entrypoints/references: `node tools/skills_eval.mjs`,
  `node tools/skills_sync.mjs --check`, context budget, quick pipeline.
- Product gate/tooling: focused product-gate tests plus quick pipeline.
- Taskboard/status rules: taskboard tests/validate plus quick pipeline.
- Portable export or root/export drift: `node tools/ai.mjs validate --full`
  or full dry-run when dependencies are unavailable.

## Report Shape

State the source-of-truth changed, the invariant now enforced, exact validation
commands, commit hash, and any remaining environment-only warnings. Do not claim
the full long-running pipeline goal is complete unless every objective is
audited against current evidence.
