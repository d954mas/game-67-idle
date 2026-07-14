---
id: T0334
title: "Canvas home: game-тег + архив проектов + фильтрация списка (папки без папок)"
status: done
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-06
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=owner-game and archive backend shipped, remaining Home and new-game tail merged into T0269, full Studio CI run 29329533678 green"}]}
---

## What

Проектов на канвасе становится много — лиду нужны «папки без папок»:
аддитивные поля `game` (строка-тег) и `archived` (bool) на project.json,
фильтр-чипы по играм на главной `/canvas/`, архив скрыт в отдельной
свёрнутой секции. Без вложенных деревьев папок (решение лида 2026-07-06).
Tool parity: `project-set` (game/archived) в CLI и API одним оп-слоем.

## Done when

- [ ] project.json несёт опциональные `game`/`archived`; оп + CLI
      `project-set` + API-роут (паритет), громкая валидация типов.
- [ ] Главная: чипы-фильтры по существующим game-тегам; архивные проекты
      скрыты из основного списка, доступны в свёрнутой секции «Archive».
- [ ] Лид разложил реальные проекты по играм/архиву и подтвердил, что
      «глаза не мозолит».

## Open questions

## Log
- 2026-07-06: Решение лида (2026-07-06, с телефона): скоуп = game-тег + archived-флаг на project.json (аддитивно) + фильтр-чипы по играм на главной + скрытая секция Архив; БЕЗ вложенных папок. Строить ПОЗЖЕ, после закрытия анимационных инкрементов. Tool parity: project-set в CLI/API.
- 2026-07-07: Перенумерован из T0330 (коллизия id с параллельной сессией: их T0330 = pack-пилот, закрыт в архив). Скоуп без изменений: game-тег + archived-флаг + фильтр на главной.
- 2026-07-07: перенумерован T0333→T0334 (второй раз: гонка номеров с параллельными сессиями).
- 2026-07-14: Closure: waived; reason: grooming reconciled a stale historical checklist with the delivered or retained scope; evidence: owner-game and archive backend shipped, remaining Home and new-game tail merged into T0269
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=owner-game and archive backend shipped, remaining Home and new-game tail merged into T0269, full Studio CI run 29329533678 green
