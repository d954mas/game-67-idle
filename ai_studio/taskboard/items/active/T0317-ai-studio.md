---
id: T0317
title: "AI Studio: арт-гейт против битых вырезок - авто-проверка альфы/фона/обрезки до попадания ассета в игру"
status: backlog
project: P001
epic: E010
priority: P1
tags: [assets, quality, art-gate, vibejam-retro]
created: 2026-07-05
updated: 2026-07-14
---

## What

VibeJam: криво вырезанный ассет с остатками зелёного фона попал в игру; зелёные
ареолы ловил только глаз лида (тред 31dc). Решение лида 2026-07-06: гейт живёт
на этапе ПОДГОТОВКИ АССЕТОВ (image tools / canvas / asset workflow), НЕ в игре —
битый ассет не должен доехать до пака вообще.

Проверки: остатки хромакея (green/magenta spill по краю альфы), ареолы/halo,
пустые поля обрезки (bbox vs canvas), рваная альфа. Точка врезки: выход
cutout/slice-операций image tools + promote в ассеты игры (nt-asset-workflow).

## Done when

- [ ] Автопроверка запускается на выходе alpha_matte/alpha_dualplate/corridorkey
      и при promote ассета в games/<id>/assets/: fail при spill/halo/bbox-браке.
- [ ] На corpus джема (location_scene_sprites_01 и соседние паки) гейт ловит
      известный битый ассет с зелёным фоном и не флажит принятые чистые.
- [ ] Отчёт гейта — одна строка verdict + миниатюра проблемной зоны (для быстрой
      проверки лидом).

## Open questions

- Порог spill/halo: фиксированный или per-style из art_contract?

## Log

- 2026-07-06: заполнен из ретро-разбора (пункт 2 плана); решение лида о
  размещении гейта на этапе подготовки ассетов зафиксировано.
