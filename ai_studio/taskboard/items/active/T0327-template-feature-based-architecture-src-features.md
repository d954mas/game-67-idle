---
id: T0327
title: "Template: feature-based architecture (src/features, слои, per-feature state/assets)"
status: backlog
project: P001
epic: E009
priority: P1
tags: [template, architecture, features]
created: 2026-07-06
updated: 2026-07-06
---

## What

Перевести templates/template на фичевую архитектуру, принятую лидом
2026-07-06. Дизайн: templates/design/feature_architecture_2026-07-06.md (при закрытии
задачи перенести в ai_studio/game_design/knowledge_base или templates/).

Ключевые решения (лид, 2026-07-06):
- Игра = набор фич; фича = папка src/features/<id>/ (код + ассеты + стейт +
  тесты), один публичный хедер, остальное static.
- Слои строго вниз: L0 шелл → L1 foundation (items/inventory/wallet) →
  L2 surface (shop/combat/...). L2→L2 запрещено — опускаем зависимую-от вниз.
- Кадр = явный агрегатор game_features.c (список = z-order), БЕЗ hook-таблиц.
- Вызов показа экрана — задача игры; bottom_nav = фича-виджет (entries от
  игры), фичи в нав не саморегистрируются.
- Стейт: map по фичам в сейв-документе (ключ = id фичи), типизированные
  структуры в рантайме; числовые id-диапазоны отменены.
- Ассеты: в игре — общий assets/ с тегом feature=<id> в assets.jsonl; в
  библиотеке features/<id>/assets/ самодостаточно; у фичи свой атлас-блок
  через <id>_pack_assets(ctx).
- Copy-then-own; обобщаемые улучшения промоутить в features/ до закрытия игры.

Инкременты (каждый shippable):
1. src/features/ + game_features.{c,h} + settings как первая фича
   (перенос sys_settings, main.c-кадр сворачивается; поведение идентично).
2. src/features/README.md — конвенция (слои, one-public-header, фазовый
   контракт, no-World-fields, стейт/ассет-правила).
3. Per-feature стейт в сейв-документе (под-объект на фичу; save/load хуки
   руками через game_storage; генератор — только по факту боли).
4. bottom_nav как фича-виджет + ассет-шов в build_packs.c.
5. Item-система как обучающий набор (дизайн:
   templates/design/item_system_design_2026-07-06.md, прошёл адверсариальное ревью):
   ОДНА фича items = каталог (ядро+блоки equip/use/currency) + стаки int64
   + пул инстансов + контейнеры (слияние по ревью, лид подтвердил;
   инвариант «инстанс ровно в одном контейнере» в одном месте; purse =
   скрытый контейнер валют/опыта). Вместе с фичей: op-слой
   (list/validate/upsert/deprecate/icon-link/schema) + CLI + скилл
   nt-game-items (tool parity; T0316 = только веб-клиент поверх).
   Учебная L2-фича сверху: progression (лид принял 2026-07-06) — треки
   {id, currency_def, кривая, max, режим manual|auto|threshold}, опыт =
   currency-предметы в purse, трек-стейт = level; + HUD-бейдж.
   shop НЕ в шаблоне (при первой реальной игре).
   equipment (слоты-как-данные) — при первой игре с экипировкой.
6. (отложено до первого промоута во 2-ю игру) extraction-тулинг:
   schema-фрагменты в генераторе, feature.json→INSTALL, extract-feature
   для ассетов; расширение features/README чеклистом.

## Done when

- [ ] Инкременты 1-4 в templates/template, сборка native+web зелёная,
      поведение шаблона идентично (settings работает).
- [ ] Инкремент 5: items+inventory+учебный shop в шаблоне собираются,
      слоёвое правило видно в коде (shop включает items.h/inventory.h,
      обратного нет); золото/опыт живут как currency-предметы в purse.
- [ ] features/README.md обновлён полями layer/provides/registers/assets_tag
      в feature.json-контракте.
- [ ] Дизайн-док перенесён из tmp/ в постоянное место.

## Open questions

- Ревью 4× завершены (синтез: templates/design/reviews_synthesis_2026-07-06.md).
  Перед реализацией: (а) doc-sync по чек-листу §7 синтеза (доки
  противоречат — агенту не отдавать), (б) build-spec инкремента 1,
  (в) T0328 (state toolkit) — S2/S4 предшествуют стейт-части items.
  LEAN-порезы инкремента 1: события, level_down→set_level/reset, хвост,
  bake, Diablo-статы, сетка, широкий op-слой, shop-стаб (§3 синтеза).

## Log

- 2026-07-06: создана по решению лида после двух-угольного Opus-дизайна
  (bottom-up от кода rb-dark-rpg + top-down от целей студии) и правок лида
  (нав = задача игры; стейт = map по фичам). Контекст: коллизии параллельных
  агентов на общих файлах в VibeJam (game_actions.c 1769 строк, bottom_nav
  switch), пункт 4 плана ретро закрыт архитектурой вместо процессных правил.
