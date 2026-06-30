---
id: T0165
title: Review asset module and legacy asset surfaces
status: done
epic: E001
priority: P2
tags: [assets, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What

Review the current `ai_studio/assets` module and asset-related legacy surfaces
before the next cleanup step. Keep the review practical: what works, what is
still legacy, what should be moved/deleted, and what should be optimized now.

## Done when

- [x] Current asset module structure is mapped.
- [x] Asset storage/viewer/prep/workflow tests are checked.
- [x] External shared asset library shape is inspected.
- [x] Legacy asset references are classified as current, historical, or follow-up.
- [x] One concrete low-risk improvement is implemented and validated.
- [x] Decide whether to clean legacy OKF metadata from the external asset library.

## Open questions

None.

## Review

### Current Structure

- `ai_studio/assets/viewer/` is the browser surface. It chooses registered
  sources, renders packs/assets, uses lazy asset batches, and delegates index,
  preview, license, pull, and promote logic to storage modules.
- `ai_studio/assets/storage/` owns source registries, Pack Manifest metadata,
  generated SQLite indexes, snapshots, previews, intake, and license guard.
- `ai_studio/assets/prep/` owns local preparation helpers: crop, cutout, source
  sheet checks, review atlas, textures, and conversion.
- `ai_studio/assets/workflow/` owns generated-art job/provenance workflows.
- `.codex/skills/nt-asset-workflow` is a thin asset router and correctly points
  agents to `ai_studio/assets`.

### External Library

- Root: `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- Current canonical shape is Pack Manifest: `packs/<pack>/pack.json` plus
  `assets.jsonl`.
- Inspected counts: 110 packs, 3776 manifest records, 0 missing resources,
  0 missing previews.
- Remaining attribution debt: 850 CC-BY/attribution-required records without
  `credit_text`. This is allowed for development but should fail release checks.
- Legacy evidence remains in external metadata: `legacy_okf_catalog` fields,
  `migration-okf-to-manifests.json`, and small pack READMEs with migration text.
  Current repo code does not route agents to OKF.

### Validation

- `node --test ai_studio/assets/**/*.test.mjs`: 113 passed.
- `py -3.12 -m pytest ai_studio/assets/prep`: 65 passed.
- `node ai_studio/assets/storage/license/restricted_assets_guard.mjs`: ok.
- `node ai_studio/assets/storage/search.mjs --query crate --json`: works.

### Improvement Made

`ai_studio/assets/storage/search.mjs` used to call `refreshAssetIndex()` on
every search, which forced a filesystem snapshot check before every agent query.
Search now reads the existing generated index by default and uses `--refresh`
only when local files or manifests changed.

Measured on the current global library:

- default search: about 71 ms;
- explicit `--refresh`: about 711 ms.

The new behavior is covered by
`ai_studio/assets/storage/tests/search.test.mjs`.

### Next Candidate

Asset storage is ready for the next functional slice. Good candidates are:

- release attribution report for the 850 CC-BY/attribution-required records that
  still need final `credit_text`;
- Asset Viewer UI review after the storage/index cleanup;
- next legacy asset-related skill review.

### External Cleanup

Cleaned `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`:

- removed `legacy_okf_catalog` from 3776 asset records;
- removed `legacy_okf_catalog` from 110 pack records;
- removed 110 pack README migration lines;
- removed top-level OKF migration references from README/index/asset_source/log;
- deleted `migration-okf-to-manifests.json`;
- backup of the deleted migration file is in
  `tmp/ai_studio/assets/okf-cleanup-backup/`.

Post-cleanup verification:

- packs: 110;
- asset records: 3776;
- missing resources: 0;
- missing previews: 0;
- remaining precise OKF legacy patterns: 0.

## Log

- 2026-07-01: Created task for current asset module + legacy surface review.
- 2026-07-01: Verified asset tests: 112 JS tests, 65 prep Python tests, license
  guard ok.
- 2026-07-01: Inspected external library shape: Pack Manifest is current source
  of truth; OKF remains only as historical migration metadata.
- 2026-07-01: Optimized `search.mjs`: default search no longer refreshes the
  asset index; use `--refresh` explicitly after local changes.
- 2026-07-01: Cleaned external asset library OKF migration metadata and
  verified no asset/previews were lost.
- 2026-06-30: Asset module review completed; external OKF migration metadata cleaned; tests and map validation passed.
