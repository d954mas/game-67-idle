# BUILD-SPEC: мультифрагмент — второй живой фрагмент `settings`, инкремент A6 (2026-07-07)

Имплементационная спецификация. Переводит принятый дизайн в файлы, сигнатуры и
критерии приёмки. НЕ меняет дизайн, НИЧЕГО не реализует — только спека.

Источник истины (при расхождении — новее побеждает, §14 главнее основного текста):
1. `features/game-state/references/state_system_design_2026-07-06.md`
   (§14 п.2 «game последним», §14 п.12 clean-break шаблона, §14 п.16 orphan/NEWER,
   §6 оркестрация, §5/§7 схема+генератор, §13 Р9 «стейт-слой генерится, логика руками»).
2. `features/game-state/references/build_spec_a5_2026-07-07.md` (стиль, FROZEN-модель,
   универсальный реестр-диспатч `game_save_devapi.c`, форма ответов get/schema/patch).

Исполнитель может писать код по этому документу, НЕ открывая историю обсуждений.

---

## 0. Тезис A6 и рамки (проверено по дереву 2026-07-07)

**Тезис (что доказывает A6):** реестр (`game_save.c`) и диспатч (`game_save_devapi.c`)
после A5 УЖЕ универсальны над `GameSaveFragment`. Значит второй живой фрагмент
`settings` обязан «просто появиться» в `get ""` / `schema` / `patch` / `save`/`load`
**без единой правки диспатча, шелла и генератора**. A6 = добавить фрагмент + доказать
это гейтом. Всё «умное» (механизм) уже построено; A6 его лишь ВКЛЮЧАЕТ вторым
экземпляром и переносит `settings` из монолитного `game`-фрагмента в свой.

**Проверенные факты, снимающие мнимую работу:**

- **Генератор править НЕ нужно.** `generate_state.py` уже пер-фрагментный и полностью
  параметризован по `NS = Ns(fragment)` (generate_state.py:111-128, 413-414). Всё, что
  требует `settings`, поддержано СЕГОДНЯ:
  - `float`-поля — да (render_* ветки `float`).
  - ПЛОСКИЕ пути полей без точки (`master_volume`) — да: `parent_var_for` (1116-1120)
    кладёт их в корень payload'а фрагмента.
  - ПУСТАЯ секция `events` — да: `load_events(raw.get("events", {}))` (396) допускает
    отсутствие; `render_events_source` (1736-1743) эмитит валидный стаб
    (`<id>_ev_descs[1]={NULL}`, `<id>_ev_desc_count=0`, пустой `<id>_ev_register`).
  - Отсутствие `enums`/`types`/`hooks`/`migrations` — да: все дефолтятся в `load_schema`
    (342/349/370/379), дескриптор эмитит `.steps=NULL, .on_new_game=NULL, .reconcile=NULL`.
  ⇒ Секция `events` пустая — генератор её ДОПУСКАЕТ; фикс генератора со своим тестом
  НЕ требуется (задача-ветка «если запрещает» — не наступает).
- **Диспатч `game_save_devapi.c` править НЕ нужно.** `build_aggregate` (devapi:57-72),
  `ep_state_schema` (93-109), `route_path` (42-53), `ep_state_patch` (186-241) итерируют
  реестр через `game_save_fragment_count/at/find` — фрагмент-агностично. Второй фрагмент
  входит в `get ""`, `schema`, кросс-фрагментный `patch`, `set`/`get` по голове пути
  автоматически.
- **Шелл `game_save.{c,h}` править НЕ нужно.** Реестр (`s_fragments[]`), fan-out
  (`reset_all`/`on_new_game_all`/`reconcile_all` :262-284), сериализация конверта
  (`build_root` :288-329) и load-FSM (:413-550) уже цикл по всем фрагментам. Порядок
  сериализации = **порядок регистрации** (build_root бежит `s_fragments[0..count]`, orphans
  ПОСЛЕ) — детерминирован, НЕ сортируется. См. §A6.5.
- **Шелл-мультифрагмент уже покрыт ctest.** `tests/test_game_save.c` регистрирует ДВА
  фрагмента (`s_extra_fragment` + `s_fake_fragment`, :555-556, «game последним»),
  проверяет пофрагментную изоляцию (`test_bad_fragment_isolation` :536), orphan-round-trip
  (:404). Файл A6 НЕ трогает (FROZEN) — механизм на уровне шелла доказан на моках; A6
  доказывает его на ЖИВОМ генерённом фрагменте + через DevAPI.

### Что A6 делает (ровно)

1. Пишет `templates/template/state/settings.schema.json` (v2, fragment `settings`,
   3 float-поля громкости, без events/enums/types/hooks/migrations) (§A6.3).
2. Пишет РУЧНУЮ ЛОГИКУ Р9 поверх генерённого стейт-слоя:
   `templates/template/src/features/settings.{c,h}` — тонкий feature-API
   (getters/setters + clamp + `game_save_mark_dirty`) над генерённым `settings_state`
   (§A6.4). Стейт-слой (`SettingsState`/дефолты/(де)сериализация/схема/дескриптор) —
   ГЕНЕРИТСЯ (Р9: руками только логика).
3. Выносит `settings.*` из `game`-фрагмента: снимает `settings.master_volume`/
   `settings.sfx_volume` из `game_state.schema.json` (clean-break, §A6.6); РЕ-ЗАХВАТ
   golden `game` (game_state.{h,c,schema.gen.h} теряют settings-строки — §A6.7).
4. Регистрирует `settings` ПЕРЕД `game` (main.c) — сохраняя инвариант «game последним»
   (§A6.8).
5. CMake: ВТОРОЙ generation-блок для settings-схемы + компиляция `settings_state.c` и
   `src/features/settings.c` (§A6.8).
6. Провод UI: `sys_settings.c` пишет/читает громкости через feature-API (персистентность
   ползунков; было — оторванные локальные статики) (§A6.4).
7. Смоук-бот: `value.game.settings` → `value.settings`; +фрагмент `settings` в
   `schema`-агрегате; синхронно `smoke_bot_test.py` (§A6.9).
8. Доки: `feature.json`, `INSTALL.md` (§A6.10).

**НЕ входит (границы жёсткие):**
- `items`/`progression` фрагменты — контент T0327, лид добро не дал. A6 = механизм +
  settings.
- Правки генератора, шелла (`game_save.{c,h}`), диспатча (`game_save_devapi.c`),
  `game_storage.*`, `game_state_json.*` — НЕ трогаются (тезис A6).
- Миграционный шаг / doc-шаг для выноса settings — НЕ строится (clean-break + правило
  «новая фича ≠ миграция» покрывают, §A6.6 FSM). Монолитный миграционный слой НЕ
  изобретать.
