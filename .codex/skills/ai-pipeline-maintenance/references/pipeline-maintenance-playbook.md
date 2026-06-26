# Pipeline Maintenance Playbook

Load this when the work changes AI workflow docs, skills, validators, profiling,
taskboard behavior, product gates, or portable export.

## Audit Flow

1. Inspect current state, not memory: `git status --short --untracked-files=all`
   and `node ai_studio/taskboard/cli.mjs summary --json`.
2. Identify the repeated failure or duplication: oversized hot doc, duplicated
   rule, root/export drift, stale skill pointer, weak validation routing, or
   missing source-of-truth.
3. Make one coherent change and keep unrelated game/content work out of scope.

Profiling needs no setup: the hook records tool calls automatically; read a
session with `node tools/ai_profile/status.mjs` when reviewing pipeline friction.

## Split Decisions

- Keep `AGENTS.md`, `ai_studio/README.md`, and `ai_studio/taskboard/README.md` as hot maps.
- Move detailed procedure, examples, and historical lessons into
  `docs/ai-pipeline/`, skill `references/`, task guides, or tools.
- Create or update a skill when the procedure is repeatable agent behavior with
  clear triggers. Do not create a skill for one-off project facts.
- Prefer updating an existing skill when the trigger already exists; create a
  new skill only when the workflow is distinct.

## Mechanical Guard Pattern

For every new mandatory rule, prefer:

1. validator or test in the owning module;
2. narrow command in the relevant skill/doc;
3. direct cross-module route only when a real integration boundary exists.

Common guards:

- `node ai_studio/core_harness/validation/doc_reference_check.mjs`
- `node ai_studio/architecture_map/validate_map.mjs`
- `node tools/skills_eval.mjs`
- `node ai_studio/core_harness/agent_surfaces/sync.mjs --check`
- `node tools/product_gate/repeated_failure_guard.mjs`
- `node ai_studio/taskboard/cli.mjs validate`

## Validation Matrix

- Hot docs or route references: doc reference check,
  `node ai_studio/core_harness/validation/doc_reference_check.mjs`.
- Skill entrypoints/references: `node tools/skills_eval.mjs`,
  `node ai_studio/core_harness/agent_surfaces/sync.mjs --check`.
- Product gate/tooling: focused product-gate tests.
- Taskboard/status rules: taskboard tests and `node ai_studio/taskboard/cli.mjs validate`.
- Architecture map: `node ai_studio/architecture_map/validate_map.mjs`.
- Portable export or root/export drift: `node --test tools/bootstrap/export_base.test.mjs`.

## Report Shape

State the source-of-truth changed, the invariant now enforced, exact validation
commands, commit hash, and any remaining environment-only warnings. Do not claim
the full long-running pipeline goal is complete unless every objective is
audited against current evidence.
