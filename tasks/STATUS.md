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

- Gate: reusable pipeline cleanup -- 2026-06-15 review milestone DONE.
- The full pipeline review backlog `T0043`-`T0053` is complete. Speed: quick is
  the validate default + tmp auto-prune; profiling is passive/advisory (no
  forced ceremony); the validation-planner machinery was removed; UI-asset
  gates are tiered (normal iteration ~2 cmds, full battery final-only); 5
  AI_PIPELINE docs -> 2; a Context Budget guideline. Quality: "done" redefined
  in AGENTS.md as screen-vs-fake-shot match + core moment feels right, with a
  continuous visual gate and visual-first freeze; a fun/reference-feel owner in
  `game-feature-iteration`; runtime art must hit the fake-shot bar; first-screen
  scope cap (<=3 FTUE beats). Net principle applied: subtract, don't add.
- Active cleanup task: none. Next = post-implementation review to find any
  remaining bottlenecks, then the next prototype when the lead starts one.
- Completed cleanup slices:
  - `T0013`-`T0034`: closed fishing, cleaned status/state, added startup/visual/
    slice gates, visual-strict rubric, and critic packets.
  - `T0035`-`T0042`: built a validation planner -- SUPERSEDED and removed by
    `T0047`.
  - `T0043`-`T0053`: quick-default validate + tmp prune; passive profiling
    (advisory guard, supersedes `T0028`); binding fake-shot gate + visual-first
    freeze; fun/feel owner; validation-planner removal; merged asset skills +
    shared reference doc; tiered UI gates; runtime-art bar; 5 AI_PIPELINE docs
    -> 2; first-screen scope cap; context-budget guideline.

## Next Priorities

The 2026-06-15 review backlog `T0043`-`T0053` is complete (see Current Gate).

Next:
1. Post-implementation review: find and optimize any remaining pipeline
   bottlenecks now that the obvious ones are fixed.
2. Start the next prototype when the lead picks a concept: create a fresh
   `gamedesign/projects/<id>/` wiki + tasks in `active/` (Stage 0 kickoff),
   and apply the new binding visual definition of done from the first screen.

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
