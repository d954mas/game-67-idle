# Developer + Designer Handoff Plan

Статус: source of truth v0.1.  
Дата: 2026-06-09.

Назначение: дать разработчику и дизайнеру один рабочий план сборки P0, чтобы
прототип не расползался на отдельные идеи. Если есть конфликт, порядок истины:
`data/balance.json` -> `data/ui_flow.json` -> `p0_ui_copy.md` ->
`ui_bible.md` -> `screen_mockups_spec.md` -> этот документ.

## Цель Работы

Собрать mobile/web P0, где игрок видит драму и путь:

```text
67/67 на вершине -> предали -> 1/67 -> город/дела/навыки/дом -> 15/67 -> мечта 67/67
```

P0 должен быть компонентным: один набор UI-компонентов используется на всех
экранах. Все растягиваемые панели, карточки, кнопки, модалки и плашки статуса
делаются через slice9 / 9-slice / nine-slice, а не отдельными уникальными PNG
под каждый размер.

## Deliverables

### Для Разработчика

- Runtime shell mobile portrait first: `360x640`, `390x844`, desktop portrait frame.
- Импорт `data/balance.json`, `data/ui_flow.json`, `data/analytics_events.json`.
- State/reducer/effects по `prototype_build_handoff.md`.
- UI component kit по `ui_bible.md` и `data/ui_components.json`.
- Экранные состояния по `screen_mockups_spec.md`.
- Save/load/offline cap 30 минут.
- Runtime evidence в `tmp/` по `data/runtime_evidence_manifest.json`.

### Для Дизайнера

- 9 мокапов экранов по `screen_mockups_spec.md`.
- UI bible по `ui_bible.md`: цвета, типографика, компоненты, states, safe copy.
- Slice9 source assets по `asset_generation_brief.md`.
- Game assets: hero stages, district thumbnails, NPC, icons, fx, UI frames.
- Export manifest: каждый финальный PNG/WebP должен иметь id из
  `data/asset_manifest.json` или новый id, добавленный туда.

## Work Order

### Phase 0 - Lock Rules

1. Прочитать `prototype_mvp_spec.md`.
2. Прочитать `data/balance.json`.
3. Прочитать `data/ui_flow.json`.
4. Прочитать `ui_bible.md`.
5. Прочитать `asset_generation_brief.md`.
6. Не добавлять новые P0-вкладки: только `Город`, `Дела`, `Улучшения`, `Дом`.

### Phase 1 - Component Kit

Разработчик:

- создать `Button`, `Panel`, `Card`, `Modal`, `BottomTabs`, `TopStatsBar`,
  `PowerBadge`, `ProgressBar`, `ResourcePill`, `ItemCard`, `DistrictCard`,
  `DealTimer`, `ChoiceButton`;
- все компоненты должны иметь states: `normal`, `pressed`, `affordable`,
  `locked`, `active`, `completed`, `new`, `disabled`;
- подключить nine-slice renderer или CSS/Canvas эквивалент для всех
  растягиваемых рамок.

Дизайнер:

- нарисовать базовые slice9 source assets для этих компонентов;
- сохранить исходники/экспорт по naming из `ui_bible.md`;
- проверить, что углы не деформируются при 160px, 280px, 340px ширины.

### Phase 2 - Screen Mockups

Дизайнер делает 9 мокапов:

1. `intro_fall_67_to_1`
2. `main_first_screen`
3. `main_first_click`
4. `main_status_up`
5. `city_map`
6. `deals_timer`
7. `upgrades_first_purchase`
8. `home_growth`
9. `event_or_mini_final`

Разработчик не импровизирует layout: экран должен совпадать с `ui_flow.json`
по required components и с `screen_mockups_spec.md` по hierarchy.

### Phase 3 - Gameplay Integration

- `Сделать 67` всегда доступно на main, даже когда идет дело.
- `X/67` всегда висит над героем и важнее коинов.
- Locked cards показывают ровно одну причину.
- При status-up меняются: badge, hero visual, reaction, next goal.
- Дом/транспорт меняют визуальный фон и не должны быть просто строками в списке.

### Phase 4 - Asset Pass

Минимальный P0 asset cut:

- hero: 6 stages;
- NPC: Банан normal/confused, Клубника secret;
- backgrounds: room start, yard, kiosk, school yard, mini business, home upgrades;
- UI: slice9 panels/buttons/cards/modals/badges;
- icons: meme coin, status, click, income, coolness, 4 skills, comfort, lock, new;
- fx: 67 gesture, badge flash, coin fly, map ping, deal complete stamp.

### Phase 5 - Verification

Команды:

```bash
node gamedesing/tools/validate_all.mjs
node gamedesing/tools/simulate_balance.mjs
node gamedesing/tools/validate_ui_flow.mjs
node gamedesing/tools/validate_assets.mjs
```

Runtime evidence сохранять только в `tmp/`:

- `tmp/build_validation_YYYYMMDD.log`
- `tmp/runtime_qa_YYYYMMDD.log`
- `tmp/viewport_evidence_YYYYMMDD.md`
- `tmp/route_30min_rehearsal_YYYYMMDD.md`

## Acceptance

Готово для внутреннего review, когда:

- все 9 экранов есть как мокапы или runtime screenshots;
- UI components переиспользуются, нет уникальных одноразовых кнопок/карточек;
- slice9 используется для всех растягиваемых UI frames;
- `balance.json` может быть отрендерен без ручных if-ов на каждый item;
- `15/67` достижимо в sim и runtime route;
- сайт GDD показывает актуальные картинки и fake shots;
- `validate_all.mjs` проходит.

Связи: `ui_bible.md`, `screen_mockups_spec.md`, `asset_generation_brief.md`,
`data/ui_components.json`, `data/asset_manifest.json`, `data/ui_flow.json`.
