---
id: T0014
title: Add new prototype startup gate
status: done
epic: E003
priority: P0
tags: [pipeline, skills, product-gate, profiling]
created: 2026-06-15
updated: 2026-06-15
---

## What

Add a reusable startup gate for new prototype iterations so agents do not jump
from a loose idea directly into broad runtime implementation. The gate should
surface missing concept/task/wiki/runtime/proof state before coding and record
the rule in the reusable pipeline and GDD skill.

## Done when

- [x] `tools/game_context/iteration_context.mjs` emits a machine-readable
      `prototype_startup_gate` with pass/fail requirements.
- [x] The rendered context pack shows startup gate status and missing items.
- [x] `AI_PIPELINE.md` defines startup gate as stage 0 before implementation.
- [x] `primary-gdd-pipeline` tells agents to run the startup gate before
      implementation handoff/runtime coding.
- [x] Tests and taskboard validation pass.

## Open questions

- none

## Log

- 2026-06-15: Created after fishing review showed the agent implemented broad
  runtime/gameplay before a strong art/product proof was accepted.
- 2026-06-15: Added `prototype_startup_gate` to
  `tools/game_context/iteration_context.mjs`. The gate checks for active
  concept, actionable task, project wiki source, runtime harness, and
  visual/product proof plan.
- 2026-06-15: Updated `AI_PIPELINE.md` and `primary-gdd-pipeline` so startup
  gate runs before implementation handoff/runtime coding.
- 2026-06-15: Verified with
  `node tools/game_context/iteration_context.mjs --json-output tmp/prototype_startup_gate_context.json`,
  `node --test tools/game_context/test.mjs`,
  `node tools/taskboard/cli.mjs validate`,
  `node tools/skills_eval.mjs`, and `node tools/pipeline_validate.mjs`.
- 2026-06-15: Done: prototype_startup_gate added to game_context, documented in AI_PIPELINE and primary-gdd-pipeline, and validated with context tests, taskboard, skills_eval, and pipeline_validate.
