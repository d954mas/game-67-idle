---
id: T0270
title: "rb-dark-rpg 0-3s: стартовый экран Последний Пост и первый активный страж"
status: backlog
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, onboarding, hub, uiux]
created: 2026-07-04
updated: 2026-07-04
---

## What

Реализовать первые 0-3 секунды после старта `rb-dark-rpg`.

Игрок должен сразу попасть на экран хаба `Последний Пост`: без главного меню,
intro slide и modal tutorial. Экран фиксирует закрытые ворота, стража у ворот
как единственное активное взаимодействие и минимальную UI-обвязку с одной
текущей целью.

Важное решение: стартовое снаряжение не лежит в отдельном `gear_crate` hotspot.
Его выдает сам страж в следующем interaction state, а предметы показываются в
панели квеста/разговора со стражем.

Видимая композиция:

- Иллюстрированный 2D background: `last_post_hub_initial`.
- Главный фокус: закрытые ворота и страж у ворот.
- Вторичные видимые, но неактивные объекты: ворота и доска контрактов.
- Допустимы только фоновые силуэты; без дополнительных интерактивных NPC.
- Минимальная верхняя status bar: HP, золото, уровень/XP.
- Objective line: `Поговорить со стражем`.

Initial interaction contract:

```json
{
  "screen_id": "last_post_hub",
  "state": "initial_arrival",
  "objective": "talk_to_gate_guard",
  "active_hotspots": ["gate_guard"],
  "disabled_hotspots": ["gate", "contract_board"],
  "visible_ui": ["top_status_bar", "objective_line"],
  "blocked_systems": ["inventory", "map", "contracts", "journal"]
}
```

## Done when

- [ ] Новый run открывается напрямую на `last_post_hub` в пределах 3 секунд,
  без главного меню, intro screen или modal tutorial.
- [ ] Фон хаба ясно показывает ворота Последнего Поста, стража и вторичные
  будущие объекты, но вторичные объекты не выглядят активными.
- [ ] `gate_guard` является единственным active hotspot в initial state.
- [ ] `gate` и `contract_board` видимы, но disabled/locked в initial state
  0-3 секунд.
- [ ] Нет отдельного `gear_crate`/quartermaster hotspot в initial state.
- [ ] Видимый UI ограничен базовым статусом и одной строкой цели; map,
  inventory, contracts и journal не открыты и не продвигаются как действия.
- [ ] Hover/click feedback делает стража очевидным первым действием.
- [ ] Executor записал реализованные screen/state IDs и asset refs для фона,
  стража и hotspots.

## Open questions

- Аватар игрока виден как маленький portrait/status element уже в первом
  состоянии или появляется только на шаге экипировки?

## Log

- 2026-07-04: Создано из планирования первых 0-3 секунд first-five-minutes
  flow. Scope намеренно ограничен первым экраном, одним active NPC, minimal UI
  и blocked future systems.
- 2026-07-04: Обновлено по решению lead: стартовое снаряжение выдает сам страж,
  поэтому отдельный `gear_crate` hotspot убран из initial state.
