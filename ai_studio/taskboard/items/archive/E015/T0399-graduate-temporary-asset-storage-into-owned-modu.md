---
id: T0399
title: Graduate temporary asset storage into owned modules
status: done
project: P001
epic: E015
priority: P1
tags: [assets, storage, decomposition, context]
created: 2026-07-10
updated: 2026-07-13
---

## What

Retire the explicitly temporary `ai_studio/assets/backlog/storage/` layout by
splitting its live catalog/index responsibilities into small owning asset
modules behind stable public operations.

## Done when

- [x] All consumers of the temporary storage path and its 1,173-line
      `index/index.mjs` are inventoried before moves.
- [x] Catalog/index, manifests, intake, licenses, previews, and source-library
      responsibilities have explicit owners; physical modules are split only
      where behavior and tests identify a real boundary.
- [x] Existing asset CLI/API/import paths remain stable through a short facade
      and parity tests; callers do not learn internal storage layout.
- [x] Default agent reads load a compact router and one requested domain, not
      the full asset catalog implementation or historical docs.
- [x] T0397 integrity/provenance rules are consumed rather than reimplemented.
- [x] The old `backlog/storage` path is removed after all imports/docs/tests
      migrate; no compatibility tree, service layer, database, DI, or event
      framework is introduced.
- [x] Focused asset tests and Taskboard validation pass.

## Open questions

None.
## Log

- 2026-07-10: Full-file coverage confirmed the README still calls this live
  storage path temporary; no prior E015/E016 task owned its graduation.
- 2026-07-10: Dependency fixed: execute after T0397 integrity contracts.
- 2026-07-13: Execution checkpoint after T0395 push. Inventory confirms 59 old-path references, 16 production import edges, and a 1,173-line index. Preserve T0397 integrity schemas/hashes, move project-owned binaries byte-for-byte, introduce only short owner facades, and keep E017/external engine out of scope.
- 2026-07-13: TDD RED proved all three missing ownership contracts before the move. Graduated 43 tracked storage files into six explicit owners, split catalog storage/source-record/index/query responsibilities, separated preview status, and removed the old tree with no compatibility layer.
- 2026-07-13: Evidence after review fixes: owner-domain tests 55/55; T0397 integrity tests 20/20 against an isolated final-diff Git index; gallery/items/new-template consumers 69/69; Studio routing 16/16; Architecture tree 10/10 and strict map 0 issues; old live-path references 0; `studio_env.hdr` SHA-256 remains `6249d61cb8e5534e6396a888b45732ab93716820a13b4ace7cd53d1565c90361`.
- 2026-07-13: Quality: QTECH_001=pass; QASSET_001=pass; evidence: parity tests exercise all owner facades and migrated callers, strict integrity consumes the unchanged T0397 schema/inventory contract, and the canonical HDR bytes match the approved hash at the new owner path.
- 2026-07-13: Implementation complete; status remains `doing` pending the required independent diff reviews and lead integration commit.
- 2026-07-13: Final gate: two independent staged-only reviews returned 0 HIGH/MEDIUM/LOW actionable; targeted owner, Studio, and Architecture checks passed; strict map, Taskboard, both skill validators, and Codex/Claude skill sync are green.
