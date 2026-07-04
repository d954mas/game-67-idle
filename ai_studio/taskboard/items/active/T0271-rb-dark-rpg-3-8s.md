---
id: T0271
title: "rb-dark-rpg 3-8s: диалог со стражем и старт квеста Допуск за ворота"
status: review
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, onboarding, dialogue, quest, uiux]
created: 2026-07-04
updated: 2026-07-04
---

## What

Реализовать следующий срез после первых 0-3 секунд: игрок кликает по
`gate_guard`, открывает короткую диалоговую панель стража и через нее запускает
первый обязательный квест `q001_gate_pass` (`Допуск за ворота`).

Это не отдельный modal `Принять / Отклонить`. Квест уже доступен на новом run,
а диалог со стражем является первым действием, которое завершает шаг
`talk_gate_guard` и переводит игрока к следующей цели: забрать снаряжение у
кузнеца.

Source data:

- character: `gate_guard` from `characters.json`;
- dialogue: `dlg_gate_guard_intro` from `dialogues.json`;
- quest: `q001_gate_pass` from `quests.json`;
- completed step after dialogue: `talk_gate_guard`;
- next step/objective: `receive_starter_gear`;
- next marker/unlock: `blacksmith`;
- starter items are not granted in this task; they come from
  `dlg_blacksmith_starter_gear` / `receive_starter_gear`.

Player flow:

```text
state: initial_arrival
objective: Поговорить со стражем
player clicks gate_guard
-> dialogue panel opens over the hub
-> node start shows guard warning
-> choice Что нужно сделать? opens node explain_check
-> primary/progress choice Понял. advances q001_gate_pass/talk_gate_guard
-> objective becomes Забрать снаряжение у кузнеца
-> blacksmith marker becomes visible/active
-> hub remains visible behind the panel
```

Dialogue content:

```text
Страж у ворот
Квест: Допуск за ворота

За ворота без жетона и оружия больше не выпускаю.
Хочешь контракт - докажи, что вернешься живым.

[Что нужно сделать?]

Кузнец выдаст старый меч и куртку.
Надень их, убери падальщика у ворот, потом вернешься ко мне.

[Понял.]
```

UI contract:

- panel opens over the hub; it is not a full-screen cutscene;
- phone: bottom sheet, max height around 70vh, content scrolls if needed;
- desktop: bottom or side dialogue panel using the same hierarchy;
- speaker name, quest name, current objective, and choices are visible;
- optional/branch choices are above the main progress action;
- the main progress action is visually stronger and sticky at the bottom;
- no full quest journal, map, contracts, inventory, shop, build screen, or
  tutorial overlay is unlocked in this task;
- user-visible text must use the engine text/rich text renderer, not baked text.

State contract:

```json
{
  "screen_id": "last_post_hub",
  "from_state": "initial_arrival",
  "input": "click_hotspot:gate_guard",
  "dialogue_id": "dlg_gate_guard_intro",
  "dialogue_node": "start",
  "quest_id": "q001_gate_pass",
  "quest_before": {
    "status": "available",
    "current_step_id": null
  },
  "quest_after_primary_action": {
    "status": "active",
    "completed_step_ids": ["talk_gate_guard"],
    "current_step_id": "receive_starter_gear",
    "flags": ["gate_guard_intro_seen"]
  },
  "objective_after": "receive_starter_gear",
  "active_hotspots": ["gate_guard", "blacksmith"],
  "disabled_hotspots": ["gate", "contract_board"]
}
```

## Out of scope

- Granting `old_sword` and `padded_jacket`.
- Equipment screen, equip actions, first combat, gate unlock, map unlock, journal
  screen, contract board.
- Rewriting quest/dialogue authoring data unless a missing field blocks loading
  the current configs.

## Done when

- [ ] Clicking `gate_guard` in `initial_arrival` opens a dialogue/quest panel
  without leaving `last_post_hub`.
- [ ] The panel loads data from `dlg_gate_guard_intro`, not hard-coded text.
- [ ] The first node shows speaker `Страж у ворот` and quest label
  `Допуск за ворота`.
- [ ] Choice `Что нужно сделать?` advances to node `explain_check`.
- [ ] Choice/action `Понял.` applies effect `advance_quest` for
  `q001_gate_pass/talk_gate_guard`.
- [ ] `q001_gate_pass` becomes active after the progress action.
- [ ] Step `talk_gate_guard` is recorded completed.
- [ ] Current step/objective becomes `receive_starter_gear`.
- [ ] Objective line changes to `Забрать снаряжение у кузнеца`.
- [ ] `blacksmith` becomes visible or marked as the next active target.
- [ ] `gate` and `contract_board` remain disabled/locked.
- [ ] No starter items are granted yet.
- [ ] Mobile and desktop layouts keep the main progress action visible at the
  bottom of the panel.
- [ ] Executor records the UI component/state ids used for the dialogue panel,
  choice buttons, quest update, and objective line.

## Open questions

- Нужен ли маленький toast `Квест начат: Допуск за ворота`, или достаточно
  смены objective line и маркера кузнеца?
- После `Понял.` панель закрывается сразу или остается открытой с коротким
  подтверждением `Новая цель: кузнец`?

## Log

- 2026-07-04: Created as the second first-five-minutes task after the 0-3s hub
  slice.
- 2026-07-04: Refined to match current authored data ids:
  `q001_gate_pass`, `dlg_gate_guard_intro`, `talk_gate_guard`,
  `receive_starter_gear`, `padded_jacket`; removed stale `gate_permit` and
  `leather_jacket` references.
- 2026-07-04: Executor started implementation after T1 state slice; aligning guard dialogue action with task contract: advance q001 to receive_starter_gear, no starter item grants, blacksmith next target.
- 2026-07-04: Implemented executor slice: dialogue definitions are generated from design/data JSON at build time; gate guard accept advances q001_gate_pass/talk_gate_guard to receive_starter_gear; no starter gear is granted at guard; blacksmith hotspot unlocks as next objective. Verification: game_dialogue_test, scene_interactions_test, quality_responsive passed. Evidence summary: tmp/quality/qclr_002_responsive/summary.json.
- 2026-07-04: Lead correction: no blacksmith in this slice. Guard gives starter gear and explains the world; next objective returns to equipping granted gear. Reworking previous blacksmith objective/hotspot changes.
- 2026-07-04: Lead correction applied: guard dialogue now says the guard gives the starter gear and explains the world; no blacksmith objective/hotspot/dialogue in this slice. q001 advances to equip_old_sword after talk_gate_guard. Verification passed: game_dialogue_test, scene_interactions_test, quality_responsive. Evidence: tmp/quality/qclr_002_responsive/summary.json.
- 2026-07-04: Data-driven reward/effect follow-up: guard accept choices now carry authored reward_id in dialogues.json; runtime choice effects and reward claim id are generated from dialogue data instead of C hardcode. Compatibility validator now reads authored choice.reward_id. Verification passed: game_dialogue_test, scene_interactions_test, quality_responsive; git diff --check has only CRLF normalization warnings.
