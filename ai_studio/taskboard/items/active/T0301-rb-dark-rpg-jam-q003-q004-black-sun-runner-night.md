---
id: T0301
title: "rb-dark-rpg jam: q003+q004 - развести black_sun_runner и night_attacker"
status: idea
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, content, cut-line]
created: 2026-07-05
updated: 2026-07-05
---

## What

black_sun_runner и night_attacker заавторены в combat.json, но не имеют ни
квестов, ни точек спавна — мёртвый контент. Развести их в q003/q004. Первый
кандидат на вырез при нехватке времени (cut-line). Фаза 1.

## Done when

- [ ] q003: погреб + улика + бой с black_sun_runner, награда black_sun_charm.
- [ ] q004: ночная защита свидетеля, бой с night_attacker.
- [ ] Оба квеста проходят devapi-прогоном.

## Open questions

## Log
- 2026-07-05: Аудит: q003/q004 нестартуемы (council_scribe не существует, доска контрактов no-op); вырезаны из джем-дуги в пользу финала на q005 (Option B). Вернуть после джема вместе с E012
