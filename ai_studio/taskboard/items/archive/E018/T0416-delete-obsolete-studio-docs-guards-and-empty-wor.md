---
id: T0416
title: Delete obsolete Studio docs guards and empty workflow surfaces
status: done
project: P001
epic: E018
priority: P1
tags: [cleanup, canvas, tests, docs]
created: 2026-07-13
updated: 2026-07-13
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=42 obsolete files removed after invariant distillation; assets domain pass in 40.4s; focused cleanup/route/map/config proof green; final independent re-review 76/76 plus validators PASS."}]}
---

## What

Delete accepted obsolete surfaces instead of preserving compatibility layers.
Before deleting a historical Canvas document, move any unique live invariant
into its short owning contract; do not retain narrative history in active
routing or Architecture Map data.

## Done when

- [x] Canvas `contracts/full-reference.md`, PLAN/history/review/freeze/research
      files with no unique live contract, and their active links/map nodes are
      removed.
- [x] The empty asset backlog and its routing/map references are removed.
- [x] Migration-only structural tests and retired-path/CSS/count guards are
      removed while behavioral/API/privacy/provenance tests remain.
- [x] `surface_legacy_removal.test.mjs` and `tree_split.test.mjs` are removed;
      the one-file loader keeps confinement, malformed-input, literal-ref, and
      API containment coverage, while subtree coverage replaces test nodes.
- [x] No duplicate root-level compatibility path is introduced.
- [x] Settings and resource-panel remain feature packs and keep room for later
      expansion; they are not cleanup targets.
- [x] Studio Shell asset-viewer routing no longer references the retired
      `assets/backlog/storage/previews` path and uses the owned
      `ai_studio/assets/previews` store.

## Open questions

- File-by-file deletion is decided by live inbound references and unique
  invariants, not age alone. Git/history is sufficient for narrative history;
  only current unique invariants are distilled before deletion.

## Log

- 2026-07-13: Lead approved deletion of all surfaces that do not carry a live
  contract or required evidence.
- 2026-07-13: Accepted cleanup packet is in progress after file-by-file invariant audit; Architecture Map-specific deletions wait for T0413.
- 2026-07-13: Final close: deleted 42 obsolete Canvas/backlog/map-guard files, retained five short owner contracts, repaired both preview routes, archived three stale review/pause tasks, and removed all live deleted-doc references. Assets domain passed in 40.4s; independent final re-review PASS with 76/76 and validators clean; Settings/resource-panel untouched.
- 2026-07-13: Quality: QTECH_001=pass; evidence: QTECH_001=42 obsolete files removed after invariant distillation; assets domain pass in 40.4s; focused cleanup/route/map/config proof green; final independent re-review 76/76 plus validators PASS.
