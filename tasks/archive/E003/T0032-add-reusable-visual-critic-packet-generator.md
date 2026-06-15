---
id: T0032
title: Add reusable visual critic packet generator
status: done
epic: E003
priority: P1
tags: [visual, agent, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

We now have `--visual-strict`, but no lightweight reusable packet for a
separate visual/UI critic pass before the final product gate. Add a local
packet generator that turns screenshot/target/task context into a structured
critic prompt plus the matching `node tools/ai.mjs gate --visual-strict`
command skeleton.

## Done when

- [x] A tool generates Markdown and JSON visual critic packets without
      requiring a browser or web prototype.
- [x] The packet covers the six strict visual axes, blocker/major/minor issue
      severities, screenshot path, target path, and next gate command.
- [x] Tests cover required inputs and generated packet content.
- [x] Docs/skills mention the packet as the optional sidecar for independent
      design critique before product gate closeout.
- [x] Product-gate/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Started after confirming subagent tools exist but cannot be
  spawned unless explicitly requested; a reusable packet still improves future
  agent/critic handoff without requiring delegation.
- 2026-06-15: Added `tools/product_gate/visual_critique_packet.mjs` with
  Markdown/JSON packet output, six-axis rubric, issue severity guidance, and
  strict gate command skeleton. Validation: `node --check
  tools/product_gate/visual_critique_packet.mjs`, `node --test
  tools/product_gate/test.mjs`, `node tools/skills_eval.mjs`, profiled quick
  `node tools/pipeline_validate.mjs`, `git diff --check ...`,
  `node tools/taskboard/cli.mjs validate`, and
  `node tools/ai.mjs status --require-current-scope-usable` passed.
