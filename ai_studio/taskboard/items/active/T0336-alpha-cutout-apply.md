---
id: T0336
title: Alpha cutout всегда в копию + компактная кнопка Apply
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-07
updated: 2026-07-07
---

## What

Alpha cutout operations should always create a new sibling copy beside the source asset instead of mutating the original. The Alpha UI should expose compact Apply controls with timing hints, while longer method descriptions move into tooltips.

## Done when

- [x] Single and batch alpha cutout create a new element beside the source asset.
- [x] The original asset stays byte-for-byte unchanged.
- [x] The copy records alpha provenance, source parent id, and method naming.
- [x] Undo removes the generated copy without touching the source.
- [x] Alpha buttons use compact Apply labels with timing hints and long descriptions in tooltips.
- [x] Test and live-smoke evidence is recorded in the task log.
- [ ] Lead accepts the refreshed UI in the main checkout.

## Open questions

## Log
- 2026-07-07: Запрос лида 2026-07-07 (живой тест портфеля): (1) alpha cutout ВСЕГДА миную копию — новый элемент рядом с оригиналом (не ломать арт, легко сравнивать методы бок о бок; имя с методом, провенанс на копии, оригинал нетронут, undo убирает копию); (2) кнопка Alpha компактная: Apply + время, длинные описания методов -> tooltip. Исполнение: deep-reasoner в worktree anim-card-t0265.
- 2026-07-07: Реализовано и смерджено в master (c47278d93): alphaCutout single+batch минтят НОВЫЙ элемент рядом с исходником (16px справа, display-box twin, имя '<source> · <method>', meta.alpha.parentElementId); оригинал байт-в-байт нетронут, undo убирает только копии, ONE journal entry. Site выбирает копию после успеха. Кнопка Alpha: короткие лейблы Apply · ~15s/~1-3s/~25s, matte Apply/Apply(N), dual Generate; длинные описания в title tooltip. Сьют 724 зелёный; live matte e2e: source deep-equal unchanged, elements 1→2→1 через undo. На лиде: приёмка UI после рефреша страницы.
