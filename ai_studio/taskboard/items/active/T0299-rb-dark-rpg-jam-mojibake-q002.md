---
id: T0299
title: "rb-dark-rpg jam: починить mojibake в q002 и имени старосты"
status: review
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, content, quick-win]
created: 2026-07-05
updated: 2026-07-05
---

## What

Двойная кодировка кириллицы: шаги q002 в quests.json (~311-312, ~348-349) и
display_name старосты в locations.json (~97) рендерятся кракозябрами прямо в
журнале первого контракта. Фаза 1, самый дешёвый крупный фикс.

## Done when

- [ ] Шаги q002 и имя старосты читаются по-русски в игре (скрин в Log).
- [ ] generate_dialogue_content + content_compatibility_check зелёные.
- [ ] Остальные design/data/*.json проверены на тот же дефект.

## Open questions

## Log
- 2026-07-05: Староста починен в чекпоинте; шаги q002 переконвертированы cp1251->utf8 скриптом, текст в JSON верифицирован; скрин в игре — при финальном прогоне