- Третья golden-fixture (`golden/settings/`) — НЕ добавляется (game+mini покрывают
  генератор; settings не приносит нового рендер-пути, §A6.7).
- `game_audio.c` — НЕ компилируется в шаблоне (нет в `add_executable`, ноль вызовов
  `game_audio_set_volume`; rb-dark-хоррор-остаток). Апплаер к нему НЕ подключать.
- События settings — их нет (пустая секция); `settings_ev_register` НЕ зовётся,
  `settings_state_events.gen.c` в таргет НЕ линкуется (§A6.8).
- **Скилл `nt-game-state` v2 (4 хода: поле/фрагмент/шаг миграции/doc-шаг).** Дизайн §12
  упоминал его в составе A6; ОСОЗНАННО ОТЛОЖЕН из этой спеки — скилл пишется после
  стабилизации A6/T0327 (когда items/progression доказывают миграционные/hook-пути живьём,
  MED-2). Без правок таскборда здесь.

### FROZEN в A6 (не менять)

- `templates/template/src/game_save.{c,h}` — целиком (реестр/оркестрация/read-API).
- `templates/template/src/game_save_devapi.c` — целиком (универсальный диспатч; вся суть
  A6 в том, что его трогать НЕ надо).
- `templates/template/src/game_storage.{c,h}`, `game_state_json.{c,h}` — целиком.
- `features/game-state/scripts/generate_state.py` — целиком (пустые events уже ок).
- `templates/template/tests/test_game_save.c` — целиком (шелл-мультифрагмент доказан).
- Генерённый/golden `game`: `game_state.c`/`_schema.gen.h` меняются ТОЛЬКО на снятие
  settings-строк (ре-захват, §A6.7); `game_state_events.gen.{h,c}` — БАЙТ-В-БАЙТ (секция
  events `game` не тронута).
- `games/rb-dark-rpg/**`, движок.

---

## A6.1 Файлы

**Новые:**
- `templates/template/state/settings.schema.json` — схема фрагмента settings (§A6.3).
- `templates/template/src/features/settings.h` — feature-API (декларации) (§A6.4).
- `templates/template/src/features/settings.c` — ручная логика Р9 (§A6.4). Компилируется
  ТОЛЬКО под `FEATURE_GAME_STATE` (инклюдит генерённый `settings_state.h`).

**Генерённые (custom command; в git не коммитятся — build/):**
- `settings_state.h`, `settings_state.c`, `settings_state_schema.gen.h`,
  `settings_state_events.gen.h`, `settings_state_events.gen.c` (последние два — стаб,
  в таргет не линкуются).

**Изменяемые:**
- `templates/template/state/game_state.schema.json` — снять `settings.master_volume`
  (:33) и `settings.sfx_volume` (:34) (§A6.6).
- `templates/template/CMakeLists.txt` — 2-й generation-блок + target_sources
  `settings_state.c` + `src/features/settings.c` (§A6.8).
- `templates/template/src/main.c` — `#include "settings_state.h"` (рядом с :53);
  регистрация `&settings_state_fragment` ПЕРЕД `&game_state_fragment` (:374);
  `settings_state_fragment.reset()` в ветке `--fresh-state` (:390) (§A6.8).
- `templates/template/src/systems/sys_settings.c` — ползунки через feature-API
  (§A6.4).
- `templates/template/src/systems/sys_settings.h` — getters форвардят в feature-API (или
  снять — не используются; §A6.4, LOW).
- `templates/template/devapi/smoke_bot.py` — валидаторы под `value.settings` +
  `schema.settings` (§A6.9).
- `templates/template/devapi/smoke_bot_test.py` — FakeGame-стабы + ассерты синхронно
  (§A6.9, HIGH).
- `features/game-state/feature.json` — `outputs` (пер-фрагментные) +
  `runtime_sources` + `src/features/settings.c/.h` (§A6.10).
- `features/game-state/INSTALL.md` — copy-list, wiring, generated-files (§A6.10).

**Ре-захватываемые golden (дифф = ровно снятие settings-строк, ревью глазами, §A6.7):**
- `features/game-state/tests/golden/game/game_state.h`.
- `features/game-state/tests/golden/game/game_state.c`.
- `features/game-state/tests/golden/game/game_state_schema.gen.h`.

**Добавляемый тест (дёшево, offline):**
- `features/game-state/scripts/generate_state_test.py` — `test_settings_schema_generates`
  (реальная settings-схема грузится + эмитит 5 файлов; §A6.7).

---

## A6.2 Ключевые решения

### Р9 в этом инкременте: стейт-слой генерится, логика руками
Дизайн §13 Р9: «путь один — у ВСЕХ фрагментов схема → стейт-слой генерируется всегда;
руками пишется только ЛОГИКА». Для `settings`:
- **Генерится** (`settings_state.{h,c}` + `_schema.gen.h`): `SettingsState`,
  `settings_state` инстанс, `init_defaults`/`validate`/`to_json`/`from_json`/
  `get_path_json`/`set_path_json`/`schema_json`, дескриптор `settings_state_fragment`.
- **Руками** (`src/features/settings.c`): feature-API `settings_master/music/sfx()` +
  `settings_set_*()` (clamp01 + запись генерённого инстанса + `game_save_mark_dirty`).
  Это единственная «умная» часть; она — образец, который игры копируют для своих фич.

Дескриптор-ABI НЕ меняется. `settings` — read-WRITE через DevAPI (генерённый
`set_path_json` присутствует), в отличие от read-only ручных фрагментов.

**Границы образца (MED-2 — Р9 живьём доказывается ЧАСТИЧНО).** `settings` — образец
**hooks-free data-фрагмента с read-write DevAPI**. Его дескриптор эмитится с
`.steps=NULL / .on_new_game=NULL / .reconcile=NULL`. Значит A6 НЕ тренирует и НЕ
доказывает живым фрагментом самую ошибкоопасную часть Р9: провод рукописных тел
`on_new_game`/`reconcile`/`migration step` через `hooks{}`/`migrations{}` схемы и их
`extern`-декларации в генерённом дескрипторе (generate_state.py:1308-1361). Эти пути
покрыты только юнит-тестом генератора (`test_migrations_and_hooks_descriptor`,
generate_state_test.py:218-249), но не собранным/выполненным фрагментом. ОТЛОЖЕНО до
`items`/`progression` (T0327), где стартовый контент (on_new_game) и карантин
(reconcile) — реальны. A6 сознательно не расширяет settings хуками ради «полноты
образца»: у громкостей нет ни стартового контента, ни пост-load фиксапа.

