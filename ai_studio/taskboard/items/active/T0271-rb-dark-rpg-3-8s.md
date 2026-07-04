---
id: T0271
title: "rb-dark-rpg 3-8s: первый клик по стражу и старт квеста Допуск за ворота"
status: backlog
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, onboarding, quest, uiux]
created: 2026-07-04
updated: 2026-07-04
---

## What

Реализовать срез 3-8 секунд после старта: игрок кликает по `gate_guard`, видит
квестовую NPC-панель стража, получает первый gate quest и видит стартовые
предметы, которые выдает сам страж.

Это не optional quest modal с `Принять / Отклонить`. `Допуск за ворота` является
обязательным стартовым поручением. Панель должна ощущаться как квест/диалог в
духе старой браузерной RPG, но с современной плотностью: минимум текста,
богатое выделение важных слов, показ предметов, одна кнопка действия.

Player flow:

```text
objective: Поговорить со стражем
player clicks gate_guard
-> guard quest panel opens over the hub
-> quest gate_permit enters started state
-> panel shows starter item cards
-> objective changes to Взять снаряжение у стража
-> optional reply buttons appear above the primary action
-> bottom primary action is Взять снаряжение
```

Панель стража:

```text
Страж у ворот
Квест: Допуск за ворота

Дракон не вернулся. Дороги закрыты.
Возьми снаряжение. Потом - проверка у ворот.

Вы получите:
[Старый меч]  Урон +2
[Кожаная куртка]  Броня +1

[Что значит - не вернулся?]
[Что за проверка?]

[Взять снаряжение]
```

Choice layout rule:

```text
scrollable content area:
  NPC label / quest label
  NPC text / quest text
  item preview / rewards

sticky answer bar:
  optional reply buttons
  fixed bottom primary action
```

The dialogue uses one data contract with adaptive layout:

- phone: bottom sheet, content area scrolls, answer bar is sticky at the bottom;
- desktop: bottom or side panel, content area scrolls when needed, answer bar is
  fixed at the bottom of the panel;
- the bottom primary action is visually dominant and always advances the flow;
- optional reply buttons are secondary; they let the player talk more, but never
  hide, move, or replace the main action.

Optional replies:

```text
Что значит - не вернулся?
-> Три ночи назад он должен был пройти над Постом. Небо осталось пустым.

Что за проверка?
-> Один бой у ворот. Выживешь - получишь жетон искателя.
```

Rich text intent:

```json
{
  "text_budget": {
    "max_body_lines": 2,
    "max_body_chars": 110
  },
  "segments": [
    { "text": "Дракон не вернулся", "style": "danger_lore" },
    { "text": "Дороги закрыты", "style": "blocked" },
    { "text": "Возьми снаряжение", "style": "action" },
    { "text": "Старый меч", "style": "item_weapon" },
    { "text": "Урон +2", "style": "stat_bonus" },
    { "text": "Кожаная куртка", "style": "item_armor" },
    { "text": "Броня +1", "style": "stat_bonus" }
  ]
}
```

Interaction contract:

```json
{
  "screen_id": "last_post_hub",
  "from_state": "initial_arrival",
  "input": "click_hotspot:gate_guard",
  "quest_updates": [
    {
      "quest_id": "gate_permit",
      "state": "started",
      "step": "claim_guard_gear"
    }
  ],
  "objective": "claim_guard_gear",
  "active_hotspots": ["gate_guard"],
  "disabled_hotspots": ["gate", "contract_board"],
  "visible_ui": ["top_status_bar", "objective_line", "guard_quest_panel"],
  "dialogue_layout": {
    "component": "adaptive_dialogue_sheet",
    "content_area": "scrollable",
    "answer_bar": "sticky_bottom",
    "mobile": {
      "placement": "bottom_sheet",
      "max_height": "70vh",
      "min_button_height_px": 48
    },
    "desktop": {
      "placement": "bottom_or_side_panel",
      "hotkeys": {
        "primary": "Enter",
        "optional": ["1", "2"]
      }
    }
  },
  "panel_item_preview": ["old_sword", "leather_jacket"],
  "optional_choices": ["ask_dragon_missing", "ask_gate_test"],
  "rich_text_styles": ["danger_lore", "blocked", "action", "item_weapon", "item_armor", "stat_bonus"],
  "primary_action": "claim_starter_gear",
  "primary_action_layout": "sticky_bottom",
  "blocked_systems": ["inventory", "map", "contracts", "journal"]
}
```

