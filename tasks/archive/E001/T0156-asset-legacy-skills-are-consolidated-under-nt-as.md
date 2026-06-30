---
id: T0156
title: Asset legacy skills are consolidated under nt-asset-workflow
status: done
epic: E001
priority: P1
tags: [assets, skills, legacy]
created: 2026-06-30
updated: 2026-06-30
---

## What

Asset work should route through one main AI Studio skill:
`.codex/skills/nt-asset-workflow/SKILL.md`.

Legacy asset skills such as `game-3d-models`, `game-texture-generation`, and the
empty `game-texture-atlas-pipeline` should not remain as parallel public
entrypoints. Preserve useful instructions as `nt-asset-workflow` references, then
remove stale entrypoints and update references.

## Done when

- [x] 3D model sourcing/conversion/runtime guidance is preserved under
      `nt-asset-workflow`.
- [x] Texture/material guidance is preserved under `nt-asset-workflow`.
- [x] Legacy asset skill entrypoints are removed or converted to thin pointers.
- [x] Repo docs/scripts no longer tell agents to use old asset skill names.
- [x] Architecture map validates with the new skill ownership.

## Open questions

## Log
- 2026-07-01: Started review after T0152-T0155. Found old asset skills still visible alongside `nt-asset-workflow`; `game-texture-atlas-pipeline` has no tracked `SKILL.md` and is only an empty directory shell.
- 2026-07-01: Moved useful 3D model and texture/material guidance into `nt-asset-workflow` references, removed old tracked skill entrypoints, and updated bootstrap references to the main asset skill.
- 2026-07-01: Validation: old asset skill names only remain in this task's historical notes; `node ai_studio/architecture_map/validate_map.mjs`, `node ai_studio/taskboard/cli.mjs validate --json`, and focused asset JS tests passed.
- 2026-06-30: Moved legacy asset skill content into nt-asset-workflow references; old entrypoints removed; validation passed.
- 2026-06-30: Review adjustment: restored concrete 3D engine integration checklist and minimum texture source-record fields inside nt-asset-workflow references; validation re-run passed.
- 2026-06-30: 2026-07-01: Review closeout passed on current state: asset JS tests 112/112, Python prep tests 65/65, restricted asset guard dev/release green, architecture map validation clean, taskboard validation clean, doc reference check clean, skills sync clean.