### Порядок фрагментов: settings РЕГИСТРИРУЕТСЯ ПЕРЕД game (инвариант «game последним»)
§14 п.2: `game` регистрируется ПОСЛЕДНИМ (самый зависимый — его `on_new_game`/`reconcile`
бегут последними, сеют контент через API других фич). ⇒ settings — раньше. Порядок
регистрации детерминирует и (а) порядок `reset/on_new_game/reconcile` fan-out, и (б)
**порядок сериализации** конверта, и (в) порядок агрегата DevAPI. Все три — по регистрации,
НЕ по алфавиту (build_root/build_aggregate бегут `s_fragments[i]`). Сохранить.

### Судьба старого ключа settings в game-фрагменте: clean-break, БЕЗ миграции
§14 п.12: у шаблона нет шипнутых сейвов, межинкрементной совместимости нет. Снятие
`settings.*` из `game_state.schema.json` означает: `game`-фрагмент перестаёт эмитить группу
`settings` (render_to_json эмитит группу только под объявленное поле). На старом дев-сейве
(`features.game.settings.*`) новый `game_from_json` этот под-объект просто НЕ читает (нет
поля, ссылающегося на него) → тихо отбрасывается. **Это не orphan** (orphan = незнакомый
ТОП-ключ в `features{}`, не под-ключ фрагмента). Миграционный шаг НЕ нужен — см. FSM §A6.6.

### DevAPI-мультифрагмент доказывается ОТСУТСТВИЕМ правок диспатча
`get ""` → `{settings:{...}, game:{...}}`; `get "settings.master_volume"` → число;
`set "settings.master_volume"` → мутация+dirty; `patch` кросс-фрагментный
(settings+game в одном вызове, фейл одной группы НЕ трогает другую — пофрагментная
атомарность A5 живьём); `schema` → `{settings:{...}, game:{...}}`; `save`/`load` —
оба фрагмента в конверте. Критерий приёмки: `game_save_devapi.c` в диффе A6 ОТСУТСТВУЕТ.

### Апплаер = персистентность + seam, НЕ game_audio
`game_audio.c` не в сборке (нет в add_executable, ноль вызовов). Честная «применимость»
громкости в шаблоне = ползунки persist'ятся через генерённый стейт (наблюдаемо: DevAPI
`get/set settings.master_volume`, round-trip через save/load, значения переживают перезапуск).
Пуш в аудио-микшер — помеченный комментарием seam в `settings.c` (живого микшера в шаблоне
нет). Отвергнуто: подключать `game_audio_set_volume` + тянуть `game_audio.c` в сборку —
scope-creep + rb-dark-хоррор-cruft (winmm, cue STALKER/CAUGHT).

---

## A6.3 `state/settings.schema.json` — контракт

```json
{
  "schema": "game_seed.settings",
  "schema_version": 2,
  "fragment": "settings",
  "version": 1,
  "string_max": 32,
  "fields": {
    "master_volume": { "type": "float", "default": 0.8, "min": 0, "max": 1 },
    "music_volume":  { "type": "float", "default": 0.7, "min": 0, "max": 1 },
    "sfx_volume":    { "type": "float", "default": 0.9, "min": 0, "max": 1 }
  }
}
```

- Пути ПЛОСКИЕ (без точки) → payload = `{master_volume, music_volume, sfx_volume, v:1}`;
  DevAPI-путь = `settings.master_volume` (голова = фрагмент, sub = поле).
- Нет `enums`/`types`/`events`/`hooks`/`migrations` — все опциональны (§0). `version:1`,
  `steps=NULL`, `on_new_game=NULL`, `reconcile=NULL`.
- Дефолты (0.8/0.7/0.9) выбраны = текущие видимые значения ползунков в
  `sys_settings.c` (было `s_master=0.8, s_music=0.7, s_sfx=0.9`), чтобы поведение UI не
  менялось визуально. Старые game-дефолты (0.75/0.8) отбрасываются с clean-break (Q3).
- `music_volume` ДОБАВЛЕН (в старом game-фрагменте его не было) — чтобы фрагмент нёс ровно
  3 громкости под 3 существующих ползунка; это честный полный пример и закрывает
  пред-A6 расхождение «UI имеет music, стейт — нет» (Q3).
- Генерит: тип `SettingsState`, инстанс `settings_state`, дескриптор
  `settings_state_fragment`, макросы `SETTINGS_STATE_*`, стаб-события.

---

## A6.4 Ручная логика Р9: `features/settings.{c,h}` + провод `sys_settings.c`

### `src/features/settings.h`

```c
#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H
/* Hand-written LOGIC for the `settings` fragment (design Р9). The state layer
   (SettingsState / defaults / (de)serialization / schema / descriptor) is
   GENERATED into settings_state.{h,c}; only this feature logic is by hand.
   A game copies this shape for its own smart fragments. */
float settings_master(void);
float settings_music(void);
float settings_sfx(void);
/* clamp to [0,1], write the generated instance, mark the save dirty. */
void  settings_set_master(float value);
void  settings_set_music(float value);
void  settings_set_sfx(float value);
#endif
```

### `src/features/settings.c`

```c
#include "features/settings.h"

#include "game_save.h"      /* game_save_mark_dirty */
#include "settings_state.h" /* generated: SettingsState + settings_state instance */

static float clamp01(float v) { return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v); }

float settings_master(void) { return settings_state.master_volume; }
float settings_music(void)  { return settings_state.music_volume; }
float settings_sfx(void)    { return settings_state.sfx_volume; }

void settings_set_master(float value) {
    settings_state.master_volume = clamp01(value);
    game_save_mark_dirty();
    /* apply seam: a real game pushes master/sfx to its audio mixer here. */
}
void settings_set_music(float value) {
    settings_state.music_volume = clamp01(value);
    game_save_mark_dirty();
}
void settings_set_sfx(float value) {
    settings_state.sfx_volume = clamp01(value);
    game_save_mark_dirty();
}
```

- Компилируется ТОЛЬКО под `FEATURE_GAME_STATE` (target_sources внутри FEATURE-блока,
  §A6.8), т.к. инклюдит генерённый `settings_state.h`.
- `-Werror`+`nt_set_warning_flags`: чистый C, без каст-варнингов (float↔float).
- **Асимметрия clamp vs reject (LOW-7a, осознанно, не баг):** UI-сеттеры `settings_set_*`
  КЛАМПЯТ (5.0→1.0) — ползунок и так в [0,1], clamp = страховка. DevAPI `set`/`patch`
  идут через ГЕНЕРЁННЫЙ `settings_state_set_path_json` → `validate` ОТКЛОНЯЕТ out-of-range
  (`"… out of range"`, golden game_state.c-паттерн :319-324). Два разных писателя, две
  политики: UI мягко кламп, DevAPI строго reject (это и питает patch-fail-isolation гейт
  §A6.11, где `settings.master_volume:5.0` фейлит группу).

### `sys_settings.c` — провод ползунков через feature-API

