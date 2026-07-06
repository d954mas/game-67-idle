# Items: фича или конфиг? + ревью rb-dark-rpg на извлечение фич (2026-07-06)

Синтез двух независимых Opus-разборов: (A) анатомия rb-dark-rpg по коду,
(B) практики индустрии (RPG Maker database, Unity SO, Godot Resources,
CDDA/DF raws, MMO item templates). Контекст: templates/design/feature_architecture_2026-07-06.md.

## 1. Вердикт (оба угла единогласно)

**Предмет — это НЕ фича и не «просто конфиг», это ОДНА СТРОКА конфига.
Фича — это item-СИСТЕМА: движок базы (реестр определений + lookup + теги +
стакинг + инстансы + сериализация + codegen JSON→C). Содержимое базы — данные
конкретной игры (design/data/items.json), НИКОГДА не шарятся.**
Фича поставляется пустой; игра наполняет свой items.json. Это ровно
copy-then-own асимметрия: библиотека = движок, игра = данные.
Прецеденты: RPG Maker (движок базы общий, строки — свои у каждого проекта),
CDDA/DF (loader общий, raws = игра). Меч-лист не шарит никто.

Третий кусок, который лид не называл, но он ключевой: **стат-СХЕМА тоже
пер-геймная** (у идл-игры dps-множители, у RPG vitality/protection — ноль
общих полей). Общий слой фичи никогда не называет ни одного стата.

## 2. Definition / Instance / Machinery (по коду rb-dark-rpg)

- DEFINITION (const, codegen из items.json): id, display_name, kind, slot,
  stackable, max_stack, price_gold, icon_asset_id + game_combat_stats_t
  (game_content.h:45-58; generate_dialogue_content.py:403-419).
- INSTANCE (персистентно, уже ЕСТЬ в игре — B-агент ошибся, A проверил схему):
  StackInstance{def_id,count}, GearInstance{def_id,durability,level,bind_state},
  bag_order (game_state.schema.json:25,44,151-165). Стаки по def_id, гир —
  уникальные копии gear_<def>_NNN (game_actions.c:794,820).
- MACHINERY (генерик): find/alloc stack+gear, grant с диспатчем
  stackable→stack/else gear, equip/unequip, bag (game_actions.c:69-139,
  524-976, 777-838, 1125-1139). Сериализация на 100% из game-state codegen.
- GAME-SPECIFIC: только стат-математика (game_combat.c:38-157, формулы
  Legend-style). Единственная точка сцепки — stats внутри item_definition.

## 3. Слои и разрез

- **items (L1, самый нижний)** = база определений + инстансы + рюкзак
  (в коде неразделимы: grant/stack/bag завязаны на def-lookup в одном файле).
  Рекомендация A (синтез принял): МЕРДЖ «инвентаря» в items; отдельная
  фича inventory не нужна. [ОТКРЫТЫЙ ВОПРОС ЛИДУ]
- **equipment (над items)**: слоты + equip/unequip + свой под-объект стейта.
- **wallet (L1, тривиальный)**: gold.
- **shop (L2)**: items + wallet; buyback — транзиент в file-static магазина,
  НЕ персистится.
- **combat (game-only)**: items(stats) + equipment; стат-математику НЕ
  канонизировать — боевые модели не обобщаются.
Стрелки строго вниз, items не зависит ни от кого (кроме game-state + движок).

API-скетч (из фактического потребления, не спекуляция):
items_find/count/at, items_with_tag, items_grant(state,def_id,count),
items_find_gear/find_stack, items_bag_contains/remove;
equipment_equip/unequip/in_slot; shop_buy/sell/sell_price.

## 4. Пер-геймное расширение статов

- СЕЙЧАС: copy-then-own типизированного stats-структа — игра правит свою
  копию свободно. Никакого генератора полей.
- АПГРЕЙД (триггер: редактор T0316 ИЛИ вторая игра с другими статами):
  per-game item_fields.schema.json → codegen типизированного структа,
  зеркало прецедента features/game-state. Схема тогда = единый источник для
  {codegen, editor UI, валидация}.
