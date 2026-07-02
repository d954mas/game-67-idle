---
id: T0225
title: "Asset viewer: pretty previews regressed after gallery module move"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Found by the lead during the 2026-07-02 live verification pass, right after
the T0220 sweep moved the gallery module to `ai_studio/assets/gallery/`
(routes `/asset_viewer/` + `/viewer/` kept): "В asset viewer пропали красивые
превью. Там теперь генерация а не красивое превью" - the gallery used to show
the curated/pretty preview images; now it shows the generation(-ish) image
instead. Suspect the module move broke a preview-resolution path (preview
file lookup relative to the old module location, or the preview route/asset
path not repointed in the T0220 move).

Investigate: how the gallery resolved "pretty previews" before commit
`494ae3dd` (T0220) vs now; check preview-related routes/paths in
`ai_studio/assets/gallery/` against the pre-move `ai_studio/assets/viewer/`
history. Restore the previews exactly as they were; add a regression test if
the resolution logic is testable.

## Done when

- [x] root cause identified and written in this task's log (what the move broke)
- [x] pretty previews show again in /asset_viewer/ exactly as before the move (self-healed; headless verify 427/427 curated webp)
- [x] regression guard where testable; gallery test suite + gates green
- [ ] lead re-verified live

## Open questions

## Log
- 2026-07-02: Created during lead's live verification (checklist item: gallery works as before - FAILED on previews).
- 2026-07-02: INVESTIGATED (deep-reasoner) + latent fix ACCEPTED, commit 53aad691. VERDICT: the T0220 move is EXONERATED - git proves api.mjs/viewer.js were pure git-mv, resolution modules byte-identical, server routes unchanged. Lead's symptom = TRANSIENT stale on-disk index (tmp/ai_studio/assets/asset_index/global-library.sqlite built 2026-06-30 pointed at pre-reorg preview paths at the library root; previews later moved into packs/<pack>/previews/). Self-heals: buildSourceSnapshot hashes all tracked files so ensureAssetIndex rebuilds on next library change. Live state verified healthy: 427/427 thumbnails curated /previews/*.webp, API 3775/3776 correct (1 font w/o preview), pack covers 109/110. REAL LATENT BUG FIXED while there: previews/cache.mjs assetIdFor degraded to the ABSOLUTE path on Windows (startsWith failed across / vs \) so folder-scanned sources' preview-cache keys never matched the index reader's source-relative ids -> now relative(sourceRoot, path); regression test pins it. Affects folder .glb sources only (none currently registered - verified synthetically in node). Server restart needed for the refresh path (cache.mjs is server-held) - DEFERRED to the T0222 acceptance restart. OPERATIONAL REMEDY if raw images ever reappear: POST /api/asset-viewer/reindex for global-library.
