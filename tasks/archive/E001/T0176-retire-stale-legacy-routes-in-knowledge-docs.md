---
id: T0176
title: Retire stale legacy routes in knowledge docs
status: done
epic: E001
priority: P2
tags: [knowledge, assets, runtime-automation, legacy, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Remove stale `tools/` and `docs/ai-pipeline` routing from active agent-facing
and reusable knowledge docs after the AI Studio migration. Keep only intentional
guard/test references and local ignored settings out of scope.

## Done when

- [x] Reviewed stale route references after tracked legacy files were removed.
- [x] Updated `AGENTS.md` so new AI-pipeline docs/tools route to `ai_studio/`.
- [x] Updated generated-art knowledge docs to use
      `ai_studio/assets/workflow/art_jobs/`.
- [x] Updated live-state and runtime capture guidance away from `tools/<id>/`.
- [x] Updated Bootstrap template copy docs away from `tools/<id>/`.
- [x] Verified docs, bootstrap, runtime automation, map, and taskboard.
- [x] Committed and pushed the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after legacy files were removed but route search still
  found active guidance pointing to `tools/assets/...`,
  `tools/<game-id>/capture_states.py`, and legacy `docs/`/`tools/` locations.
- 2026-07-01: Updated active routes to `ai_studio/assets/workflow/art_jobs/`,
  asset prep/quality docs, and game-owned runtime capture drivers. Left
  synthetic test references and local untracked `.claude/settings.local.json`
  out of scope.
- 2026-07-01: Validation passed: bootstrap tests 3/3, state capture tests 3/3,
  doc reference check, `validate_map --strict`, and taskboard validation.
- 2026-06-30: Retired stale tools/docs routes in AGENTS, reusable knowledge, runtime capture guidance, and bootstrap template docs; validated bootstrap/runtime/docs/map/taskboard.
