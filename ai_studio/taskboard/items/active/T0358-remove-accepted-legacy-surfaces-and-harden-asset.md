---
id: T0358
title: Remove accepted legacy surfaces and harden asset integrity
status: backlog
project: P001
epic: E015
priority: P1
tags: [cleanup, assets, experimental]
created: 2026-07-10
updated: 2026-07-10
---

## What

Delete only the legacy surfaces explicitly accepted during review, accurately
label incomplete experimental work, and make asset integrity mandatory after a
one-time backfill.

## Done when

- [ ] Delete unused `mcps/tasks`, unused `external/cjson`, `templates/design`,
      the five feature-local build specs, and `state_system_design`; repository
      search proves no live caller or router remains.
- [ ] Preserve the short current state contract/workflow/review documentation
      and update links that previously routed through deleted material.
- [ ] Skeletal animation moves under `extensions/experimental/`, is marked
      incomplete with its real limits, and the confirmed `wrap_time` defect is
      documented with reproducible evidence; completing/fixing the runtime is
      not required and the extension is not deleted as legacy.
- [ ] Existing committed assets receive verified SHA-256 metadata in a reviewed
      backfill, after which missing/mismatched integrity fails validation.
- [ ] No unrelated historical source, current feature contract, or private game
      artifact is deleted by this cleanup.

## Open questions

- Identify the exact five feature-local build-spec paths from the audit before
  deletion and attach the zero-live-reference evidence.

## Log

- 2026-07-10: This task is intentionally an allow-list cleanup, not a broad
  repository sweep.
