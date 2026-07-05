---
id: T0293
title: "rb-dark-rpg: expand release content JSON from 30-minute balance"
status: review
project: P003
epic: E012
priority: P1
tags: [rb-dark-rpg, release, content, json]
created: 2026-07-05
updated: 2026-07-05
---

## What

Expand authored game content from the 30-minute release balance into the actual
`games/rb-dark-rpg/design/data/*.json` content files.

## Done when

- [x] `quests.json` has 10 critical release quests plus optional support quests.
- [x] `combat.json` has the 10-level curve and 10+ encounter roster.
- [x] `items.json`, `services.json`, and `asset_manifest.json` include T3-T5
  gear/icon references for the release ladder.
- [x] New enemies/items have gameplay asset refs and provenance-backed PNG
  placeholder art where final art is not ready.
- [x] Content validation and JSON parse checks pass or unrelated legacy noise is
  logged.

## Open questions

- Keep level 10 as a hard cap for the 30-minute release unless the lead asks for
  overleveling.

## Log

- 2026-07-05: Started after lead clarified that "visual" means in-game content
  visuals such as item icons and enemies, not a standalone fake-shot SVG.
- 2026-07-05: Expanded release authored data: 13 quests, 12 encounters, 37
  items, 7 locations, 30 dialogues, and 89 asset manifest entries. Added 27
  procedural PNG placeholder assets with `manifest.generated.json` and
  `provenance.md`.
- 2026-07-05: Validation evidence: `py -3.12
  games/rb-dark-rpg/tools/validate_content_compatibility.py --warnings` passed;
  `py -3.12 games/rb-dark-rpg/tools/generate_dialogue_content.py ...` generated
  a temp runtime C file successfully; `py -3.12 -m unittest
  generate_equipment_icons_test.py` passed from `games/rb-dark-rpg/tools`;
  `git diff --check` passed on touched content/taskboard files.
- 2026-07-05: Пауза: 30-минутный контракт отложен ради джем-эпика E013 (24ч); вернуться после сдачи
