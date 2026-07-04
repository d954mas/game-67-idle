---
id: T0273
title: "rb-dark-rpg art: стартовый комплект игрока - меч, броня, поножи"
status: backlog
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, art, assets, onboarding, items]
created: 2026-07-04
updated: 2026-07-04
---

## What

Create/source the visual starter gear set promised by the first mandatory guard
dialogue:

- `old_sword` / `asset_icon_old_sword`;
- `padded_jacket` / `asset_icon_padded_jacket`;
- `leather_greaves` / `asset_icon_leather_greaves`.

Primary use: inventory/reward cells in the first quest dialogue and equipment
screen. The icons must read clearly at 62px in the current dark RPG modal, not
only at full resolution.

Use source-first workflow: shared library/free sources first, generation only if
no suitable redistributable art is found. Keep provenance with every committed
asset.

## Done when

- [ ] Three final transparent PNG item icons exist under `games/rb-dark-rpg/assets/` with stable filenames.
- [ ] Icons read at small reward-cell size: sword, armour, and greaves are visually distinct without text labels.
- [ ] `games/rb-dark-rpg/design/data/asset_manifest.json` has `file_path`, `origin`, `license`, `provenance_path`, and `sha256` for all three icon asset ids.
- [ ] Each asset has a provenance note/source record; paid or non-redistributable binaries are not committed.
- [ ] A small preview/contact sheet is saved under `games/rb-dark-rpg/design/layout/` or `games/rb-dark-rpg/design/ui_ux/`.
- [ ] Follow-up runtime wiring is either completed or explicitly logged as a separate task if icon rendering needs engine changes.

## Open questions

- Exact final icon resolution can be 256x256 or larger, but must downscale cleanly.
- Runtime currently shows text placeholders in reward cells; this task owns art
  production and manifest readiness, not broad dialogue UI redesign.

## Log

- 2026-07-04: Created after first quest dialogue started showing immediate reward cells for sword, armour, and greaves.
