# Game Implementation Plan

Статус: source of truth v0.1.  
Дата: 2026-06-09.

Назначение: план для нового чата разработки. Этот документ отвечает на вопрос:
что именно делать, в каком порядке и где остановиться, чтобы получить первый
играбельный build `Game 67 Idle`.

## 0. Короткая Цель

Сделать первый playable slice:

```text
интро 67/67 -> предательство -> старт 1/67
главный экран -> 67-жест -> мем-коины -> первая покупка -> 2/67
одно дело с таймером -> награда -> визуальный рост героя/дома
save/load -> DevAPI screenshot/input smoke
```

Это не полный P0 на 30 минут. Это первый рабочий вертикальный срез, который
доказывает, что игра, арт, state, input и тестовый harness соединены.

## 1. Что Уже Готово

- `gamedesing/prototype_mvp_spec.md` - правила MVP.
- `gamedesing/data/balance.json` - контент, цены, эффекты, unlocks.
- `gamedesing/data/reducer_test_vectors.json` - expected reducer/action behavior.
- `gamedesing/data/ui_flow.json` - экраны, tabs, actions.
- `gamedesing/data/analytics_events.json` - analytics contract.
- `gamedesing/p0_ui_copy.md` - player-facing copy.
- `gamedesing/assets/generated/runtime_asset_manifest.json` - готовые runtime PNG.
- `gamedesing/art_bible.html` - визуальная арт-библия.
- `src/` - текущий native/wasm Neotolis shell, state codegen, autosave, DevAPI.

## 2. Главный Source-Of-Truth Порядок

Если документы конфликтуют:

1. `data/balance.json` - числа, id, effects, unlocks.
2. `data/reducer_test_vectors.json` - поведение reducer/actions.
3. `data/ui_flow.json` - screen flow и действия.
4. `p0_ui_copy.md` - точный текст для игрока.
5. `assets/generated/runtime_asset_manifest.json` - runtime art paths.
6. `prototype_technical_blueprint.md` - архитектура.
7. `prototype_build_handoff.md` - контракты реализации.
8. `playtest_acceptance_gates.md` - приемка.

## 3. Scope Первого Playable Slice

Входит:

- стартовое состояние `1/67`, `0` мем-коинов;
- главный экран с фоном `bg_starter_room_yard`;
- герой `hero_1_67_body`;
- отдельная плашка `1/67` над героем;
- большая кнопка `button_67_gesture`;
- top resource pills;
- две cards: первое дело и первый upgrade;
- bottom tabs как визуальные кнопки, даже если часть экранов еще stub;
- action `do67`;
- action `buyFirstUpgrade`;
- action `startFirstJob`;
- action `claimFirstJob`;
- save/load/reset;
- DevAPI: прочитать state, нажать кнопку, сделать screenshot.

Не входит:

- все 30 минут прогрессии;
- все события;
- все районы;
- полноценная аналитика;
- магазин, реклама, аккаунты;
- взрослые/тяжелые темы;
- редактирование GDD из игры.

## 4. Целевой Игровой Loop Вертикального Среза

```text
игрок видит 1/67
жмет 67-жест
получает +1 мем-коин
на 5 монет покупает первый upgrade
status становится 2/67
герой меняется на cap/body variant или получает визуальный flash
открывается первое дело
игрок стартует дело
таймер идет 6 секунд
игрок забирает награду
coins/status/visual stage сохраняются
```

## 5. Файлы, Которые Новый Чат Должен Читать Первыми

Обязательный минимум:

- `AGENTS.md`
- `CMakeLists.txt`
- `src/main.c`
- `src/game_state_actions.c`
- `src/game_state_actions.h`
- `state/game_state.schema.json`
- `gamedesing/game_implementation_plan.md`
- `gamedesing/data/balance.json`
- `gamedesing/data/reducer_test_vectors.json`
- `gamedesing/assets/generated/runtime_asset_manifest.json`

Потом:

- `gamedesing/prototype_technical_blueprint.md`
- `gamedesing/prototype_build_handoff.md`
- `tools/devapi/devapi_client.py`
- `tools/devapi/smoke_test.py`

## 6. Implementation Phases

### Phase 1. State Schema

Заменить/расширить smoke-test fields в `state/game_state.schema.json`.

Нужные поля:

- `meme_coins`;
- `status`;
- `click_power`;
- `income_per_second`;
- `hands_skill`;
- `coolness`;
- `visual_stage`;
- `owned_upgrade_ids`;
- `active_job_id`;
- `active_job_started_ms`;
- `active_job_duration_ms`;
- `seen_intro`;
- `final_ready`;

Acceptance:

