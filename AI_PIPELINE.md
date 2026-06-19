# AI Pipeline

Reusable process for human-led, AI-assisted game development. `AGENTS.md` holds
repo-specific rules; this file is the hot-context map for portable workflow.

Keep this file short. Load deeper files only when the current task needs them:

- `docs/ai-pipeline/agent-workflow.md`: roles, context policy, work loop,
  multi-agent boundaries, and Markdown-writing rules.
- `docs/ai-pipeline/quality-validation.md`: separate quality gates, validation
  defaults, and the repeated strict/product failure stop.
- `docs/ai-pipeline/profiling-reuse.md`: passive profiling, prototype closeout,
  visual/asset routing, and portable export.

## Default Context

For substantial work:

1. `AGENTS.md`
2. `node tools/taskboard/cli.mjs context`
3. one relevant task or evidence file
4. one matching skill
5. one deep pipeline doc from `docs/ai-pipeline/` only when needed

Prefer scoped search and compact command output over whole-file dumps. Keep
volatile session facts in tasks, status, evidence files, or final reports rather
than rewriting hot instruction files repeatedly.

## Default Loop

1. Interpret the user request into one working scope.
2. Select or create durable tracking only when useful.
3. For non-trivial work, set passive profiling scope or state why unavailable.
4. Read only files needed for the scope.
5. Make the smallest coherent change.
6. Run the narrowest validation that proves the change.
7. Record evidence in the task/status/final response when project state changes.

Use multiple agents only for independent side work with clear ownership,
expected artifact, evidence, and out-of-scope boundaries. The main integrator
keeps final responsibility for merge, validation, and status.

## Hard Stops

- Do not call a playable/visual slice done from one green gate. Product,
  game-loop, art-source, and technical gates are separate verdicts.
- When a strict/product gate fails twice for the same major reason, stop the
  local polish loop and change path: architecture, tooling, source asset,
  reference, or explicit lead acceptance.
- When the lead says a prototype/game is done, stopped, or only a test, stop
  game implementation and follow the latest task/status instruction.
- Repeated failures should become tools, validators, skills, or compact rules,
  not more duplicated prose.

## Core Commands

```powershell
node tools/taskboard/cli.mjs context
node tools/pipeline_validate.mjs
node tools/pipeline_validate.mjs --full
node tools/product_gate/repeated_failure_guard.mjs
```

Use `node tools/pipeline_validate.mjs` after normal pipeline/tooling edits.
Use `--full` when export/runtime/deep asset coverage is relevant.
