---
id: T0293
title: "rb-dark-rpg: expand release content JSON from 30-minute balance"
status: backlog
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

- [ ] `quests.json` has 10 critical release quests plus optional support quests.
- [ ] `combat.json` has the 10-level curve and 10+ encounter roster.
- [ ] `items.json`, `services.json`, and `asset_manifest.json` include T3-T5
  gear/icon references for the release ladder.
- [ ] New enemies/items have gameplay asset refs and provenance-backed PNG
  placeholder art where final art is not ready.
- [ ] Content validation and JSON parse checks pass or unrelated legacy noise is
  logged.

## Open questions

- Keep level 10 as a hard cap for the 30-minute release unless the lead asks for
  overleveling.

## Log

- 2026-07-05: Started after lead clarified that "visual" means in-game content
  visuals such as item icons and enemies, not a standalone fake-shot SVG.
- 2026-07-05: Пауза: 30-минутный контракт отложен ради джем-эпика E013 (24ч); вернуться после сдачи