- Отвергнуто: KV-map статов в рантайме (теряет типы и дешёвый доступ,
  game_combat.c:42-47), god-stat-struct на все жанры, void*-расширения.
- Стандартизуется МЕТА-схема (как объявляется стат: name/type/default),
  не сами статы — игры меняют список полей, не грамматику.

## 5. Редактор T0316 (оба угла сошлись)

Schema-driven форма над design/data/items.json: UI генерится из той же
схемы, что и codegen (потому и «универсальный» — как инспектор Unity).
Студийный ВЕБ-инструмент рядом с canvas/gallery (tool parity: validate/write
CLI + веб как равные клиенты), НЕ внутриигровой DevAPI (в игре — const
compiled таблица, живой редактор мутировал бы не тот артефакт). Иконки —
пикер из галереи по icon_asset_id. СТРОИТЬ ПОСЛЕ того как форма фичи
переживёт 2-ю игру (классический провал: редактор раньше стабильной модели).

## 6. Анти-паттерны rb-dark-rpg — НЕ канонизировать при извлечении

1. **Арт захардкожен дважды в обход данных**: equipment_item_art()
   (equipment_screen.c:280-348) и shop_item_art() (shop_screen.c:263-309) —
   гигантские strcmp-цепочки в параллельный enum регионов, хотя
   icon_asset_id есть у каждого предмета. Добавить предмет = 2 enum + 2
   if-цепочки. Фича обязана рендерить из icon_asset_id.
2. **Слоты paperdoll полу-данные**: equipment_slots в JSON, но enum слотов в
   C (game_content.h:19-33) + исчерпывающие switch ×3 + 12 ручных полей
   equipment.<slot>_instance_id в схеме. Добавить слот = ~6 файлов.
   При извлечении: слоты из данных (codegen enum + стейт-полей) — или честно
   оставить пер-геймными. [ОТКРЫТЫЙ ВОПРОС ЛИДУ]

## 7. Кандидаты на извлечение из rb-dark-rpg (ранжировано)

Universal-now (reuse × 1/cost):
1. settings — уже пилот миграции (T0327 инкремент 1);
2. wallet — тривиален, хороший второй пилот;
3. **items — флагман L1** (расцепить статы, убить арт-цепочки);
4. equipment — вместе с items, с решением по слотам;
5. dialogue-runtime — проверить форму при извлечении;
6. bottom_nav — как виджет entries[] (уже в T0327 инкремент 4).

After-2nd-game: shop (модели экономики разные, buyback-квирк), quests/journal
(генерик-ядро, но тяжёлое сцепление), locations/world-map (node-graph
реюзабелен, requirements сцеплены), tutorial/callouts.

Game-only / не извлекать: combat (стат-математика), scene-interactions,
audio (тонкая обёртка, cue-enum пер-геймный), hud/first_screen_hud/end_screen
(композиция игры). Shell: persistence-оркестрация, theme, ui_runtime, render.

## 8. Риски «фундамента на годы» + дешёвые митигации

- Пере-обобщение под жанры, которых не будет → definition-ядро сейчас,
  instance-модель уже есть по факту, аффиксы/сокеты/дюрабилити-математика —
  только когда игра попросит.
- Editor lock-in к сырой схеме → редактор из схемы, после 2-й игры.
- Тихая дивергенция форков под copy-then-own → общая поверхность крошечная
  (id/tag/stack/value/codegen), большая пер-геймная поверхность — это ДАННЫЕ
  (вариация = контент, не форк); штамп copied_from: features/items@<commit>
  для диффа при промоуте; дисциплина «обобщается → промоутни до закрытия».

## 9. Открытые вопросы лиду

1. Мердж: items = «база + рюкзак» одной фичей (рекомендация; equipment
   отдельно) — или держать items и inventory раздельно?
2. Генератор стат-схемы: отложить до редактора/2-й игры (рекомендация,
   lean) — или строить сразу, раз редактор давно хочется?
3. Слоты paperdoll: делать data-driven при извлечении items/equipment
   (~6 файлов, самый большой shape-фикс) — или пер-геймными до 2-й игры?
