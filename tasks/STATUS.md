# Project Status

Short live project-status index. Workflow rules live in `tasks/README.md`.
Historical task evidence belongs in `tasks/archive/`; project-specific design
and review evidence belongs under `gamedesign/projects/<game-id>/`.

## Current Goal

Optimize the reusable AI-first game development pipeline after the Splash Rods
fishing prototype test. The fishing game is closed; future work should improve
task/status hygiene, asset pipeline boundaries, validation gates, and passive
profiling for the next game iteration.

## Active Product State

- Active game concept: none. This repository is in pipeline/template
  improvement mode until the lead starts the next prototype.
- Closed test concept: `roblox-fishing` / `Splash Rods`. Its GDD, fake shots,
  runtime UI evidence, reviews, and failure reports remain under
  `gamedesign/projects/roblox-fishing/`.
- Legacy concept: `rune-marches`. Its old tasks are archived/dropped and
  should be treated only as historical evidence unless the lead reopens it.
- Active pipeline epic: `E003` reusable AI game pipeline cleanup.

## Source Pointers

- Reusable process: `AI_PIPELINE.md`, `tasks/README.md`, `.codex/skills/`.
- Current taskboard: `node tools/taskboard/cli.mjs summary`.
- Startup gate: `node tools/game_context/iteration_context.mjs`.
- New prototype kickoff:
  `node tools/game_context/new_prototype.mjs --game-id <id> --title "<name>" --brief "<one sentence>"`.
- Closed fishing project evidence: `gamedesign/projects/roblox-fishing/`.
- Closed work logs: `tasks/archive/E001/`, `tasks/archive/E002/`,
  `tasks/archive/E003/`, and `tasks/archive/unassigned/`.

## Blocking Work

- No active Splash Rods blocker remains. Fishing is closed as a test iteration.
- Do not continue Splash Rods gameplay, world art, UI kit, model selection, or
  visual rescue work unless the lead explicitly reopens that game.
- Do not continue Rune Marches product work unless the lead explicitly reopens
  that game.

## Current Gate

- Gate: reusable pipeline cleanup.
- Active cleanup task: none yet; pick the top open task below. The 2026-06-15
  full pipeline review produced a prioritized backlog `T0043`-`T0052` covering
  speed (default-quick validate, passive profiling, gate tiering, planner
  retirement, doc/skill consolidation) and quality (continuous binding visual
  fake-shot gate, a fun/reference-feel owner, runtime-art quality bar,
  first-screen scope discipline). Core diagnosis: gates were advisory/post-hoc
  and validated artifacts not fun/ref-match, and machinery grew additively.
- Current review findings to fix, in order:
  1. Visual prototype workflow must start from fake shot vs native screenshot,
     not from code-first gameplay expansion.
  2. Product gate fail must stop feature/content expansion unless the lead
     explicitly accepts the debt.
  3. Profiling coverage must stay current across task switches; the fishing
     profile had near-zero wall-clock coverage, so review conclusions are
     incomplete unless the missing telemetry is named. `T0023` added a
     current-scope guard so future handoffs cannot silently use sparse
     telemetry, and `T0028` made strict slice hygiene require profiler guard
     evidence before handoff/commit.
  4. Generated UI must prove non-empty crop/runtime manifests and pixel audit
     before runtime integration claims.
  5. Prototype commits need review hygiene: scoped diffs, current evidence,
     known red gates, and checked push constraints.
- Completed cleanup slices:
  - `T0013`-`T0026`: closed fishing, cleaned old concept/status/state, added
    startup/profile/slice-hygiene guards, and split project assets.
  - `T0027`-`T0034`: added visual-first gates, strict product-read rubric,
    visual critic packets, and `node tools/ai.mjs critic`.
  - `T0035`-`T0042`: taught validation planning for product-gate,
    game-context, and asset-tool checks; made low profiling coverage show
    largest gaps, recovered validation checks by check id, and let
    `node tools/ai.mjs validate` plan from touched files including the
    pipeline validator, skill tooling, and state-codegen tests.

## Next Priorities

P0 done (2026-06-15): `T0043` quick-default + tmp prune; `T0044` passive
profiling (advisory guard, supersedes T0028); `T0045` binding fake-shot gate +
visual-first freeze (redefined "done" in AGENTS.md); `T0046` fun/reference-feel
owner in `game-feature-iteration`.

Remaining, in order:
1. P1 subtraction: `T0047` retire validation-planner machinery; `T0048` merge UI
   asset skills + shared reference deconstruction; `T0049` tier UI asset gates;
   `T0050` runtime-art quality bar; `T0053` cut per-session context load.
2. P2: `T0051` consolidate AI_PIPELINE docs; `T0052` first-screen scope cap.
3. Then: re-run a pipeline review pass and optimize remaining bottlenecks.

## Validation Policy

- Task/status change: `node tools/taskboard/cli.mjs validate`.
- Taskboard tooling change: `node --test tools/taskboard/test.mjs`.
- Game-context tooling change: `node --test tools/game_context/test.mjs`.
- Product-gate tooling change: `node --test tools/product_gate/test.mjs`.
- Asset tooling change: run the narrow asset test(s) for the changed tool, e.g.
  `node --test tools/assets/validate_art_job.test.mjs`.
- AI profiling/tooling change: `node --test tools/ai_profile/test.mjs`.
- Skill/process change: `node tools/skills_eval.mjs`.
- Reusable pipeline-base change: quick `node tools/pipeline_validate.mjs`;
  full portable/export gate `node tools/pipeline_validate.mjs --full`.
- Playable prototype change: native PC build and screenshot/input proof first.
- Web/mobile/browser validation is out of scope unless the lead explicitly
  approves web work for the current task.
