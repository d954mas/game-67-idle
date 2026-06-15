---
id: T0013
title: Close fishing iteration and split project-specific asset builders
status: done
epic: E003
priority: P0
tags: [pipeline, assets, taskboard, profiling]
created: 2026-06-15
updated: 2026-06-15
---

## What

Close the Splash Rods fishing prototype as a completed test iteration and
turn the review findings into reusable pipeline improvements. The immediate
cleanup is to stop treating the fishing visual rescue as current product work,
split fishing UI asset generation out of the Rune Marches asset builder, and
leave validation evidence for the next game iteration.

## Done when

- [x] `tasks/STATUS.md` no longer names fishing as the active current goal or
      product blocker.
- [x] E002 fishing tasks are closed honestly: delivered prototype/GDD/UI work
      marked done, abandoned visual rescue marked dropped with reason.
- [x] Fishing UI asset build has a project-specific entry point instead of
      requiring the Rune Marches builder command.
- [x] The reusable asset-pipeline skill or docs record the rule that project
      builders must not absorb unrelated game assets.
- [x] Validation proves taskboard consistency and the fishing UI asset gate
      still passes.

## Open questions

- none

## Log

- 2026-06-15: Created after lead decision that the fishing game iteration is
  finished and future work should optimize the overall AI/game pipeline rather
  than continue product development on Splash Rods.
- 2026-06-15: Closed E002 by moving T0008/T0010/T0011 to done and T0012 to
  dropped. Updated `tasks/STATUS.md` to pipeline mode.
- 2026-06-15: Added `tools/assets/build_roblox_fishing_ui_assets.py` as the
  owner for fishing UI crops, cleanup, remap, manifests, and runtime PNGs.
  `tools/assets/build_rune_marches_assets.py` now only embeds already-built
  fishing runtime PNGs into the temporary shared C texture array; Rune data
  manifests no longer list fishing assets.
- 2026-06-15: Added the project-specific builder boundary rule to
  `.codex/skills/game-asset-pipeline/SKILL.md`. Verified with separate fishing
  UI builder, generated UI audit PASS, edge proof total=0, native CMake build,
  and taskboard validation.
- 2026-06-15: Broad reusable-pipeline validation passed:
  `node tools/skills_eval.mjs` and `node tools/pipeline_validate.mjs`.
- 2026-06-15: Done: fishing iteration closed, project-specific fishing UI builder split out, reusable asset-pipeline rule added, UI audits/native build/taskboard validation passed.
