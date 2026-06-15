---
id: T0034
title: Add ai facade shortcut for visual critic packets
status: done
epic: E003
priority: P1
tags: [visual, agent, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

`visual_critique_packet.mjs` is useful, but it sits outside the `tools/ai.mjs`
facade that agents already use for `gate`, `close-slice`, `run`, and
profiling. Add a short facade command so visual critique packets are easier to
discover and use during prototype work.

## Done when

- [x] `node tools/ai.mjs critic ...` forwards to
      `tools/product_gate/visual_critique_packet.mjs`.
- [x] `tools/ai.mjs` usage text documents the critic command.
- [x] Facade tests cover packet creation through `node tools/ai.mjs critic`.
- [x] Docs/skills prefer the facade shortcut where agents are already using
      `node tools/ai.mjs`.
- [x] AI facade/product-gate/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Started after T0032/T0033 made critic packets available but still
  required a deeper direct tool path.
- 2026-06-15: Added `node tools/ai.mjs critic` facade forwarding to
  `tools/product_gate/visual_critique_packet.mjs`, updated kickoff/docs/skills,
  and added facade coverage. Validation: `node --check tools/ai.mjs`,
  `node --test tools/ai.test.mjs`, `node --test tools/game_context/test.mjs`,
  `node tools/skills_eval.mjs`, profiled quick `node tools/pipeline_validate.mjs`,
  `git diff --check ...`, `node tools/taskboard/cli.mjs validate`, and
  `node tools/ai.mjs status --require-current-scope-usable` passed.
