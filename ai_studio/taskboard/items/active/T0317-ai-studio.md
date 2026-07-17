---
id: T0317
title: "AI Studio: арт-гейт против битых вырезок - авто-проверка альфы/фона/обрезки до попадания ассета в игру"
status: doing
project: P001
epic: E010
priority: P1
tags: [assets, quality, art-gate, vibejam-retro]
created: 2026-07-05
updated: 2026-07-17
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

- Resolved 2026-07-17: thresholds are per-style in
  `design/style_lock.json#technical_gate`; T0317 owns the metric formulas and
  corpus calibration, while the broader art contract remains taste/review
  guidance.

## Log

- 2026-07-06: заполнен из ретро-разбора (пункт 2 плана); решение лида о
  размещении гейта на этапе подготовки ассетов зафиксировано.
- 2026-07-17: Started slice 1 after T0326 increment 1: define calibrated post-cutout metric formulas and a fail-closed evaluator/CLI with synthetic clean-vs-broken corpus and problem thumbnail. This slice will add threshold fields to style_lock only after formulas exist; alpha/canvas/promote wiring remains a later slice. T0258 is a separate weak-alpha product-choice optimization and does not block this gate.
- 2026-07-17: Slice 1 complete: added deterministic post-cutout spill, halo, alpha-noise, empty-margin, and aspect formulas; fail-closed CLI/reporting; stale-thumbnail cleanup on every invocation; synthetic clean/broken regression corpus; and per-style technical_gate thresholds. Independent review clean. Focused suites, changed verify, architecture validation, Taskboard/doc-reference checks, diff check, and full 10-domain verify pass. Canvas alpha-output and asset-promote wiring plus real jam-corpus calibration remain for later slices.
- 2026-07-17: Slice 2 adds the trusted Canvas checked-transition seam: `asset-status-check` resolves the accepted game style lock, maps its thresholds, runs the real evaluator outside the project lock, then stores frozen evidence and PASS→checked / FAIL→quarantine in one conflict-safe journal step. CLI/API bodies cannot supply verdict evidence. A real adapter test also closed a warm-worker PYTHONPATH parity gap. Automatic alpha-output invocation, promote enforcement, and jam-corpus calibration remain open, so no Done-when checkbox closes yet.
