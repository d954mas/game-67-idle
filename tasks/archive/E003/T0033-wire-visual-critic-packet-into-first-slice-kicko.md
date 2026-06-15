---
id: T0033
title: Wire visual critic packet into first slice kickoff
status: done
epic: E003
priority: P1
tags: [visual, startup, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

`visual_critique_packet.mjs` exists, but `new_prototype.mjs` still creates a
first-slice visual gate that jumps straight to `--visual-strict`. Wire the
critic packet command into the generated first-slice artifact so independent
visual critique is visible at prototype kickoff.

## Done when

- [x] Generated `reviews/first_slice_visual_gate.md` includes a
      `visual_critique_packet.mjs` command skeleton.
- [x] The template names the expected critic packet Markdown/JSON paths.
- [x] Game-context tests assert the packet command and paths are generated.
- [x] Docs/status mention the first-slice critic packet as optional sidecar
      before strict product gate.
- [x] Game-context/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Started after T0032 added the critic packet generator but kickoff
  still did not reference it.
- 2026-06-15: Added the visual critic packet command and Markdown/JSON packet
  paths to the generated first-slice gate, plus game-context assertions and
  primary GDD skill guidance. Validation: `node --check
  tools/game_context/new_prototype.mjs`, `node --test tools/game_context/test.mjs`,
  `node tools/skills_eval.mjs`, profiled quick `node tools/pipeline_validate.mjs`,
  `git diff --check ...`, `node tools/taskboard/cli.mjs validate`, and
  `node tools/ai.mjs status --require-current-scope-usable` passed.