UI/UX boundaries:

- Панель открывается поверх хаба; хаб остается видимым.
- Панель не full-screen cutscene и не общий quest journal.
- Нет выбора ответа и нет `Отклонить`.
- Текст короткий: максимум две body-строки до блока предметов.
- Важные слова выделяются rich text styles, а не дополнительными предложениями.
- Предметы показаны карточками/строками прямо в панели, с иконкой/типом и
  коротким stat bonus.
- Dialogue content area содержит NPC text, quest context, item preview и
  rewards; если не помещается, скроллится только она.
- Answer bar находится на отдельной подложке внизу панели и не скроллится.
- Optional reply buttons располагаются над главным действием и визуально
  вторичны.
- Главный progress action всегда находится внизу панели, выделен сильнее
  остальных кнопок и остается доступен без прохождения optional ответов.
- На телефоне панель работает как bottom sheet, кнопки выбора достаточно
  крупные для пальца.
- На ПК можно использовать ту же структуру как нижнюю или боковую панель;
  допустимы hotkeys `1`, `2`, `Enter`, если они не заменяют клики.
- User-visible text должен идти через engine text/rich text renderer, не быть
  запеченным в картинку.
- Do not unlock map, contracts, journal, shop, crafting, or character build yet.

## Done when

- [ ] Clicking `gate_guard` from `initial_arrival` opens `guard_quest_panel`
  without changing away from the hub screen.
- [ ] Panel has NPC label `Страж у ворот` and quest label `Допуск за ворота`.
- [ ] Dialogue body uses at most two short lines before item preview and
  communicates only: Dragon missing / roads closed / take gear.
- [ ] Key phrases are visually distinguished through rich text styles:
  `danger_lore`, `blocked`, `action`, `item_*`, `stat_bonus`.
- [ ] Panel shows item preview/cards for `old_sword` and `leather_jacket`.
- [ ] Optional reply buttons are available above the primary action:
  `Что значит - не вернулся?` and `Что за проверка?`.
- [ ] Optional replies are short and return the player to the same panel with
  the primary action still visible.
- [ ] Dialogue content area scrolls independently if text/rewards do not fit.
- [ ] Answer bar remains visible and fixed at the bottom while content scrolls.
- [ ] Mobile layout uses a bottom-sheet style panel with touch-sized answer
  buttons.
- [ ] Desktop layout uses the same content/action hierarchy in a bottom or side
  panel.
- [ ] `gate_permit` is created or updated to state `started` with current step
  `claim_guard_gear`.
- [ ] Objective line changes from `Поговорить со стражем` to
  `Взять снаряжение у стража`.
- [ ] Primary action is `Взять снаряжение`; no `Принять`, `Отклонить`, or
  separate gear hotspot is required.
- [ ] `Взять снаряжение` is fixed at the bottom of the panel and visually
  stronger than optional reply buttons.
- [ ] `gate` and `contract_board` remain disabled/locked.
- [ ] The player can proceed by pressing the panel action; there is no mandatory
  lore screen or tutorial modal blocking action.
- [ ] Executor records the UI component/state IDs used for the guard quest
  panel, item preview, and rich text styles.

## Open questions

- После `Взять снаряжение` предметы автоматически надеваются или следующий
  срез учит быстрый action `Надеть`?
- Нужен ли маленький quest toast `Квест начат: Допуск за ворота`, или панели и
  смены objective line достаточно?

## Log

- 2026-07-04: Создано как второй executor-ready срез first-five-minutes flow:
  первый клик по стражу, старт `gate_permit`, переход к получению стартового
  снаряжения.
- 2026-07-04: Обновлено по решению lead: как в рефе, стартовые предметы выдает
  сам страж; они показываются в квестовой NPC-панели, отдельный `gear_crate`
  hotspot больше не нужен.
- 2026-07-04: Обновлено по решению lead: игра делается для нового поколения,
  поэтому стартовая NPC-панель использует минимальный текст и rich text
  выделение важного вместо длинного диалога.
- 2026-07-04: Обновлено по решению lead: главный выбор/progress action всегда
  выделен и находится внизу; optional разговорные ответы находятся выше и
  остаются выбором игрока.
- 2026-07-04: Зафиксировано для executor: структура близкая к KOTOR, но
  адаптивная для ПК и телефона; dialogue content scrolls, answer bar sticky,
  primary action снизу.
