---
id: T0026
title: Keep no-concept iteration context free of closed project sources
status: done
epic: E003
priority: P1
tags: [context, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

When no active game concept is selected, `node
tools/game_context/iteration_context.mjs` should orient agents to the clean
template, not closed Splash Rods/Rune Marches project files. The current context
pack still lists archived GDD/balance files and closed runtime source, which
creates avoidable context bloat and raises the risk of continuing old games.

## Done when

- [x] With no active concept, the context pack source-file list excludes
      `gamedesign/projects/roblox-fishing/`, `gamedesign/projects/rune-marches/`,
      and closed runtime `src/main.c`.
- [x] The same context pack still lists clean-template sources needed for the
      next iteration, especially `src/clean_seed_main.c`,
      `state/game_state.schema.json`, `CMakePresets.json`, and relevant
      reusable design knowledge.
- [x] A regression test covers the no-active-concept source list.
- [x] Game-context/taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Created after `iteration_context` showed closed project GDDs and
  `src/main.c` under `Source Files To Inspect` while the repository is in
  clean-template/no-active-concept mode.
- 2026-06-15: Done. `tools/game_context/iteration_context.mjs` now only
  includes reusable design knowledge and clean runtime harness sources when no
  active concept is selected. Closed project GDDs and closed `src/main.c` stay
  out of the default context pack.
- 2026-06-15: Evidence: `node --test tools/game_context/test.mjs`, real
  `node tools/game_context/iteration_context.mjs --json-output
  tmp/t0026-context.json`, `node tools/taskboard/cli.mjs validate`, `node
  tools/skills_eval.mjs`, `node tools/ai.mjs status
  --require-current-scope-usable`, and profiled `node tools/pipeline_validate.mjs`.
