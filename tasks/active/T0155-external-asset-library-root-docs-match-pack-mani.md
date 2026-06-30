---
id: T0155
title: External asset library root docs match pack manifest structure
status: review
epic: E001
priority: P2
tags: [assets, library, docs]
created: 2026-06-30
updated: 2026-06-30
---

## What

The external shared asset library has moved to `packs/<pack>/pack.json` plus
`assets.jsonl`, but the root `README.md` and `index.md` still describe the old
OKF/Markdown catalog. Update or replace those docs so agents and humans start
from the real pack-manifest structure.

## Done when

- [x] Root library docs describe Pack Manifest as the source of truth.
- [x] Old OKF/catalog navigation is removed or clearly marked historical.
- [x] `asset_source.json` exists if the library should be self-describing.
- [x] Docs explain intake, restricted assets, previews, and search/reindex route.
- [x] No repo docs still instruct agents to use the removed OKF catalog path as
      the current workflow.

## Open questions

## Log
- 2026-06-30: Start: update external shared asset library root docs from old OKF/catalog wording to Pack Manifest source-of-truth structure.
- 2026-07-01: Updated external library README/index/asset_source.json to Pack Manifest source-of-truth docs and replaced current repo docs that still referenced old find_assets/OKF navigation.
- 2026-07-01: Verified external asset_source.json parses without BOM, asset JS tests pass 105/105, taskboard validate is clean, and architecture map validation is clean.