Сейчас (:24-31, :34-52, :85-87) UI держит ОТОРВАННЫЕ локальные статики
`s_master/s_music/s_sfx` и лишь дёргает `game_save_mark_dirty()` — стейт не читается/не
пишется. A6 делает ползунки персистентными через feature-API. `sys_settings.c`
компилируется БЕЗУСЛОВНО (CMake:127), поэтому feature-вызовы под `#if FEATURE_GAME_STATE`,
с локальным fallback при OFF.

Подход (границы — реализатору):
- Оставить локальные статики `s_master=0.8F, s_music=0.7F, s_sfx=0.9F` как backing-float
  ползунка (нужны и при `FEATURE_GAME_STATE` OFF — build feature-review-no-state).
- **Место reseed (LOW-7b, точно):** в `sys_settings_ui`, ПОСЛЕ guard `if (!s_open) return;`
  (:70-72), НЕПОСРЕДСТВЕННО ПЕРЕД тремя `volume_row` (:85-87), под `#if FEATURE_GAME_STATE`
  засеять статики из feature (`s_master = settings_master();` …) — авторитет = persisted
  стейт. `commit`-колбэк (`settings_set_*`) — ЕДИНСТВЕННЫЙ писатель `settings_state`
  громкостей; reseed только читает. `volume_row` принимает `commit`; на изменение зовёт его:
  ```c
  static void volume_row(nt_ui_context_t *ctx, const char *name, const char *id,
                         float *value, void (*commit)(float)) {
      char buf[48];
      (void)snprintf(buf, sizeof buf, "%s   %d%%", name, (int)(*value * 100.0F + 0.5F));
      const float before = *value;
      CLAY(/* … как сейчас … */) {
          nt_ui_label(/* … */);
          (void)nt_ui_slider_float(/* … &v == value … */);
      }
      if (*value != before && commit) { commit(*value); } /* persist (marks dirty) */
  }
  ```
  Вызовы:
  ```c
  #if FEATURE_GAME_STATE
      s_master = settings_master(); s_music = settings_music(); s_sfx = settings_sfx();
      volume_row(ctx, "Master", "settings/master", &s_master, settings_set_master);
      volume_row(ctx, "Music",  "settings/music",  &s_music,  settings_set_music);
      volume_row(ctx, "SFX",    "settings/sfx",     &s_sfx,     settings_set_sfx);
  #else
      volume_row(ctx, "Master", "settings/master", &s_master, NULL);
      volume_row(ctx, "Music",  "settings/music",  &s_music,  NULL);
      volume_row(ctx, "SFX",    "settings/sfx",     &s_sfx,     NULL);
  #endif
  ```
  `#include "features/settings.h"` под `#if FEATURE_GAME_STATE` (у :10-12).
- Снять старый `#if FEATURE_GAME_STATE ... game_save_mark_dirty() ...` из `volume_row`
  (:37-51) — dirty теперь внутри setter'а.
- `sys_settings_master/music/sfx()` (sys_settings.h:19-21, sys_settings.c:29-31): вернуть
  `settings_master()` и т.д. под FEATURE (или снять — grep: ноль внешних потребителей,
  LOW). Держать статики при OFF.

Наблюдаемое поведение: ползунок → persisted `settings_state` → переживает перезапуск;
DevAPI `set settings.master_volume` меняет то же значение, что видит UI.

---

## A6.5 Порядок сериализации фрагментов (детерминизм — зафиксировать)

- **Конверт (save):** `build_root` (game_save.c:309-317) бежит `s_fragments[0..count]` в
  порядке РЕГИСТРАЦИИ, затем orphans (:319-324). Регистрация A6 = `[settings, game]` ⇒
  `features:{settings:{…,v:1}, game:{…,v:1}}, <orphans>`.
- **DevAPI-агрегат (get ""/load value):** `build_aggregate` (devapi:62-71) бежит тем же
  `game_save_fragment_count/at` ⇒ `{settings:{…}, game:{…}}` (без `v`).
- **schema-агрегат:** `ep_state_schema` (devapi:97-107) — тот же порядок.
- НЕ сортируется. Инвариант: порядок в конверте/агрегате == порядок регистрации в main.c.
  Это делает golden/round-trip/смоук детерминированными. Тесты (`test_game_save.c`)
  ожидают именно порядок регистрации.

---

## A6.6 Снятие settings из game-схемы + прогон load-FSM (clean-break, БЕЗ миграции)

