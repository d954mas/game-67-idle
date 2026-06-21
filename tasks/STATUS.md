# Project Status

## Current Goal

Maintain the reusable AI pipeline/orchestration workflow. Current work is
pipeline/taskboard/profiling, not gameplay.

Dragon Grove remains review-only runtime context. Load its tasks/evidence only
when explicitly reviewing the prototype or a validator requires it.

## Current Runtime Surface

Native `game_seed` still contains the Dragon Grove debug slice under review:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c`.

`external/neotolis-engine` is read-only from this repo.

## Current Gate

Pipeline/orchestration changes need a taskboard item, scoped evidence, machine
validation where applicable, and review validation. Use bounded subagents when
parallel context helps.

T0029 remains in review for Dragon Grove. Broad game feature/content expansion
is out of scope unless the lead reopens game implementation.

## Blocking Work

- No blocker for pipeline/orchestration cleanup.
- Game expansion is blocked by current lead direction.

## Required Validation

```powershell
node tools/taskboard/cli.mjs validate
node tools/context_budget.mjs --review
node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok ... # use current parent/session values in task evidence
node tools/ai.mjs validate --review
```

## Last Known Good Evidence

- T0055 moved to review: taskboard summary/context suppress live game sections
  while current actionable work is pipeline/tooling-scoped.
- `node tools/ai.mjs validate --review` PASS after T0055.
- Dragon Grove runtime evidence remains in T0029.

## Next Priorities

1. Improve orchestration from profiler/taskboard evidence.
2. Keep subagent packets bounded, tool-safe, and machine-backed.
3. Review/close old pipeline tasks only when doing review cleanup.
