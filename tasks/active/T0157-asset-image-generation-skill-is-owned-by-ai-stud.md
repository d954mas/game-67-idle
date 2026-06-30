---
id: T0157
title: Asset image generation skill is owned by AI Studio assets
status: review
epic: E001
priority: P1
tags: [assets, skills, generation]
created: 2026-06-30
updated: 2026-06-30
---

## What

The old `delegated-image-generation` skill is useful but asset-owned: it creates
raw raster source images after source-first search fails. It should be a
`nt-asset-*` skill and route back into `nt-asset-workflow` for prep, storage,
provenance, and quality evidence.

## Done when

- [x] Skill directory and metadata use `nt-asset-image-generation`.
- [x] All internal script/reference paths use the new skill path.
- [x] Asset prep tools that call generation scripts use the new path.
- [x] `nt-asset-workflow` explicitly routes raster generation to this skill.
- [x] AI Studio architecture map owns the skill under Assets.
- [x] Validation and focused tests pass.

## Open questions

## Log
- 2026-07-01: Started after T0156. `delegated-image-generation` has useful scripts and references, so this is a rename/ownership migration, not deletion.
- 2026-07-01: Renamed skill to `nt-asset-image-generation`, updated generation paths, updated `route_cutout.py` auto-dual script path, added skill/scripts to `ai_studio/tree.json`, and linked it from `nt-asset-workflow`.
- 2026-07-01: Fixed `skills_sync.mjs` to accept UTF-8 BOM before skill frontmatter; regenerated `.claude/skills` pointers and added a regression test.
- 2026-07-01: Validation: `skills_sync --check`, `skills_sync.test.mjs` 4/4, taskboard validate, architecture map validate, and focused asset JS tests 33/33 passed. Old asset skill names are absent from current `.codex`, `.claude`, `ai_studio`, `template`, and `tools` surfaces.