### Правка `game_state.schema.json`
Снять две строки из `fields` (:33-34) — это ВЕСЬ фактический состав секции `settings.*`
(проверено grep'ом: ровно два пути):
```
"settings.master_volume": { "type": "float", "default": 0.75, "min": 0, "max": 1 },
"settings.sfx_volume":    { "type": "float", "default": 0.8,  "min": 0, "max": 1 },
```
Больше в схеме `game` нет полей с префиксом `settings.` ⇒ `game`-фрагмент перестаёт
эмитить группу `settings`. `tutorial.*`, `inventory.*`, `wallet.*`, `hero.*`, RPG-поля —
остаются.

**Добавить снятые пути в `reserved` game-схемы (MED-1b, дисциплина §5+§14 п.9).** Текущее
`"reserved": ["grabbed"]` (:7) → `["grabbed", "settings.master_volume", "settings.sfx_volume"]`.
Генератор ENFORCE'ит reserved через `validate_field_names` (generate_state.py:169-170):
переиспользование умершего имени = `SystemExit`. Это делает вынос необратимым по имени и
учит образцовой дисциплине (умершее имя поля не воскрешают в том же фрагменте). Отказ от
reserved обосновать не удаётся — правило прямое.

### Что происходит с лишними ключами в `from_json` фрагмента (проверено по golden)
`game_state_from_json` (golden/game/game_state.c:1047-1049) читает `settings` через
`gsj_object_item(json,"settings")` СЕЙЧАС. После снятия полей эти строки исчезают из
генерённого кода → под-объект `settings` в старом сейве просто не читается → **тихо
отбрасывается** (не ошибка, не orphan). Это и есть clean-break: разовая потеря громкости
из старого дев-сейва (у шаблона шипнутых сейвов нет).

### Прогон load-FSM (§6/§14) на СТАРОМ дев-сейве (`features.game.settings.*`, нет `features.settings`)
Реестр A6 = `[settings(v1), game(v1)]`.
- **NEWER-детект** (`doc_is_newer`, game_save.c:367-389): `format=1` ок; `save_version=1`
  ок; `game.v=1 == code.v=1` ок; `settings`-фрагмент: `gsj_object_item(features,"settings")`
  → NULL (в старом сейве нет топ-ключа `settings`) → `if(!frag) continue`. ⇒ **НЕ NEWER**.
- **load_from_doc** (:424-459), порядок `[settings, game]`:
  - `settings`: `f = gsj_object_item(features,"settings")` → NULL → `frag->reset()`
    (дефолты 0.8/0.7/0.9), `continue`. `on_new_game` НЕ зовётся (правило «новая фича на
    живом сейве ≠ миграция», §6). ⇒ громкости = дефолты.
  - `game`: грузится; вложенный `settings` под-объект не читается (дропается); `tutorial`,
    `inventory`, `wallet`, `hero` — читаются как раньше.
  - `capture_orphans` (:457): топ-ключи `features{}` = только `game` (зарегистрирован) →
    orphans пусты.
- **Итог:** `LOADED`, settings = дефолты, старые вложенные громкости отброшены. Разово,
  безопасно (шаблон, clean-break). **Миграция/doc-шаг НЕ нужны.**

**MED-1a — заскоупить «миграция не нужна» ЯВНО (чтобы образец не учил «переноси поля
свободно»).** Тихий дроп законен ТОЛЬКО как template-clean-break (§14 п.12: у шаблона нет
шипнутых сейвов). ШИПНУТАЯ игра, переносящая поле между фрагментами, ОБЯЗАНА кросс-
фрагментным doc-шагом (§6 «save_version < DOC_VERSION → doc-шаги по сырому features»;
§14 п.9 «doc-шаги видят сейв КАК ЗАПИСАН; на каждый doc-шаг обязательна фикстура»):
поднять `GAME_SAVE_DOC_VERSION`, в doc-шаге переложить `features.game.settings.*` →
`features.settings.*` на сыром документе ДО пофрагментных чейнов. A6 этого не делает
исключительно потому, что переносить нечего (нет шипнутых сейвов).

**MED-1c — честная цена.** Пользовательские громкости на dev-сейве перехода A5→A6 МОЛЧА
сбрасываются к дефолтам (0.8/0.7/0.9). Приемлемо ТОЛЬКО потому, что шипнутых сейвов у
шаблона нет; для игры это была бы потеря данных, требующая doc-шага (см. MED-1a).

### Прогон на НОВОМ сейве (`features.settings` + `features.game`)
Оба фрагмента грузятся штатно, round-trip держится. FRESH-путь:
`reset_all` (settings→дефолты, game→дефолты) → `on_new_game_all` (у обоих NULL — у game
нет `hooks`) → save; конверт `features:{settings:{…}, game:{…}}`.

### Downgrade-заметка (не задача A6, механизм готов)
Старый билд без `settings` увидел бы `features.settings` как orphan (§14 п.16: незнакомый
топ-ключ = сирота, НЕ NEWER) → retain verbatim + warn + продолжение. У шаблона старых
билдов нет; фиксируется как «механизм защищает».

**Вывод:** судьба старого ключа = тихий дроп внутри game-фрагмента; конверт получает
`features.settings` рядом с `features.game`; FSM (FRESH/LOADED/NEWER/orphans) корректен на
обоих сейвах без единого миграционного шага. Правило дизайна «пер-фрагментная версия»
соблюдено (settings.version=1, чейн пуст).

---

## A6.7 Golden: ре-захват `game` (вынужденный), БЕЗ `golden/settings`

### Почему `game`-golden МЕНЯЕТСЯ
`generate_state_test.py:154-155` (`test_v2_template_golden`) генерит golden/game из ЖИВОЙ
`templates/template/state/game_state.schema.json`. Снятие `settings.*` меняет выхлоп ⇒
ре-захват ОБЯЗАТЕЛЕН (не опция). Затрагивает:
- `golden/game/game_state.h` — снятие struct-полей `settings_master_volume`/
  `settings_sfx_volume` + макросов `GAME_STATE_SETTINGS_*_DEFAULT/MIN/MAX`.
- `golden/game/game_state.c` — снятие строк settings в `init_defaults` (:249-250),
  `validate` (:319-324), `to_json` (:489-491), `get_path` (:574-578), `set_path`
  (:759-770), `from_json` (:1047-1049).
- `golden/game/game_state_schema.gen.h` — встроенная нормализованная схема теряет
  settings-поля.
- **БАЙТ-В-БАЙТ (гейт, git diff пуст):** `golden/game/game_state_events.gen.{h,c}`
  (секция events `game` не тронута), весь `golden/mini/*`.

### Процедура ре-захвата (как A5 §A5.6)
После правки `game_state.schema.json`:
```
py -3.12 features/game-state/scripts/generate_state.py \
  --schema templates/template/state/game_state.schema.json \
  --out-dir features/game-state/tests/golden/game --fragment game
```
`git diff golden/game` = РОВНО снятие settings-строк (ревью глазами; шире — промах в
схеме). `golden/game/*_events.gen.*` и `golden/mini/*` — 0-дифф.

### Почему НЕТ `golden/settings/`
game+mini уже фиксируют каждый рендер-путь, который использует settings (float-поля,
плоские пути, пустые events). settings не приносит нового кода генератора ⇒ третья fixture
= избыточная поддержка (задача: «не раздувай»). Корректность `settings` гейтится:
(а) компиляция `settings_state.c` в game-таргете под `-Werror` (§A6.11);
(б) DevAPI-смоук: живой `save`/`load` round-trip обоих фрагментов (§A6.9).

### `generate_state_test.py` — ОДИН дешёвый тест (offline)
Добавить `test_settings_schema_generates` (по образцу `test_clean_schema_generates_all_outputs`
:54-59). Инфраструктура (LOW-5): завести модульную константу
`SETTINGS_SCHEMA = ROOT / "templates" / "template" / "state" / "settings.schema.json"`
(по образцу `TEMPLATE_SCHEMA`, generate_state_test.py:10), читать выхлоп через
существующий `read_outputs(out_dir, "settings_state")` (:27). Сгенерить из РЕАЛЬНОЙ
`SETTINGS_SCHEMA` во временную папку, проверить:
- `settings_state{.h,.c,_schema.gen.h,_events.gen.h,_events.gen.c}` существуют;
- `"extern SettingsState settings_state;" in settings_state.h`;
- `"const GameSaveFragment settings_state_fragment" in settings_state.c`;
- `"settings_ev_desc_count = 0" in settings_state_events.gen.c` (пустые events → стаб).
Гейт: коммитнутая settings-схема остаётся валидной и генерящейся (ловит битую схему на
тесте, без полной сборки). Остальные тесты — без изменений.

---

## A6.8 Интеграция: main.c + CMake

### main.c
- У :53 (`#include "game_state.h"` под `#if FEATURE_GAME_STATE`) добавить
  `#include "settings_state.h"`.
- :374 — регистрация settings ПЕРЕД game (инвариант «game последним»):
  ```c
  game_save_register_fragment(&settings_state_fragment); /* settings before game */
  game_save_register_fragment(&game_state_fragment);     /* `game` last (§14 п.2) */
  ```
  Комментарий «the only fragment, hence also last» → «last (most dependent)».
- :390 (ветка `--fresh-state`, до/после `game_state_fragment.reset()`): добавить
  `settings_state_fragment.reset();` — иначе на `--fresh-state` (его использует смоук,
  `fresh_state=True`) `settings_state` = 0-init (master_volume=0.0). Оба фрагмента
  сеются дефолтами.
- **ЛИНК-FOOTGUN (MED-3, исполнителю прямо):** main.c инклюдит ТОЛЬКО `settings_state.h`;
  НЕ `settings_state_events.gen.h`; `settings_ev_register()` НЕ вызывать. `settings_state.h`
  события не тянет — инклюда достаточно для `SettingsState`/`settings_state_fragment`.
  `settings_state_events.gen.c` НЕ в target_sources (§A6.8 CMake) ⇒ символ
  `settings_ev_register` не определён ⇒ любой его вызов = красный линк (undefined reference).

### CMakeLists.txt (внутри `if(FEATURE_GAME_STATE)`, :131-183)
Добавить ВТОРОЙ generation-блок ПОСЛЕ game-блока (:159), в тот же
`${GAME_STATE_GENERATED_DIR}` (уже на include-path, :175):
```cmake
set(SETTINGS_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/settings.schema.json")
set(SETTINGS_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/settings_state.c")
add_custom_command(
    OUTPUT
        "${GAME_STATE_GENERATED_DIR}/settings_state.h"
        "${SETTINGS_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DIR}/settings_state_schema.gen.h"
        "${GAME_STATE_GENERATED_DIR}/settings_state_events.gen.h"
        "${GAME_STATE_GENERATED_DIR}/settings_state_events.gen.c"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_STATE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
        --schema "${SETTINGS_STATE_SCHEMA}"
        --out-dir "${GAME_STATE_GENERATED_DIR}"
        --fragment settings
    DEPENDS "${SETTINGS_STATE_SCHEMA}" "${GAME_STATE_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    COMMENT "Generating installed settings-state fragment sources"
    VERBATIM
)
```
В `target_sources(${GAME_TARGET} PRIVATE …)` (:160-166) добавить:
```cmake
    "${SETTINGS_STATE_GENERATED_SOURCE}"   # A6: generated settings fragment state layer
    src/features/settings.c                # A6: hand-written settings logic (Р9)
```
- `settings_state_events.gen.c` в target_sources НЕ добавлять (событий нет; в таргет не
  линкуется). OUTPUT его перечисляет (генератор пишет 5 файлов детерминированно).
- Обе custom command пишут в один каталог (разные имена) — параллельной сборке ок
  (`make_directory` идемпотентна; зависимости по файлам, не по каталогу).
- `test_game_state_roundtrip` (:343-353) — НЕ трогать (линкует только `game_state.c`;
  settings-агностичен; grep подтвердил — settings в нём нет; остаётся зелёным).

---

## A6.9 Смоук-бот на мультифрагмент (Р8)

`smoke_bot.py`. Имена команд/`REQUIRED_METHODS` — без изменений. Меняется форма:
`value.game.settings` уходит (settings вынесен) → `value.settings`.

- **`validate_game_state` (:121-136):** `value.game` больше НЕ содержит `settings`. Проверять:
  ```python
  value = state.get("value")
  game = value.get("game")
  if not isinstance(game, dict): raise DevApiError("game.state.get missing value.game fragment")
  for key in ("tutorial", "inventory"):                 # settings УБРАН из game-набора
      if not isinstance(game.get(key), dict): raise DevApiError(f"missing value.game.{key} object")
  settings = value.get("settings")                      # НОВЫЙ фрагмент рядом с game
  if not isinstance(settings, dict): raise DevApiError("game.state.get missing value.settings fragment")
  if not isinstance(settings.get("master_volume"), (int, float)):
      raise DevApiError("value.settings.master_volume is not a number")
  ```
- **`validate_game_state_schema` (:104-118):** добавить проверку фрагмента `settings` в
  агрегате:
  ```python
  settings = schema.get("settings")
  if not isinstance(settings, dict) or not isinstance(settings.get("fields"), list):
      raise DevApiError("game.state.schema missing 'settings' fragment fields")
  ```
  (Опц.: `settings.get("schema") == "game_seed.settings"`.) `game`-проверка — как есть.
- `summary` schema-строку можно бампнуть `"template.devapi_smoke.v3"` (опц., не гейт).

### `smoke_bot_test.py` — синхронно (HIGH; offline-юнит, НЕ ctest)
**MED-4 — ВСЕ 5 точек синхронной правки менять ВМЕСТЕ (валидаторы согласованы с живым
выхлопом, дефекта нет — нужна только дисциплина):** (1) FakeGame-стаб `schema` :32;
(2) FakeGame-стаб `get` :33-43; (3) `test_validate_..._snapshot` :78-81;
(4) `test_validate_..._schema` :84-87; (5) ассерт `test_run_smoke` :106. Деталь:
- **FakeGame `game.state.get` (:33-43):** форма →
  ```python
  "value": {
      "settings": {"master_volume": 0.8, "music_volume": 0.7, "sfx_volume": 0.9},
      "game": {"tutorial": {"done": False}, "inventory": {"item_ids": []}},
  }
  ```
- **FakeGame `game.state.schema` (:32):** →
  `{"game": {"schema": "game_seed.state", "document": "game", "fields": []},
    "settings": {"schema": "game_seed.settings", "fragment": "settings", "fields": []}}`.
- **`test_validate_game_state_requires_template_snapshot_shape` (:77-81):** позитив →
  `{"path":"","value":{"settings":{"master_volume":0.8},"game":{"tutorial":{},"inventory":{}}}}`;
  негатив (нет `settings`) → `{"path":"","value":{"game":{"tutorial":{},"inventory":{}}}}`
  (⇒ `DevApiError`).
- **`test_validate_game_state_schema_requires_template_schema` (:83-87):** позитив → добавить
  ключ `settings` с `fields:[]`; негатив `{}` — как есть (нет `game` → ошибка).
- **`test_run_smoke_toggles_render_and_writes_summary` (:106):** ассерт
  `summary["game_state"]["value"]["settings"]["master_volume"]` (или что бот пишет в
  summary) остаётся согласован; `game_state_schema["game"]["schema"]` — без изменений.

Гейт: `py -3.12 templates/template/devapi/smoke_bot_test.py` зелёный (offline) И
`devapi_smoke` (CMake:245) зелёный на живой devapi-debug сборке (реальный
`get ""`={settings,game}).

---

## A6.10 Доки: feature.json + INSTALL.md

### feature.json
- `outputs` (:33-37): сейчас game-only И даже не перечисляет events-файлы (протух).
  Перевести на ПЕР-ФРАГМЕНТНЫЙ паттерн (LOW-6): `<id>_state.h`, `<id>_state.c`,
  `<id>_state_schema.gen.h`, `<id>_state_events.gen.h`, `<id>_state_events.gen.c` — то, что
  генератор реально пишет на каждый фрагмент (main() generate_state.py:1816-1834).
- `default_template.runtime_sources` (:41-49): добавить
  `"templates/template/src/features/settings.c"` и `"…/settings.h"`. (M1-гард как в A5:
  copy-model добавляет существующие файлы — безопасно.)

### INSTALL.md
- Copy-list (:17-28): добавить `state/settings.schema.json`, `src/features/settings.c`,
  `src/features/settings.h`. **LOW-8 — НЕ добавлять `src/systems/sys_settings.c` в
  copy-list:** это template-system (UI-потребитель), НЕ deliverable фичи game-state; фича
  копирует только `src/features/settings.{c,h}`. Правка sys_settings.c (§A6.4) — интеграция
  в шаблоне, не установочный артефакт.
- CMake wiring (:36-45): добавить пункт «run the generator a SECOND time with the settings
  schema (`--fragment settings`) into the same generated dir; compile `settings_state.c`
  and hand-written `src/features/settings.c` under `FEATURE_GAME_STATE`».
- Runtime wiring (:50-72): добавить `#include "settings_state.h"` и
  `game_save_register_fragment(&settings_state_fragment);` ПЕРЕД game-регистрацией
  («settings before game; game last»); `settings_state_fragment.reset()` в fresh-state ветке.
- Generated Files (:113-123): обобщить «per fragment the generator writes
  `<id>_state.{h,c}`, `<id>_state_schema.gen.h`, `<id>_state_events.gen.{h,c}`»; отметить,
  что шаблон генерит два фрагмента: `game` и `settings`.
- Verify (:167-200): добавить, что `game.state.get {path:""}` возвращает
  `{settings, game}`, а `game.state.schema` — оба фрагмента.

---

## A6.11 План тестов

**Python (`generate_state_test.py`, offline):**
- ДОБАВИТЬ `test_settings_schema_generates` (§A6.7).
- `test_v2_template_golden` — станет зелёным ПОСЛЕ ре-захвата golden/game (§A6.7).
- Прочие (namespace-golden, property, validation, events) — без изменений.

**Ре-захват golden/game** (§A6.7): git diff = ровно снятие settings-строк.

**Ctest (остаются зелёными):** `test_game_state_json`, `test_game_storage`, `test_game_save`
(шелл-мультифрагмент уже там — FROZEN), `test_game_events`(+overflow), `test_game_events_typed`,
`test_game_state_roundtrip` (settings-агностичен), `check_mini_state_events`.

**Бот-юнит (`smoke_bot_test.py`, offline):** зелёный после §A6.9.

**Смоук (`devapi_smoke`, CMake:245) — ГЛАВНЫЙ функциональный гейт мультифрагмента**
на живой devapi-debug сборке. Ручные/бот-проверки 7 команд ПРОТИВ ДВУХ фрагментов:
- `get {path:""}` → `{"settings":{master_volume,music_volume,sfx_volume}, "game":{…}}`
  (порядок settings→game).
- `get {path:"settings.master_volume"}` → число.
- `get {path:"settings"}` → payload фрагмента.
- `set {path:"settings.master_volume", value:0.5}` → мутирует + dirty; эхо.
- `patch {values:{"settings.master_volume":0.3, "game.tutorial.done":true}}` → обе группы
  применены, per-key `true`.
- `patch {values:{"settings.master_volume":5.0, "game.tutorial.done":true}}` → settings-группа
  ФЕЙЛИТ (out-of-range) и ОТКАТЫВАЕТСЯ (master_volume не изменён), game-группа применена;
  per-key: `settings.master_volume=false`, `game.tutorial.done=true` — **«фейл в одном НЕ
  трогает другой» живьём** (в A5 доказано на моках).
- `schema` → `{"settings":{…fields[master_volume,…]}, "game":{…}}`.
- `save` → `load` → оба фрагмента round-trip; конверт `features:{settings, game}` в
  детерминированном порядке.

**Компиляция (гейт -Werror):** `settings_state.c` + `src/features/settings.c` собираются в
game-таргете warning-clean под `nt_set_warning_flags`.

---

## A6.12 Критерии приёмки (бинарные)

- [ ] `state/settings.schema.json` существует; `test_settings_schema_generates` зелёный;
      генератор эмитит `settings_state.{h,c,_schema.gen.h,_events.gen.h,_events.gen.c}`.
- [ ] **Диспатч НЕ тронут:** `git diff` НЕ содержит `templates/template/src/game_save_devapi.c`
      и `templates/template/src/game_save.{c,h}` (тезис A6 — механизм универсален).
- [ ] **Генератор НЕ тронут:** `git diff` НЕ содержит `generate_state.py`.
- [ ] `src/features/settings.{c,h}` существуют; логика = getters/setters+clamp+mark_dirty
      над генерённым `settings_state`; компилируется под `FEATURE_GAME_STATE` warning-clean.
- [ ] **(MED-2)** образец заскоплен как hooks-free data-фрагмент: дескриптор settings —
      `.steps/.on_new_game/.reconcile = NULL`; спека прямо фиксирует, что step/hook-тела Р9
      живьём НЕ доказываются и отложены до items/progression (T0327).
- [ ] main.c регистрирует `&settings_state_fragment` ПЕРЕД `&game_state_fragment`
      («game последним» сохранён); `--fresh-state` сбрасывает оба фрагмента. **(MED-3)**
      main.c инклюдит ТОЛЬКО `settings_state.h`, НЕ `_events.gen.h`; `settings_ev_register`
      НЕ вызывается.
- [ ] CMake: 2-й custom command (`--fragment settings`) + `settings_state.c` +
      `src/features/settings.c` в target_sources; `settings_state_events.gen.c` НЕ линкуется.
- [ ] `game_state.schema.json` без `settings.*`; **(MED-1b)** снятые пути добавлены в
      `reserved` (`["grabbed","settings.master_volume","settings.sfx_volume"]`); golden/game
      (`.h/.c/_schema.gen.h`) ре-захвачены — дифф = ровно снятие settings-строк (ревью
      глазами); golden/game `*_events.gen.*` и весь golden/mini — 0-дифф.
- [ ] **Мультифрагмент живьём (devapi_smoke + ручные 7 команд):** `get ""`=
      `{settings, game}` (порядок регистрации); `get/set settings.master_volume`;
      cross-фрагментный `patch` с фейлом одной группы НЕ трогает другую; `schema`=оба;
      `save`/`load` round-trip обоих; конверт `features:{settings, game}` детерминирован.
- [ ] **Load-FSM (§A6.6):** на старом дев-сейве (`features.game.settings.*`, без
      `features.settings`) → LOADED, settings=дефолты, старый под-объект дропнут, НЕ
      NEWER, НЕ orphan, БЕЗ миграции. **(MED-1a/c)** спека прямо фиксирует: тихий дроп +
      сброс громкостей к дефолтам законны ТОЛЬКО как template-clean-break; шипнутая игра
      обязана doc-шагом.
- [ ] `sys_settings.c` ползунки persist'ятся через feature-API (master/music/sfx →
      `settings_state` → переживают перезапуск); `FEATURE_GAME_STATE=OFF` сборка
      (feature-review-no-state) собирается (локальный fallback).
