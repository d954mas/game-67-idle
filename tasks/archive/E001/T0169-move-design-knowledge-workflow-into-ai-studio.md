---
id: T0169
title: Move design knowledge workflow into AI Studio
status: done
epic: E001
priority: P2
tags: [game-design, skill, refactor, knowledge]
created: 2026-06-30
updated: 2026-06-30
---

## What

Move the legacy `design-source-knowledge` workflow into reviewed AI Studio
ownership without moving the actual design knowledge data.

Target model:

- `gamedesign/knowledge/` and `gamedesign/sources/` remain durable design data.
- `ai_studio/game_design/knowledge/` owns agent workflow docs for routing,
  source intake, reference review, and templates.
- `.codex/skills/nt-design-knowledge` becomes a thin skill surface that points
  agents to the AI Studio module.

## Done when

- [x] Canonical workflow docs live under `ai_studio/game_design/knowledge/`.
- [x] Legacy `design-source-knowledge` skill is migrated to
      `nt-design-knowledge`.
- [x] Old skill reference docs are removed from `.codex/skills`.
- [x] `ai_studio/tree.json` owns the new module and skill surface.
- [x] Generated `.claude/skills` pointers are synced.
- [x] Skill validation, map validation, doc reference check, and taskboard
      validation pass.

## Open questions

- None yet.

## Review

### Moved

- `.codex/skills/design-source-knowledge` ->
  `.codex/skills/nt-design-knowledge`.
- Skill-local reference docs moved into
  `ai_studio/game_design/knowledge/`:
  `source_routing.md`, `source_intake_promotion.md`,
  `reference_work_review.md`, and `templates.md`.

### Ownership

- `ai_studio/game_design/` now owns reviewed game-design workflow surfaces.
- `ai_studio/game_design/knowledge/` owns workflow docs for source routing,
  source intake, reusable knowledge promotion, reference-backed work, and
  templates.
- `gamedesign/knowledge/` and `gamedesign/sources/` remain durable design data,
  not AI Studio workflow code.
- `nt-design-knowledge` is now a thin skill router to the AI Studio module.

### Cleanup

- Removed stale generated `.claude/skills/design-source-knowledge` pointer and
  generated `.claude/skills/nt-design-knowledge`.
- Updated `tasks/archive/README.md` so it no longer says detailed procedures
  always live under `.codex/skills/*/references/`.
- Historical mention in `gamedesign/knowledge/log.md` remains intentionally as
  past log evidence.

### Validation

- `py -3.12 .../quick_validate.py .codex/skills/nt-design-knowledge`: valid.
- `node ai_studio/core_harness/agent_surfaces/skills_sync.mjs --check`: ok.
- `node ai_studio/architecture_map/validate_map.mjs`: ok, missing=0,
  unmapped_ai_studio=0, unmapped_legacy=35.
- `node ai_studio/core_harness/validation/doc_reference_check.mjs`: ok.
- `node ai_studio/taskboard/cli.mjs validate --json`: ok.

## Log

- 2026-07-01: Created after map review showed `design-source-knowledge` as the
  next small unmapped legacy skill group.
- 2026-07-01: Moved design knowledge workflow docs into AI Studio, migrated the
  skill to `nt-design-knowledge`, synced generated agent surfaces, and validated
  the map/taskboard/docs.
- 2026-06-30: Moved design knowledge workflow into ai_studio/game_design/knowledge and migrated skill to nt-design-knowledge.