- codegen проходит;
- save/load сохраняет новые поля;
- старые smoke-test поля можно оставить временно, если это дешевле и не мешает.

### Phase 2. Reducer Actions

Добавить в `src/game_state_actions.*`:

- `game_state_action_do67`;
- `game_state_action_buy_upgrade`;
- `game_state_action_start_job`;
- `game_state_action_claim_job`;
- `game_state_action_tick`;
- `game_state_action_reset_playtest`.

Acceptance:

- действия валидируют state;
- покупки не уходят в минус;
- статус capped на `15`;
- dirty flag ставится после изменений;
- reducer vectors из `data/reducer_test_vectors.json` либо проходят, либо есть
  отдельный documented gap для следующей фазы.

### Phase 3. Runtime Main Screen

В `src/main.c` заменить 3D smoke scene как primary surface на портретный 2D
игровой экран.

Минимум:

- фон из generated assets;
- герой;
- `1/67` badge;
- coin pill;
- tap pill;
- большая кнопка `67`;
- job card;
- upgrade card;
- bottom tabs;
- click/touch hit zones.

Acceptance:

- первый клик очевиден без чтения;
- input работает мышью на PC;
- DevAPI видит UI nodes с ids:
  - `main.do67`;
  - `main.upgrade.first`;
  - `main.job.first`;
  - `main.claim`;
  - `main.reset`.

### Phase 4. Art Integration

Использовать `gamedesing/assets/generated/runtime_asset_manifest.json`.

Первый экран:

- background: `bg_starter_room_yard`;
- hero: `hero_1_67_body`;
- button: `button_67_gesture`;
- badge: `badge_power_1_67`;
- job card: `card_job_kiosk`;
- upgrade card: `card_upgrade_tap`.

Если Neotolis pack builder пока не готов к этим PNG:

- сначала подключить минимальный runtime asset loading path;
- или временно собрать pack target только для этих PNG;
- не рисовать placeholder shapes вместо ассетов, кроме аварийного fallback.

Acceptance:

- screenshot native/PC визуально похож на `assets/generated/runtime_composed_screen.png`;
- нет magenta/chroma-key фона;
- UI не выходит за портретный safe area.

### Phase 5. Save, Reset, Offline Stub

Сохранить:

- coins;
- status;
- owned upgrades;
- active job;
- visual stage;
- seen intro.

Reset:

- возвращает `1/67`;
- очищает coins/upgrades/job;
- нужен для playtest и DevAPI.

Offline:

- в первом slice можно сделать stub: вычислять только доход за время отсутствия,
  без completion final.

### Phase 6. DevAPI Smoke

Минимальный бот-сценарий:

1. запустить native debug build with `--devapi 9123`;
2. снять initial screenshot;
3. нажать `main.do67` 5 раз;
4. купить первый upgrade;
5. проверить `status >= 2`;
6. стартовать job;
7. дождаться timer;
8. claim reward;
9. save/load roundtrip;
10. снять final screenshot.

Файлы evidence писать в `tmp/`, не в GDD:

- `tmp/runtime_first_slice_initial.png`;
- `tmp/runtime_first_slice_after_upgrade.png`;
- `tmp/runtime_first_slice_after_job.png`;
- `tmp/runtime_first_slice_smoke.json`.

## 7. Build And Test Commands

Native configure:

```powershell
cmake --preset native-debug
```

Build:

```powershell
cmake --build --preset native-debug --target game_67_idle
```

Pack:

```powershell
cmake --build --preset native-debug --target game_67_idle_pack
```

Run DevAPI:

```powershell
build/game_67_idle/native-debug/game_67_idle.exe --devapi 9123
```

GDD validation:

```powershell
node gamedesing/tools/validate_all.mjs
```

## 8. Done Definition For First Slice

Готово, когда:

- build запускается;
- видно игровой экран, а не smoke-test cube;
- 67-кнопка кликается;
- coins растут;
- первая покупка работает;
- `1/67 -> 2/67` видно на экране;
- первое дело стартует и claim дает reward;
- state сохраняется и загружается;
- DevAPI smoke проходит;
- есть 3 screenshots в `tmp/`;
- `node gamedesing/tools/validate_all.mjs` проходит.

## 9. Prompt Для Нового Чата

```text
Сделай реализацию по gamedesing/game_implementation_plan.md.
Не начинай с нового дизайна. Начни с первого playable slice:
state schema, reducer actions, main screen, generated runtime assets,
67-click, first upgrade, first job timer, save/reset, DevAPI smoke screenshots.
Все временные outputs клади в tmp/. Финальные изменения - в src/, state/,
tools/ и при необходимости gamedesing/validators.
```
