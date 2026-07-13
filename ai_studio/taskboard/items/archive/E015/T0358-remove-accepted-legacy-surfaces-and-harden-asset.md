---
id: T0358
title: Remove accepted legacy surfaces and repair confirmed text corruption
status: done
project: P001
epic: E015
priority: P1
tags: [cleanup, legacy, encoding]
created: 2026-07-10
updated: 2026-07-11
---

## What

Delete only the legacy surfaces explicitly accepted during review and repair
the confirmed atlas-label mojibake. Skeletal quarantine and asset-integrity
backfill are split to T0396 and T0397.

## Done when

- [x] Repository search proves zero live callers/routers before deleting
      `mcps/tasks/`, unused `external/cjson`, `templates/design`, and
      `state_system_design`.
- [x] The exact five feature-local build specs are deleted only after their
      durable decisions are linked from current owning contracts:
      `features/items-core/docs/build_spec_stack_int_2026-07-08.md`,
      `features/game-state/references/build_spec_a1_a3_2026-07-06.md`,
      `build_spec_a4_2026-07-06.md`, `build_spec_a5_2026-07-07.md`, and
      `build_spec_a6_2026-07-07.md`.
- [x] Current short state/feature contracts, workflows, and review evidence
      remain reachable after links to deleted material are updated.
- [x] Confirmed `???` mojibake in
      `ai_studio/assets/tools/review_atlas/atlas_review_labels.py` is repaired
      and a UTF-8/syntax smoke prevents recurrence.
- [x] No unrelated history, current feature contract, asset, or private-game
      artifact is deleted.
- [x] Targeted searches, affected tests, and Taskboard validation pass.

## Open questions

None.
## Log

- 2026-07-10: Final convergence split skeletal experimental quarantine to
  T0396 and asset integrity to T0397 so this task has one cleanup context.
- 2026-07-10: Resolved planning detail: cleanup is the exact allow-list in Done when; no broad sweep.
- 2026-07-11: Checkpoint: all accepted allow-list paths were rechecked before deletion. state_system_design is already absent; mcps/tasks, external/cjson, templates/design, the exact five feature-local build specs, and the atlas label script remain. Starting zero-caller/link proof and durable-decision routing only; T0396 skeletal quarantine, T0397 asset integrity, T0393 audio WIP, external/neotolis-engine, and E016 stay out of scope.
- 2026-07-11: Checkpoint correction: the allow-list name state_system_design resolves to tracked features/game-state/references/state_system_design_2026-07-06.md; only a root-level path was absent. The tracked design file remains in deletion scope after its durable contract/workflow/review decisions are confirmed reachable.
- 2026-07-11: Evidence: deleted only the accepted legacy allow-list; current
  stack and four-fragment state contracts now own the durable decisions, and
  repository search reports zero references to deleted paths or orphaned
  design/build-spec section labels.
- 2026-07-11: Evidence: Items ops 17/17, state codegen/modules 45/45, atlas
  label/build/audit 39/39, Items Viewer 16/16, 13 changed Python files compile,
  doc-reference validation, strict Architecture Map validation
  (`mapped=347`, `scanned=779`, all error counts zero), Taskboard validation,
  and cached diff check pass.
- 2026-07-11: Evidence: isolated staged-tree CMake configure built engine-owned
  cJSON and `test_game_state_json` plus `test_game_save`; focused CTest passed
  2/2, proving removal of the unused root cJSON copy does not break consumers.
- 2026-07-11: Review convergence: three cycles resolved contract-envelope,
  state error/compatibility, deleted-spec citation, malformed-comment, UTF-8,
  and clean-build findings. Final independent architecture and process reviews
  report 0 HIGH and 0 actionable findings; T0393 audio remains unstaged.
- 2026-07-11: Quality: QTECH_001=pass; evidence: exact allow-list deletion,
  durable owner contracts, UTF-8 regression coverage, 117 focused checks,
  isolated cJSON build/CTest proof, strict repository validation, and two clean
  independent final reviews.
- 2026-07-11: Closed after three review cycles: 0 HIGH, 0 actionable; focused validation and isolated cJSON CTest proof pass.
