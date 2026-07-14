---
id: E009
title: Game template + structure for future games
status: active
project: P001
priority: P1
tags: [template]
created: 2026-06-24
updated: 2026-07-14
---

## Goal

Шаблон, из которого новая игра рождается как композиция фич: фичевая
архитектура (src/features/, слои строго вниз, per-feature стейт/ассеты),
copy-then-own реюз через features/, и все накопленные фиксы пайплайна
(две сборки human/agent, devapi, web parity) — в шаблоне, не в играх.

## In scope

Структура templates/template, конвенции src/features/, контракт
features/README + feature.json, обучающие фичи-образцы в шаблоне.

## Out of scope

Контент конкретных игр; extraction-тулинг до первого реального промоута
фичи во вторую игру; плагин-менеджер/солвер (запрещены).

## Log

- 2026-07-14: Groomed to the two remaining template-owned outcomes: complete
  web delivery parity and idempotent per-game Canvas bootstrap. Items
  Workbench moved to E016.

- 2026-07-01: moved from the closed shared Game Projects bucket to AI Studio;
  this is reusable template/workflow planning, not a concrete game project.
- 2026-07-06: активирован; принята фичевая архитектура
  позже закреплённая в T0327.
