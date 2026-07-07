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

## Done when

- [ ]

## Open questions

## Log
- 2026-07-07: Запрос лида 2026-07-07 (живой тест портфеля): (1) alpha cutout ВСЕГДА миную копию — новый элемент рядом с оригиналом (не ломать арт, легко сравнивать методы бок о бок; имя с методом, провенанс на копии, оригинал нетронут, undo убирает копию); (2) кнопка Alpha компактная: Apply + время, длинные описания методов -> tooltip. Исполнение: deep-reasoner в worktree anim-card-t0265.
- 2026-07-07: Реализовано и смерджено в master (c47278d93): alphaCutout single+batch минтят НОВЫЙ элемент рядом с исходником (16px справа, display-box twin, имя '<source> · <method>', meta.alpha.parentElementId); оригинал байт-в-байт нетронут, undo убирает только копии, ONE journal entry. Site выбирает копию после успеха. Кнопка Alpha: короткие лейблы Apply · ~15s/~1-3s/~25s, matte Apply/Apply(N), dual Generate; длинные описания в title tooltip. Сьют 724 зелёный; live matte e2e: source deep-equal unchanged, elements 1→2→1 через undo. На лиде: приёмка UI после рефреша страницы.
