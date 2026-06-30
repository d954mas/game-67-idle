---
id: T0185
title: Audit remaining asset legacy surfaces
status: done
epic: E001
priority: P1
tags: [assets, legacy, audit]
created: 2026-07-01
updated: 2026-07-01
---

## What
Audit the current Asset Studio module after the storage/viewer/license refactor:
find remaining legacy asset surfaces, check whether reviewed asset docs and
skills still point to old concepts, and produce the next concrete cleanup slice.

## Done when

- [x] Search old `tools/`, `docs/`, and `.codex/skills` asset surfaces.
- [x] Review current `ai_studio/assets` ownership boundaries.
- [x] Identify concrete cleanup issues: delete, keep, merge, or defer.
- [x] Apply a small safe cleanup when evidence is clear.
- [x] Validate map, docs, taskboard, and focused asset tests.
- [x] Commit and push the slice.

## Open questions

- None yet.

## Audit findings

- Current repo legacy scan found no obvious old asset tools/docs outside
  reviewed `ai_studio/assets/` and `nt-asset-*` skills.
- `ai_studio/assets/` ownership is coherent: `storage/` owns sources, manifests,
  index, snapshots, previews, and license guard; `viewer/` owns the browser
  surface and export helpers; `prep/` owns asset preparation; `workflow/` owns
  generated-art job records.
- External global library is already Pack Manifest based:
  `asset_source.json`, `packs/<pack>/pack.json`, `assets.jsonl`; it currently
  reports 110 packs and 3776 assets.
- Old Markdown files in the external library are human notes/templates, not the
  source of truth. Keep root `README.md`, `index.md`, and `log.md` as human
  documentation; `_templates/*.md` are legacy authoring helpers and can be
  deleted later if they stop helping intake.
- `build_review.mjs`, `serve_gallery.mjs`, and `record_gallery.mjs` are not the
  normal site path. Keep them as export/demo helpers, but they should stay
  clearly secondary to Studio Shell `/asset_viewer/`.
- Search health on the real library is good: cached query was about 93 ms,
  unchanged refresh was about 665 ms, and Poly Pizza Animated Enemies returns
  five model assets with multi-pack membership.
- Concrete bug fixed: `record_gallery.mjs` error cleanup no longer kills every
  `chrome.exe`; it now only attempts to stop the Chrome process it spawned.

## Next candidates

- Add a small testable seam around `record_gallery.mjs` if recorder behavior is
  edited again; current change is intentionally minimal.
- Later decide whether `_templates/*.md` in the external library should be
  deleted or replaced by command examples in `ai_studio/assets/storage/intake/`.
- Later rename prep-only schemas such as `game.asset_manifest` /
  `game.review_atlas` if they become confusing with Pack Manifest storage.

## Log

- 2026-07-01: Started after the pull publishability fix. Initial search found
  no obvious old asset tools/docs by filename outside reviewed `nt-asset-*`
  skills; next pass is content and module-boundary review.
- 2026-07-01: Reviewed asset skills, module README files, repo legacy paths, and
  external global library shape. Fixed the recorder cleanup bug that could kill
  user Chrome on failure.
- 2026-07-01: Validated recorder syntax, 49 focused asset tests, architecture
  map, markdown references, and taskboard state.
- 2026-07-01: Audited current asset module and legacy surfaces; fixed recorder cleanup to avoid killing user Chrome; validated focused asset tests/map/docs/taskboard.
