---
id: T0296
title: "rb-dark-rpg jam: чекпоинт-коммит и решение по сабмодулю движка"
status: done
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, tech]
created: 2026-07-05
updated: 2026-07-05
---

## What

Дерево из ~55 файлов не закоммичено поверх коммита "wip", сабмодуль движка
грязный (nt_ui_modal, nt_ui_popup правлены локально). Перед 24-часовым кранчем
нужна точка отката. Фаза 0 (час 0-1).

## Done when

- [ ] Все текущие изменения игры закоммичены чекпоинтом (или явно отброшены решением лида).
- [ ] Правки в external/neotolis-engine закоммичены внутри сабмодуля или откачены на public API.
- [ ] `git status` чистый или содержит только осознанный WIP.

## Open questions

## Log
- 2026-07-05: Чекпоинт 84ae6a38 (P0 sprite packet + fix stale world_story asserts); сабмодуль движка оказался уже чистым; git status чист