- [ ] Смоук-бот: `value.settings` + `schema.settings` валидируются;
      `smoke_bot_test.py` зелёный (стабы+ассерты синхронны, HIGH).
- [ ] `feature.json`/`INSTALL.md` перечисляют settings-схему, `features/settings.{c,h}`,
      два фрагмента.
- [ ] Все существующие ctest зелёные; сборки native-debug + devapi-debug (-Werror) +
      wasm-devapi-debug (**компиляция TU, НЕ линк** — известный движковый web-devapi
      link-блокер, INSTALL.md:224-234, лиду доложен); все TU (вкл. `settings_state.c`,
      `features/settings.c`) warning-clean.

---

## A6.13 Порядок работ

0. **Baseline:** собрать native-debug + devapi-debug + прогнать все ctest +
   `generate_state_test.py` + `smoke_bot_test.py` на HEAD → «зелёный до» доказан.
   Зафиксировать статус wasm-devapi-debug (компиляция; линк — известный SKIP).
1. **Схема settings (fast):** `state/settings.schema.json`. Прогнать генератор руками в
   tmp → убедиться в 5 файлах + плоском payload.
2. **Ручная логика (deep/fast):** `src/features/settings.{c,h}`.
3. **CMake (fast):** 2-й custom command + target_sources. Собрать devapi-debug (settings_state.c
   + settings.c компилятся; ещё не зарегистрированы).
