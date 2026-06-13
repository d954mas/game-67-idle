---
id: T0110
title: Add game iteration context pack for prototype work
status: review
epic: ""
priority: P1
tags: [pipeline, context, gameplay, prototype, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a compact pre-code context pack for playable game iterations so future
agents keep the active concept, native runtime gate, reference-study gate,
visual/art gate, current project gate, and validation requirements in view
before implementing a prototype or game change.

Keep the pipeline fast: analytics should be available through a short facade,
and project-specific game tools should be physically separated from reusable
workflow/runtime tools.

## Done when

- [x] `tools/game_context/iteration_context.mjs` prints markdown and optional
      JSON context for playable game work.
- [x] The context pack preserves wrapped hard-gate bullets from `AGENTS.md`
      instead of clipping critical rules.
- [x] The context pack is included in portable AI base export and pipeline
      validation.
- [x] `game-feature-iteration` requires the context pack before playable
      implementation when the tool is available.
- [x] Profiling docs explain how to capture the context pack through
      `node tools/ai.mjs context`.
- [x] `tools/ai.mjs` provides the fast path for start, context, profiled run,
      status, and reflection closeout.
- [x] Current 67 World-specific scripts are grouped under
      `tools/project_67_world/` instead of being mixed into generic tool
      folders.
- [x] Regression tests cover native/web hard gates and source discovery.
- [x] Full validation is complete and the work is ready to commit.

## Open questions

None.

## Log

- 2026-06-13: Started `T0110` scope with `tools/ai_profile/start.mjs`.
- 2026-06-13: Added `tools/game_context/iteration_context.mjs` and tests for
  wrapped native/web hard gates, source discovery, and JSON output.
- 2026-06-13: Added `tools/ai.mjs` as the short AI workflow facade and moved
  67 World-specific asset, balance, release, and DevAPI scenario scripts under
  `tools/project_67_world/`.
- 2026-06-13: Validation passed: `node --check tools/ai.mjs`;
  `node --check tools/project_67_world/package_native_release.mjs`;
  `py -3.12 -m py_compile` for moved release/art/scenario scripts;
  `node tools/ai.mjs context`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`; `node tools/ai.mjs run -- node
  tools/pipeline_validate.mjs`; `git diff --check`; `node tools/ai.mjs
  reflect`.
