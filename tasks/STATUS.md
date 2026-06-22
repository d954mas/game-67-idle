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

Pipeline/orchestration changes need a taskboard item, pass validation
(`node tools/ai.mjs validate`), and lead review. Acceptance gates the work
product, not the delegation: there is no machine proof that a subagent ran. Use
bounded subagents when parallel context helps; the lead is the backstop.

T0029 remains in review for Dragon Grove. Broad game feature/content expansion
is out of scope unless the lead reopens game implementation.

## Blocking Work

- No blocker for pipeline/orchestration cleanup.
- Game expansion is blocked by current lead direction.

## Required Validation

```powershell
node tools/taskboard/cli.mjs validate
node tools/context_budget.mjs --review
node tools/ai.mjs validate --review
```

## Last Known Good Evidence

- T0055 moved to review: taskboard summary/context suppress live game sections
  while current actionable work is pipeline/tooling-scoped.
- `node tools/ai.mjs validate --review` PASS after T0055.
- Dragon Grove runtime evidence remains in T0029.

## Next Priorities

1. Keep the subagent protocol lean and packets bounded; the lead is the backstop.
2. Archive the closed orchestration meta-tasks (T0030-T0101) during review cleanup.
3. Mine the passive profiler for real friction; fix with tools/validators, not gates.
