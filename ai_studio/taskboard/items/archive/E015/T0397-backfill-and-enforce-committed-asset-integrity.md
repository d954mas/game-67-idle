---
id: T0397
title: Backfill and enforce committed asset integrity
status: done
project: P001
epic: E015
priority: P1
tags: [assets, integrity, provenance]
created: 2026-07-10
updated: 2026-07-12
---

## What

Backfill complete integrity metadata for every committed asset, then make
missing or mismatched integrity fail validation through the existing asset
workflow. This task owns metadata and enforcement, not asset redesign.

## Done when

- [x] A tracked inventory classifies every committed binary as product asset,
      generated/procedural output, test fixture, font, or external boundary.
- [x] Every committed asset has license, provenance, origin, and verified
      SHA-256 in its owning manifest; non-redistributable binaries remain out
      of git.
- [x] Existing prose-only records are migrated once without inventing origin;
      unknown provenance fails and requires lead disposition.
- [x] Font OFL/origin records, generated HDR routing, PNG/ntpack fixtures,
      audio manifests, and current game/template assets are covered by tests.
- [x] `games/web-dressup/design/data/asset_manifest.json` is reconciled so
      committed assets are not left marked as merely `needed`.
- [x] Validation fails early on missing metadata, missing file, unexpected
      file, hash mismatch, invalid license/origin, or duplicate conflicting ID.
- [x] Validator output is compact and scoped; unchanged valid libraries do not
      force agents to load full manifests.
- [x] Windows/Linux validation and Taskboard checks pass.

## Open questions

None.
## Log

- 2026-07-10: Split from T0358 after full tracked-file coverage confirmed
  metadata incompleteness but found no additional unowned binary class.
- 2026-07-10: Dependency fixed: execute after T0358 legacy deletion.
- 2026-07-10: Dependency tightened after review: execute after both T0358 legacy deletion and T0356 prototype retirement, so deleted rb-dark-rpg assets are not backfilled.
- 2026-07-12: Checkpoint after T0354 at 0b8b5b347. Backfill and enforce committed asset integrity in an isolated worktree; preserve T0393 audio WIP, E017 planning, and game implementation work.
- 2026-07-12: 2026-07-12: Isolated implementation and three review/fix cycles complete: 0 HIGH and 0 actionable. Integrity core covers 110 binary blobs plus one gitlink, verifies all 110 hashes, and fails closed. Template scope is green; global QASSET_001 remains blocked on explicit public-redistribution disposition for 61 dress PNG, 13 procedural UI PNG, generated HDR, and hook_record_fast.exe. No license claim was invented.
- 2026-07-12: Lead approved the current bytes of 61 dress PNGs, 13 procedural UI PNGs, `studio_env.hdr`, and `hook_record_fast.exe` as canonical project-owned outputs with public redistribution, commercial-use, and modification rights; current SHA-256 values supersede stale prose records.
- 2026-07-12: Verification: Windows focused suite 28/28; Linux clean-snapshot suite 28/28; global guard on both platforms reports 110 tracked binary blobs, one external gitlink, 110 inventory entries, 110 metadata records, 110 verified, and zero issues. Architecture Map strict validation reports 0 unmapped, missing, duplicate, or invalid-description entries.
- 2026-07-12: Three independent review/fix cycles plus final inventory and validator closure rechecks completed with 0 HIGH and 0 actionable findings. The main-worktree integration intentionally excludes the two dirty `kenney-audio` manifest files owned by T0393; the clean committed snapshot proves T0397, and T0393 will reconcile and re-run the real-worktree gate.
- 2026-07-12: Quality: QASSET_001=pass; evidence: complete 1:1 committed-binary inventory, verified SHA/license/provenance/origin metadata, fail-closed scoped validator tests on Windows and Linux, global zero-issue guard, strict Architecture Map, and independent review closure.
- 2026-07-12: Quality: QASSET_001=pass; evidence: Windows and Linux 28/28; global guard 110/110 verified, 0 issues; Architecture Map strict green; independent reviews 0 HIGH and 0 actionable