4. **Регистрация (fast):** main.c (#include + register-before-game + fresh-state reset).
5. **Вынос settings из game-схемы (fast):** снять :33-34; ре-захват golden/game; ревью диффа.
6. **Тест генератора (fast):** `test_settings_schema_generates`; прогнать
   `generate_state_test.py` зелёным (вкл. обновлённый game-golden).
7. **Провод UI (fast):** `sys_settings.c` через feature-API (+FEATURE-OFF fallback).
8. **Смоук (deep/fast):** `smoke_bot.py` + `smoke_bot_test.py` синхронно.
9. **Доки (fast):** `feature.json`, `INSTALL.md`.
10. **Гейт:** native-debug + devapi-debug + wasm-devapi-debug(компиляция) (-Werror);
    все ctest; `smoke_bot_test.py`; `devapi_smoke`; ручные 7 команд против 2 фрагментов
    (§A6.11), особо — cross-фрагментный patch с частичным фейлом.

Зависимости: шаг 4 требует шага 2 (символ `settings_state_fragment` определён генератором
из шага 1/3). Шаг 5 (ре-захват golden) — ПОСЛЕ снятия полей. Шаг 8 требует шага 5 (форма
`value.settings` живёт после выноса).

---

## A6.14 Риски

- **R1 (golden/game не байт-идентичен).** Вынужденно (game-golden генерится из живой
  схемы). Митигация: дифф ревьюится глазами = ровно снятие settings-строк;
  `*_events.gen.*` + mini = 0-дифф гейт.
- **R2 (провод sys_settings + FEATURE_GAME_STATE OFF).** UI компилится безусловно; feature
  под `#if FEATURE_GAME_STATE` с локальным fallback. Гейт: build feature-review-no-state.
- **R3 (порядок сериализации).** Регистрация settings-раньше-game → конверт
  `{settings, game}`. Детерминирован (§A6.5); тесты ожидают порядок регистрации. Низкий.
- **R4 (два custom command в один каталог).** Разные имена, идемпотентный make_directory,
  зависимости по файлам. Параллельной ninja ок. Низкий.
- **R5 (пустые events генерятся, но не линкуются).** `settings_state_events.gen.c` — стаб,
  в target_sources не входит; `settings_ev_register` не зовётся. Компилятор его не видит —
  безвредно. Низкий.
- **R6 (wasm-devapi линк).** Известный движковый блокер (INSTALL.md:224-234): TU компилятся
  clean, executable не линкуется (engine-side, read-only). A6-гейт = компиляция
  `settings_state.c`/`settings.c`, НЕ линк. Не риск A6.
- **R7 (`game_audio`/`sys_settings_*` мёртвый код).** `game_audio.c` не в сборке, getters без
  потребителей — rb-dark-остаток. A6 их НЕ оживляет и НЕ чистит (отдельный cleanup). Не риск.

---

## A6.15 Вопросы лиду (дефолт применён — спека не блокируется)

- **Q1 [ДЕФОЛТ]. Отдельный `src/features/settings.c` (Р9-образец) vs. слить логику в
  `sys_settings.c` (без нового файла)?** ДЕФОЛТ: ОТДЕЛЬНЫЙ файл. Почему: шаблон учит
  паттерн «генерённый стейт + рукописная feature-логика как копируемый модуль»; отдельный
  артефакт — кришее для будущих фич (items/progression). Слияние = чуть меньше файлов, но
  размывает границу UI↔логика. Реверс тривиален.
- **Q2 [ДЕФОЛТ]. Добавлять `golden/settings/`?** ДЕФОЛТ: НЕТ. game+mini покрывают все
  рендер-пути settings; компиляция под -Werror + DevAPI round-trip — достаточный гейт;
  третья fixture = избыточная поддержка. Если лид хочет байт-lock settings-выхлопа — добавить
  `golden/settings/*` + `test_v2_settings_golden` (симметрично game).
- **Q3 [ДЕФОЛТ]. Состав settings: master+music+sfx (3 громкости)?** ДЕФОЛТ: три (под 3
  существующих ползунка; `music` добавлен — в старом game-фрагменте его не было). Дефолты
  0.8/0.7/0.9 = текущие видимые значения UI (визуального изменения нет). Старые
  game-дефолты (0.75/0.8) отброшены с clean-break. Если лид хочет ровно старый набор
  (master+sfx) — снять `music_volume` и оставить music-ползунок эфемерным.
- **Q4 [ДЕФОЛТ]. Отдельный ctest round-trip для settings?** ДЕФОЛТ: НЕТ (компиляция в
  таргете + DevAPI save/load покрывают тривиальный float-фрагмент). Если лид хочет —
  `test_settings_state_roundtrip.c` (симметрично game) в отдельном инкременте.

Все дефолты консервативны, обратимы, не блокируют исполнение.
