---
id: T0028
title: Enforce orchestration evidence for substantial pipeline work
status: done
epic: E001
priority: P0
tags: [pipeline, orchestration, guard]
created: 2026-06-21
updated: 2026-06-21
---

## What

Add a mechanical guard so substantial pipeline/orchestration work cannot be
closed as if the main agent worked alone. The guard should require task-log
evidence of bounded subagent packets plus independent review/verification, or a
clear small-scope exception.

### Scope

- Add a focused CLI guard with fixture tests.
- Run it in reusable pipeline validation.
- Keep routine tiny tasks possible through explicit `orchestration: not needed`
  evidence with a reason.
- Do not require subagents for clean seed status, normal taskboard hygiene, or
  small one-file edits.

### Out Of Scope

- No game implementation.
- No broad workflow engine.
- No automatic detection of actual subagent tool calls from transcripts yet.

## Done when

- [x] Guard fails done/review substantial pipeline tasks without orchestration
      evidence.
- [x] Guard accepts substantial tasks with subagent packet and independent
      reviewer/verifier evidence.
- [x] Guard accepts small tasks with an explicit scoped exception reason.
- [x] Guard is included in quick reusable pipeline validation.
- [x] Task evidence records the subagents used for this implementation.

## Open questions

- None for this guard increment.

## Log

- 2026-06-21: Started after lead goal to make orchestration reliable so the
  main agent cannot drift back into solo-worker mode.
- 2026-06-21: orchestration: used
  objective: enforce taskboard-level orchestration evidence for substantial pipeline work
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs, tools/taskboard/cli.mjs, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0028-enforce-orchestration-evidence-for-substantial-p.md
  expected output: transition guard, validation guard, regression tests, CLI hint, protocol docs, task evidence
  evidence command: node --test tools/taskboard/test.mjs; node tools/ai.mjs validate --review; AI_PIPELINE_PYTHON=<bundled-python> node tools/ai.mjs validate --full
  stop condition: substantial pipeline tasks cannot move to review/done without a complete orchestration packet or narrow small-scope exception
  independent reviewer: Plato, Darwin, and Linnaeus reviewed guard behavior; P1/P2/high findings were fixed and revalidated
- 2026-06-21: Evidence passed: `node --test tools/taskboard/test.mjs`
  (31/31), `node tools/taskboard/cli.mjs validate`,
  `node tools/context_budget.mjs --review`,
  `node tools/doc_reference_check.mjs`,
  `node --test tools/pipeline_validate.test.mjs`,
  `node tools/ai.mjs validate --review`, and
  `AI_PIPELINE_PYTHON=<bundled-python> node tools/ai.mjs validate --full`.
