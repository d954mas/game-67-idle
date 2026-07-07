---
id: T0327
title: "Template: feature-based architecture (src/features, слои, per-feature state/assets)"
status: doing
project: P001
epic: E009
priority: P1
tags: [template, architecture, features]
created: 2026-07-06
updated: 2026-07-07
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

- [x] И1 (settings-эталон + README-конвенция) в templates/template: сборка
      native+web зелёная, поведение идентично (settings работает через
      game_features, sys_settings умер, папочная конвенция src/features/<id>/).
      ФАКТ 2026-07-07 (027d7c212): гейты G1-G8 зелёные — ctest 9/9, smoke_bot
      8/8 + живой смоук (settings/gear в ui.tree, master_volume в schema/get),
      wasm-release и wasm-devapi-debug линкуются, FEATURE_GAME_STATE=OFF
      линкуется под -Werror, скрин панели идентичен, grep: ноль sys_settings
      в шаблоне; спека → 2 ревью (ACCEPT-WITH-FIXES/ACCEPT) → фикс-раунд →
      исполнитель → deep-ревью ACCEPT-WITH-ADDITIONS (док-дрейф INSTALL.md/
      TEMPLATE.md закрыт в том же коммите).
- [ ] И2 items + И3 progression+resource_panel в шаблоне собираются,
      слоёвое правило видно в коде (progression включает items.h,
      обратного нет); золото/опыт живут как currency-предметы в purse;
      resource_panel не инклюдит ни items.h, ни progression.h (entries
      с геттерами от игры).
- [ ] features/README.md обновлён полями layer/provides/registers/assets_tag/
      art_needs в feature.json-контракте (декларативная арт-модель).
- [ ] Дизайн-док перенесён из tmp/ в постоянное место.

## Open questions

- Ревью 4× завершены (синтез: templates/design/reviews_synthesis_2026-07-06.md).
  Перед реализацией: (а) doc-sync по чек-листу §7 синтеза (доки
  противоречат — агенту не отдавать), (б) build-spec инкремента 1,
  (в) T0328 (state toolkit) — S2/S4 предшествуют стейт-части items.
  LEAN-порезы инкремента 1: события, level_down→set_level/reset, хвост,
  bake, Diablo-статы, сетка, широкий op-слой, shop-стаб (§3 синтеза).

РЕШЕНО лидом 2026-07-07 (старт арки):
- Предпосылки сняты: doc-sync §7 сделан (4238ab89), T0328 закрыт целиком
  (A0-A6, E1-E4). Карта разрыва: инкр.1 частично / инкр.3 полностью
  поглощены T0328 (game_features 7 фаз, settings-фрагмент, реестр
  фрагментов с on_new_game); инкр.2/4/5 были открыты.
- Shop: ВНЕ шаблона («магазин слишком специфичен для игры») —
  противоречие What/Done-when решено в пользу What, Done-when поправлен;
  слоёвость доказывает progression.
- bottom_nav-виджет ВЫРЕЗАН («просто набор кнопок»): кода не будет;
  правило навигации (фичи экспортируют open/close/is_open, состав нава =
  код игры, саморегистрации нет) → README-конвенция. Форма нава — при
  первой реальной игре.
- Ассет-шов <id>_pack_assets УМЕР, заменён декларативной моделью: фичи
  никогда не пишут в паки; feature.json art_needs {slot, kind, hint} +
  README-рецепт; example-ассеты в библиотечной копии
  features/<id>/assets/ (с provenance); конкретные хендлы фича получает
  от игры как конфиг; graceful-фолбэк без арта; build_packs.c остаётся
  100% игровым. Кастомизация тремя ступенями: конфиг → правка копии
  (поведение) → промоут обобщаемого в features/.
- HUD-бейдж ЗАМЕНЁН generic-фичей resource_panel (золото/опыт/премиум-
  валюта/…): entries {id, icon-хендл, label, getter} от игры, две
  визформы (счётчик + прогресс-бар), displayed≠logical, count-up
  (ease-out, ретаргет, снап на больших дельтах), акцент цвет/punch,
  poll+diff внутри виджета (коалесит идл-спам, не занимает event-лог),
  стейт только транзиентный, int64-abbrev форматтер общим хелпером.
  Демо шаблона: золото (счётчик) + xp-бар с уровнем. Floater ±N и полёт
  монет — ОТЛОЖЕНЫ в будущую отдельную фичу coin_fx; звук не нужен
  (решение лида). Шов под полёт закладывается сразу и стоит один
  аксессор: resource_panel_anchor(entry_id) в публичном API. Ресёрч:
  индустрия (displayed/logical, коалесинг, K/M/B) + анти-уроки
  rb-dark HUD (хардкод порогов уровней, reach в god-struct, int32 без
  анимации; забрать хорошее: shadowed_label, бары track+fill).
