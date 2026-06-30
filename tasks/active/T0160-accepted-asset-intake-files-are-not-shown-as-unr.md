---
id: T0160
title: Accepted asset intake files are not shown as unregistered
status: review
epic: ""
priority: P1
tags: [assets, storage, intake]
created: 2026-06-30
updated: 2026-06-30
---

## What

Decide and implement the intake cleanup rule for accepted candidates so files
that already have Pack Manifest records are not also indexed from `_incoming/`
as unregistered unknown-license assets.

## Done when

- [x] The accepted-candidate lifecycle is clear in `ai_studio/assets/storage/intake/README.md`.
- [x] Accepted files no longer appear as unregistered duplicates in Asset Viewer/search.
- [x] The behavior is covered by intake or index tests.
- [x] External storage cleanup is explicit and does not delete unaccepted candidates.

## Open questions

- Resolved: `accept.mjs` moves accepted candidate folders to `_accepted/`.
- Resolved: `_accepted/` keeps audit metadata; Asset Index skips `_accepted/`
  and `_rejected/`.

## Log

- 2026-07-01: Review found two accepted examples still indexed from external
  `_incoming/` as unregistered: Khronos `Box.glb` and Poly Haven
  `brown_mud_leaves_01_diff_1k.jpg`.
- 2026-07-01: Implemented lifecycle rule: accepted candidates move to
  `_accepted/`; rejected candidates stay in `_rejected/`; only undecided
  `_incoming/` files are shown as unregistered.
- 2026-07-01: Added legacy-safe index suppression for `_incoming` files whose
  `intake.json` hash matches an already registered Pack Manifest resource. This
  keeps older external storage clean in the viewer without deleting files.
- 2026-07-01: Evidence: intake/index/search tests pass; global search now
  returns `total: 0` for `--origin unregistered` and `--license unknown`.
- 2026-06-30: Started intake lifecycle fix: accept should archive staged candidates and index should avoid showing already-accepted incoming files as unregistered.
- 2026-06-30: Validated: asset storage tests 66/66, architecture map validate ok, taskboard validate ok, skills sync ok, global-library unregistered search total 0.
