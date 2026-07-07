---
id: T0331
title: "Идея: palette-match + levels нормализация пака от anchor-ассета"
status: idea
project: P001
epic: ""
priority: P3
tags: [assets, imagegen, normalization]
created: 2026-07-06
updated: 2026-07-06
---

## What

Шаг «нормализация по размеру+цвету+яркости» из доклада Declarative Art
(TiltShift), которого у нас нет: размер/quantize/denoise есть в
assets/tools/image/, но quantize только сокращает число цветов — не подгоняет
пак под общий таргет. Идея:

- расширить quantize: снап на ЗАФИКСИРОВАННУЮ палитру, семплированную из
  anchor-ассета пака (или из art_contract.palette);
- добавить exposure/levels-проход, выравнивающий яркость набора.

Это не детерминизм, а yield/консистентность выхода — пак читается как один
сет. Лид: пока не делаем, записано как идея (2026-07-06).

## Done when

- [ ]

## Open questions

- Палитра от anchor или от art_contract.palette — что первично при конфликте?

## Log

- 2026-07-06: заведено по итогам разбора доклада Declarative Art; лид одобрил
  как идею без немедленной реализации.