- Порядок остатка: И1 settings-эталон (папка src/features/settings/,
  UI из sys_settings въезжает, main.c чистится от прямых sys_*-вызовов)
  + README-конвенция + поля feature.json-контракта → И2 items →
  И3 progression + resource_panel. Каждый инкремент через полный процесс
  (спека → 2 ревью → исполнитель → deep-ревью → контрольный прогон →
  коммит).
- И2 (2026-07-07): лид ратифицировал OQ1 — `content/` = стандартная
  папка контент-данных И в шаблоне, И в играх (items.json туда;
  компайл-тайм embed, не рантайм-парс). Дефолты (порядок
  settings→items→game; CLI на Python; карантин = флаг на записи) —
  без возражений лида.
- И2 (2026-07-07, ПЕРЕ-РЕШЕНИЕ OQ4): «игра без стейта невозможна» —
  ось FEATURE_GAME_STATE УБИТА ЦЕЛИКОМ. Стейт-система всегда включена:
  CMake-опция и все #if/#else в шаблоне удаляются (settings теряет
  вторую версию, main.c-гейты умирают), items и все будущие фичи
  пишутся в ОДНОЙ версии, гейт «собирается без стейта» отменён.
  Снимает H2 дизайн-ревью и H1 код-ревью И2-спеки по построению.
- Карантин (лид 2026-07-07): карантинная запись ЗАНИМАЕТ слот
  вместимости контейнера (переполнения при оживлении не бывает по
  построению); намеренное удаление отгруженного def = шаг миграции
  (удалить / конвертировать в компенсацию); визуал для будущих
  инвентарных UI — серая заглушка «?» (финальное решение при первой
  игре с инвентарём).
- Деструктивные правки каталога (лид 2026-07-07): удаление/
  переименование отгруженного def ПРИНУДИТЕЛЬНО требует реакции —
  items.lock.json v2 с секцией removed {def_id: {fragment_version}};
  validate требует квитанцию + подъём версии фрагмента, а генератор
  (version == миграции+1) физически требует шаг миграции (настоящий
  или явно-пустой = осознанный карантин). Батч: N удалений = одна
  версия = один шаг. note опциональна. Забыл любой из шагов — красный
  validate или красная сборка с точным указанием, чего не хватает.

## Log

- 2026-07-06: создана по решению лида после двух-угольного Opus-дизайна
  (bottom-up от кода rb-dark-rpg + top-down от целей студии) и правок лида
  (нав = задача игры; стейт = map по фичам). Контекст: коллизии параллельных
  агентов на общих файлах в VibeJam (game_actions.c 1769 строк, bottom_nav
  switch), пункт 4 плана ретро закрыт архитектурой вместо процессных правил.
- 2026-07-07: старт арки, status=doing. Карта разрыва снята (Explore),
  решения лида зафиксированы (см. Open questions: shop вне шаблона,
  bottom_nav вырезан, декларативная арт-модель, resource_panel вместо
  HUD-бейджа). Ресёрч плашки ресурсов выполнен (Opus: индустрия +
  rb-dark). Спека И1 запущена.
- 2026-07-07: И2 ГОТОВ ЦЕЛИКОМ, четырьмя коммитами: И2-0 759326967
  (ось FEATURE_GAME_STATE убита, байт-идентичность SHA256), И2a
  8df1c80bb (content/items.json + первый контент-codegen + скелет
  фрагмента, ctest 11/11), И2b f0381b038 (владение/purse/Р9-хуки/
  карантин; deep-ревью поймал 5 data-loss багов ДО коммита — seq-reseed
  уников, int64-overflow, self-move, clamp-conservation, capacity
  уник-move; 26 Unity-тестов), И2c 91b6eefc5 (CLI items_ops list/
  validate/schema --json + lock v2 c деструктивным гардом + README
  фичи + скилл nt-game-items). Каждый срез: исполнитель → deep-ревью →
  фикс-раунд → контрольный прогон. ОСТАЛОСЬ: И3 progression +
  resource_panel (спека дальше).
- 2026-07-07: И1 ГОТОВ (027d7c212). settings = эталонная фича-папка,
  main.c чист от прямых UI-вызовов, game_features_draw_ui владеет
  ui_runtime-кадром, README-конвенция отгружена (слои с persistence
  toolkit=L0, навигационное правило, арт-модель art_needs, лестница
  кастомизации), контракт feature.json в корневом features/README
  (layer/provides/registers/assets_tag/art_needs). Полный процесс:
  спека → 2 Opus-ревью → фикс-раунд (11 пунктов) → исполнитель (Sonnet)
  → deep-ревью → контрольный прогон. ДАЛЬШЕ: И2 items (спека).
