# Project Status

Short live project-status index. Workflow rules live in `tasks/README.md`.
Historical task evidence belongs in `tasks/archive/`; project-specific design
and review evidence belongs under `gamedesign/projects/<game-id>/`.

## Current Goal

Keep the reusable AI-first game pipeline lean and pointed at the right target
(a game that reaches the fake shot's direction and is fun), and keep the
template clean for the next prototype. The `E003` pipeline cleanup milestone is
done.

## Active Product State

- Active game concept: none. The repository is a clean reusable template ready
  for the next prototype (`src/clean_seed_main.c`).
- The Splash Rods (fishing) and Rune Marches test prototypes were removed
  entirely -- code, runtime, tooling, and design folders. History is in git and
  `tasks/archive/`; their durable lessons are encoded in `AGENTS.md` + skills.
- Pipeline cleanup epic `E003` is essentially complete (see Current Gate).

## Source Pointers

- Reusable process: `AI_PIPELINE.md`, `tasks/README.md`, `.codex/skills/`.
- Current taskboard: `node tools/taskboard/cli.mjs summary`.
- Startup gate: `node tools/game_context/iteration_context.mjs`.
- New prototype kickoff:
  `node tools/game_context/new_prototype.mjs --game-id <id> --title "<name>" --brief "<one sentence>"`.
- Closed work logs (history): `tasks/archive/E001/` (rune-marches),
  `tasks/archive/E002/` (fishing), `tasks/archive/E003/`, `tasks/archive/unassigned/`.

## Blocking Work

- None. The closed prototypes are fully removed; do not re-add prototype-specific
  game code/assets/tooling to the template. Start the next game as a fresh
  project under `gamedesign/projects/<game-id>/`.

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
- Active cleanup task: none. E003 cleanup is essentially complete; the next
  move is a fresh prototype, not more pipeline work.
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
  - `T0054`-`T0063`: post-review fixes; deep-reflection chain removed (-3.4k
    LOC); `--full` skips redundant in-export reruns; python mask dedup; dropped
    `tool_layers.json`; context practices (subagent return-contract, model
    tiering, cache protection); ponytail build-less ladder borrowed (no dep);
    `T0063` removed the closed prototypes entirely (clean template).

## Next Priorities

All E003 cleanup tasks `T0043`-`T0063` are done (see Current Gate). `T0057` was
resolved: the visual gate stays QUALITATIVE/advisory -- the fake shot is
inspiration, not a pixel target, so image-similarity scoring was rejected and
removed. No open cleanup tasks remain.

Next move: **start the next prototype.** Pick a concept, create a fresh
`gamedesign/projects/<game-id>/` wiki + tasks in `active/` (Stage 0 kickoff),
and from the very first screen apply the binding visual definition of done
(reaches the fake shot's direction + the core moment feels right; visual-first
freeze; <=3 FTUE beats).

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
