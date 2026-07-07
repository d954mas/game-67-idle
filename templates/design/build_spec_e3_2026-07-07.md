# BUILD-SPEC: DevAPI-хвост событийного лога + читаемые метки, инкремент E3 (2026-07-07)

Имплементационная спецификация. Переводит принятый дизайн в файлы, сигнатуры, CMake,
тесты и критерии приёмки. НЕ меняет дизайн. При расхождении источник истины —
`templates/design/event_system_design_2026-07-06.md` (главный: §4 DevAPI-лента = render-at-copy,
§3 дескрипторы, §7 Q4 bytes, §8 NT_HASH_LABELS), затем `build_spec_e1_2026-07-06.md`
(транспорт game_events — ЗАМОРОЖЕН, §E1.9 шов имён/аллокатора), `build_spec_e2_2026-07-06.md`
(генерённые дескрипторы `<frag>_ev_descs`/FT-enum/оффсеты — E3 их ПОТРЕБИТЕЛЬ),
`features/game-state/references/build_spec_a5_2026-07-07.md` (форма DevAPI-ответов/ошибок:
`error.code = {bad_params, internal}`; рукописный TU под `NT_DEVAPI_ENABLED`).

Исполнитель может писать код по этому документу, НЕ открывая историю обсуждений.

---

## 0. Предпосылки и рамки (проверено по дереву HEAD 2026-07-07)

- **E1 (транспорт) + E2 (генерённые дескрипторы) + A4–A6 УЖЕ в дереве.** Живой путь:
  `game_events.{c,h}` (сырой `game_event_emit`, `game_event_log(&n)`, `game_events_tick()`,
  `game_events_dropped()`, двухфазный кадр, `game_event_register_type_name`); shared-контракт
  `src/game_event_desc.h` (`game_event_desc_t {name, payload_size, fields, field_count}`,
  `game_event_field_t {name, type, offset, len_offset}`, `game_event_field_type_t`
  = `BOOL/INT/I64/FLOAT/STRING/HASH/BYTES`); генерённые `<frag>_state_events.gen.{h,c}` с
  пер-фрагментной таблицей `game_ev_descs[]`/`game_ev_desc_count` и `game_ev_register()`
  (регистрирует метки типов). `game_state.schema.json` НЕСЁТ событие `shape_changed`
  (`game_ev_descs` — 1 запись: int/int/string); `settings.schema.json` событий НЕ несёт.
- **Двухфазный кадр УЖЕ проведён (main.c:249-261).** `game_features_update` (EMIT) →
  `game_events_react_begin()` → `do { game_features_react } while (react_progressed())` →
  `set_phase(RECORD)` → `game_features_record(&s_world)` → (autosave) → `game_event_frame_reset()`.
  **Фаза RECORD — дом рекордера E3** (арена ЖИВА до `frame_reset`; `game_features_record`
  вызывается РОВНО ОДИН раз/кадр). Скелет `game_features_record` (game_features.c:46-48) несёт
  ЯКОРЬ: `TODO(E3/E4): рекордеры (DevAPI tail E3, аналитика E4)` — E3 закрывает якорь одной
  гейт-строкой (см. §E3.6).
- **DevAPI-модель A5 (проверена):** рукописный TU `game_save_devapi.c` под `#if NT_DEVAPI_ENABLED`
  (при OFF — пустой объектник), регистрируется из `devapi_start()` (main.c:153). Хендлер =
  `bool fn(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user)`;
  `nt_devapi_register(&desc, fn, NULL)`; `nt_devapi_command_desc` = 7 строк
  (`method, group, summary, params_shape, result_shape, frame_behavior, side_effects`);
  `nt_devapi_error = {const char *code; const char *message;}` (обе строки должны ПЕРЕЖИТЬ
  вызов — static-буфер). **E3 повторяет ровно эту модель** отдельным TU `game_events_devapi.c`.
- **Метки типов (движок, проверено `nt_hash.h`):** `nt_hash64_label(nt_hash64_t)` → `const char*`;
  тело живёт в `nt_hash.c` под `#if NT_HASH_LABELS` (дефолт **0**, `nt_hash.h:10-12`); при
  выключенном флаге `register_label64` — no-op, `nt_hash64_label` → **NULL**. `game_ev_register()`
  (E2) регистрирует ИМЕНА ТИПОВ («game.shape_changed»), но эффект НУЛЕВОЙ пока движковый
  `nt_hash` собран без `NT_HASH_LABELS`. **E1/E2 ОТЛОЖИЛИ включение флага на E3** (E1 §E1.9,
  E2 §E2.13 «NT_HASH_LABELS build-config — E3»). E3 включает флаг в devapi-сборках (§E3.7).
- **Ключевой факт (упрощает E3):** имя ТИПА события для читаемого рендера берётся из
  `desc->name` (строка в дескрипторе, всегда доступна для ЗАРЕГИСТРИРОВАННОГО события) — метки
  движка для имён типов НЕ обязательны. `NT_HASH_LABELS` нужен ТОЛЬКО для (а) значений
  `hash`-полей (enum-как-hash: «Epic» вместо хекса) и (б) имён НЕЗАРЕГИСТРИРОВАННЫХ событий
  (нет дескриптора). Без флага E3 поведенчески корректен: типизированные события читаемы,
  hash-значения/сырые события — хекс.
- **Сборка:** game-таргет `-Werror` + `nt_set_warning_flags` (`-Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wundef`, `-Wno-unused-parameter`).
  u64/u32 hex — `PRIx64`/`PRIx32` из `<inttypes.h>`. `_Alignof(max_align_t)==8`. Чтение полей
  payload'а — через `memcpy` в локаль (портируемо, без `-Wcast-align`/strict-aliasing UB).
- **Один тред.** Ринг/реестр/рекордер — file-static, потокобезопасности не требуют.
- **Среда:** движок `external/neotolis-engine` READ-ONLY (public API `devapi/nt_devapi.h`,
  `cJSON.h`, `hash/nt_hash.h`); clangd-диагностика = шум (истина — ninja); сендбокс запрещает
  `rm -rf`; **wasm-devapi линк КРАСНЫЙ на HEAD (движок, доложено лиду)** → wasm-гейт E3 =
  КОМПИЛЯЦИЯ новых TU под `wasm-devapi-debug` (не полный линк).

### Что E3 делает (ровно)

1. Рукописный **универсальный рендерер** события в компактный JSON по дескриптору —
   `src/game_event_render.{c,h}` (L0, НЕ devapi-gated внутри; юнит-тестируем нативно).
2. Рукописный **DevAPI-TU** `src/game_events_devapi.{c,h}` под `#if NT_DEVAPI_ENABLED`:
   (а) реестр `hash → desc` (заполняется из `<frag>_ev_descs`); (б) рекордер render-at-copy в
   фикс-ринг (256 × 512Б, oldest-evicted); (в) команда `game.events.tail`; (г) регистрация.
3. **Проводка:** `main.c` (include + регистрация команды + регистрация дескрипторов),
   `game_features.c` (одна гейт-строка вызова рекордера в фазе RECORD).
4. **NT_HASH_LABELS=1** на движковый `nt_hash` в devapi-сборках (правка CONSUMING CMake).
5. **CMake:** два новых TU в game-таргет под `GAME_DEVAPI_ENABLED`; ctest `test_game_event_render`
   (нативный, без devapi).
6. **Смоук-бот (tool parity + triple-sync):** `game.events.tail` в `REQUIRED_METHODS` +
   валидатор + `smoke_bot_test.py` (FakeGame-стаб + тест валидатора).

### FROZEN в E3 (не менять)

- `templates/template/src/game_events.{c,h}` — E1 транспорт, целиком.
- `templates/template/src/game_event_desc.h` — E2 shared-контракт (E3 его ПОТРЕБИТЕЛЬ).
- **Генератор `generate_state.py` + golden `*_state_events.gen.{h,c}` — НЕ трогать.** Дескрипторы
  E2 уже несут всё нужное (name, payload_size, fields{type,offset,len_offset}). E3 генератора
  НЕ касается → **E3 и E4 параллельны по генератору/golden** (см. §E3.14).
- `game_save.{c,h}`, `game_save_devapi.c`, `game_state*.gen.*`, движок, `games/**`.
- `game_features.c` — ТОЛЬКО одна гейт-строка вызова рекордера в `game_features_record`
  (закрытие якоря E1 §E1.4; НЕ структурная правка).

---

## E3.1 Файлы

**Новые (рукописные):**
- `templates/template/src/game_event_render.h` — контракт рендерера (§E3.4).
- `templates/template/src/game_event_render.c` — дескриптор-driven JSON-рендер (§E3.4).
- `templates/template/src/game_events_devapi.h` — devapi-gated API (реестр/рекордер/регистрация, §E3.5).
- `templates/template/src/game_events_devapi.c` — ринг + реестр + рекордер + `game.events.tail` (§E3.5).
- `templates/template/tests/test_game_event_render.c` — Unity ctest на рендерер (§E3.10).

**Изменяемые:**
- `templates/template/src/main.c` — include + регистрация команды + регистрация дескрипторов (§E3.6).
- `templates/template/src/features/game_features.c` — одна гейт-строка рекордера (§E3.6).
- `templates/template/CMakeLists.txt` — два TU под `GAME_DEVAPI_ENABLED` + `NT_HASH_LABELS` +
  ctest (§E3.7).
- `templates/template/devapi/smoke_bot.py` — `game.events.tail` + валидатор (§E3.8).
- `templates/template/devapi/smoke_bot_test.py` — FakeGame-стаб + тест валидатора (§E3.8).

**Не трогать:** всё из FROZEN (§0). Генератор/golden — 0 правок.

---

## E3.2 Механика снапшота: render-at-copy → фикс-ринг (дизайн §4)

**Проблема владения:** события живут ОДИН кадр в арене (E1); `frame_reset` их отравляет
(0xDD в debug). DevAPI-бот стучится по сети АСИНХРОННО (много кадров спустя). Держать
указатели в арену через кадры ЗАПРЕЩЕНО (главный анти-баг, event §5). **Решение (дизайн §4,
финальное ревью): render-at-copy.** Рекордер в СВОЮ фазу (RECORD, арена ЖИВА) рендерит КАЖДОЕ
событие кадра по дескриптору в самодостаточную JSON-строку и кладёт в приватный фикс-ринг.
Бот спрашивает `game.events.tail` когда хочет и получает ГОТОВЫЕ строки — ни доступа к
дескрипторам, ни вопросов времени жизни арены на момент запроса.

**Владение и время жизни:**
- Ринг — приватные file-static буферы В `game_events_devapi.c` (game-side, вне арены событий).
  Строки — КОПИИ, самодостаточны, НЕ содержат указателей в арену. Живут пока не вытеснены.
- **Фикс-ринг (дизайн §4: N=256, ≤512Б, oldest-evicted):**
  `static char s_ring[256][512]` (~128КБ BSS, dev-only) + `static uint64_t s_ring_seq[256]`
  (глобальный seq каждой записи — для курсора `since_seq`) + `head`/`count`/`evicted`/`last_seq`.
  Роста НЕТ (паттерн фикс-арены E1); переполнение = вытеснение старейшего + счётчик `evicted`.
- **Ноль удержания указателей арены между кадрами** — рендер происходит В фазе RECORD, строка
  копируется в ринг СРАЗУ; после `frame_reset` ринг ни от чего в арене не зависит. Класс
  UAF-багов не возникает (то же свойство, что дало фикс-арене E1).
- **Сырые variable-size payload'ы НЕ рингуются** (дизайн §4) — рингуется только РЕНДЕР
  (ограниченная строка ≤512Б со срез-маркером).

**Проход (ровно один линейный, дизайн §2 страж 1 «record одним проходом»):** рекордер
вызывается ОДИН раз/кадр из `game_features_record`; идёт по `game_event_log(&n)` 0..n-1,
рендерит и пушит КАЖДОЕ событие РОВНО ОДИН раз (лог свежий каждый кадр → курсор `s_pos` НЕ
нужен, в отличие от react-потребителей). Каскадные события уже в логе (react достиг фикспойнта
до RECORD) → рекордер видит полный кадр.

**Кап-политика (не раздувать):** если событий за кадр > 256, ринг вытесняет старейшие В ТОМ ЖЕ
кадре (`evicted++`) — хвост = катящееся окно последних 256 событий. Строка > 512Б → усечение до
валидного `{seq,tick,type,truncated:true}` (§E3.4).

---

## E3.3 Поверхность DevAPI: команда `game.events.tail` (tool parity)

> **[ПОПРАВЛЕНО ночью 2026-07-07: offset-курсор против молчаливой потери seq=0; эмпирика
> исполнителя. РАТИФИЦИРОВАНО лидом 2026-07-07: since_seq = «отдай начиная С этого id»,
> next_seq = «с какого id спрашивать дальше».]** Транспорт E1 нумерует `seq` с НУЛЯ (`game_events.c:92`,
> `s_seq=0` при init) → первое событие сессии имеет `seq==0`. Исходный контракт `seq > since_seq`
> при дефолте `since_seq=0` МОЛЧА терял seq=0 (не в `dropped`/`evicted`) — каноническая идиома §E3.9
> его не видела (подтверждено живьём: эмит seq 0,1,2 → `{}` возвращал только 1,2). Правка:
> `since_seq` — ВКЛЮЧИТЕЛЬНАЯ нижняя граница (`seq >= since_seq`), `next_seq = seq_последнего+1`
> (следующий оффсет; пусто → эхо `since`). Дефолт `since_seq=0` теперь отдаёт и seq=0; без дублей и
> потерь. Идиома §E3.9 не меняется (`since = next_seq`). Прецедент оформления — правка §2 в
> `event_system_design_2026-07-06.md` (фикс-арена). E4 курсор НЕ использует → потребителей старой
> семантики нет.

**Решение лида «7 команд» касалось СОХРАНЕНИЯ state-поверхности `game.state.*` (A5) — не
запрета новых осмысленных команд.** Хвост событий — отдельный концерн (наблюдаемость, не
стейт) → НОВАЯ команда `game.events.tail`, группа `"game"`, в НОВОМ TU (не в `game_save_devapi.c`).
**Tool parity (hard invariant): новая команда ОБЯЗАНА появиться в смоук-боте + его тестах**
(triple-sync: `game_events_devapi.c` регистрирует ↔ `smoke_bot.py` требует+валидирует ↔
`smoke_bot_test.py` стабит+тестирует) — §E3.8.

**Дескриптор команды** (`nt_devapi_command_desc`, 7 строк):
```c
{"game.events.tail", "game",
 "Tail the per-frame feature event log (render-at-copy ring).",
 "since_seq?, limit?", "events, next_seq, dropped, evicted", "immediate", "none"}
```

**Params (все опциональны — минимальная поверхность):**
- `since_seq` (number, дефолт 0): ВКЛЮЧИТЕЛЬНАЯ нижняя граница — вернуть записи с `seq >= since_seq`
  (offset-курсор, поправка выше; инкрементальный тайлинг; бот помнит `next_seq` прошлого ответа).
  Читается как double→uint64 (для debug-диапазона seq точно; >2^53 — известная граница, §E3.13).
- `limit` (number, дефолт = 256): максимум записей за вызов, клампится в `[1, 256]`. Возвращаются
  СТАРЕЙШИЕ подходящие (ascending seq) → бот проходит бэклог инкрементально, без пропусков.

**Response (`result_obj`):**
```json
{ "events": [ {<рендер>}, ... ],   // oldest -> newest, seq >= since_seq, до limit
  "next_seq": <u64>,               // seq последнего возвращённого + 1 (следующий оффсет; since, если пусто)
  "dropped": <u64>,                // game_events_dropped() — переполнение арены/лога (health)
  "evicted": <u64> }               // кумулятивные вытеснения ринга (бот отстал -> потеря)
```
`dropped>0` ИЛИ `evicted>0` = бот/агент потерял события (сигнал поднять кап/чаще опрашивать).
`error.code` (внутренний сбой ИЛИ кривой ПРЕДОСТАВЛЕННЫЙ параметр) ∈ `{internal, bad_params}`
(замороженный набор A5 §MED-3); описание — в `message`. `side_effects: none` (стейт не мутирует).
**Строгость params выровнена с `game.state.*` (MED-4):** ОТСУТСТВУЮЩИЙ параметр → дефолт;
ПРЕДОСТАВЛЕННЫЙ, но кривой (не тот тип / `limit<1` / отрицательный) → `bad_params`+message
(НЕ тихий дефолт — консистентность поверхности с A5; толерантный вариант отвергнут).

**ПРАВИЛО КОГЕРЕНТНОСТИ ЧИСЕЛ/СТРОК (MED-3, пригвождено state doc §14 п.8):** конвертные КУРСОРЫ
(`seq`/`next_seq`/`tick`/`dropped`/`evicted`) = JSON-ЧИСЛА (счётчики событий сессии, всегда
<< 2^53 → точны в double); PAYLOAD-`i64`-поля (неограниченные игровые счётчики) = JSON-СТРОКИ
(§E3.4), чтобы `game.events.tail` и `game.state.get` давали ИДЕНТИЧНУЮ форму ОДНОГО счётчика
(`gsj_add_i64` = `AddStringToObject "%lld"`, reader отвергает числа >2^53 — game_state_json.c:222-230).

---

## E3.4 `game_event_render.{c,h}` — универсальный рендер по дескриптору

L0-рендерер, ЧИСТЫЙ (без devapi/nt_devapi), зависит от `game_events.h` + `game_event_desc.h` +
`hash/nt_hash.h` + `cJSON.h`. Юнит-тестируем нативно (§E3.10). Компилится в game-таргет ТОЛЬКО
под `GAME_DEVAPI_ENABLED` (единственный потребитель — рекордер), но в ctest — standalone.

### `game_event_render.h` (полностью)
```c
#ifndef GAME_EVENT_RENDER_H
#define GAME_EVENT_RENDER_H

#include "game_event_desc.h" /* game_event_desc_t */
#include "game_events.h"     /* game_event_t */

/* Renders ONE event to a compact JSON object string in out[0..cap) (NUL-terminated).
   desc==NULL => unregistered/raw event: { seq, tick, type, size, unknown:true, hex }.
   Positional-independent: string/bytes read via payload-relative offsets from the
   descriptor. EVERY read is bounds-checked against e->size BEFORE dereferencing (LOW-6):
   a scalar/offset/len word is read only if offset+width <= e->size (BYTES also needs
   len_offset+4 <= e->size); a STRING's inline bytes are scanned with a BOUNDED strnlen
   within [soff, e->size) and, if no NUL is found in range, the field is emitted as
   { "size":N, "truncated":true } (NEVER AddStringToObject on an unterminated span --
   no over-read). Any out-of-range field is skipped / marked, never dereferenced.
   Numbers vs strings (§14 п.8): i64 fields ride as a JSON STRING (gsj_i64_to_string) --
   NEVER a double (bit-for-bit parity with game.state.get; envelope seq/tick stay numbers).
   Type name: desc->name if desc, else nt_hash64_label(e->type) if non-NULL, else
   "0x%016<PRIx64>" hex. hash-FIELD values: nt_hash64_label else hex. bytes fields:
   { "size":N, "hex":"..." } (event §7 Q4: DevAPI shows size+hex; hex truncated to fit).
   If the full render would exceed cap, emits a valid minimal
   { seq, tick, type, truncated:true } instead (the ring's ≤512B slice-marker, §E3.2).
   ALWAYS writes well-formed JSON. Returns the written length (< cap). */
int game_event_render(const game_event_t *e, const game_event_desc_t *desc, char *out, int cap);

#endif /* GAME_EVENT_RENDER_H */
```

### `game_event_render.c` — контракт реализации
Инклюды: `"game_event_render.h"`, `"hash/nt_hash.h"` (`nt_hash64_label`),
`"game_state_json.h"` (`gsj_i64_to_string` — i64→строка, MED-3; уже под FEATURE_GAME_STATE-гейтом),
`"cJSON.h"`, `<inttypes.h>` (`PRIx64`), `<string.h>` (`memcpy`/`strlen`/`memchr` для bounded-NUL),
`<stdio.h>` (`snprintf` для hex), `<stdint.h>`. **Реализация через cJSON** (корректный escaping
строк/имён; никакого ручного JSON-склеивания под `-Werror`).

1. `cJSON *root = cJSON_CreateObject();`
   `cJSON_AddNumberToObject(root, "seq", (double)e->seq);`
   `cJSON_AddNumberToObject(root, "tick", (double)e->tick);`
   Имя типа: `const char *tname = desc ? desc->name : nt_hash64_label(e->type);`
   если `tname` NULL → `char hexn[19]; snprintf(hexn,sizeof hexn,"0x%016" PRIx64, e->type.value); tname=hexn;`
   `cJSON_AddStringToObject(root, "type", tname);`
2. **desc==NULL (сырое/незарегистрированное событие):** `cJSON_AddNumberToObject(root,"size",(double)e->size);`
   `cJSON_AddBoolToObject(root,"unknown",true);` + hex-дамп payload'а (до `GAME_EVENT_RENDER_HEX_MAX`
   байт, дефолт 48; из них `snprintf %02x`) в `"hex"` (усечение помечать `"…"` не обязательно —
   поле `size` даёт полную длину). Флаги-поля события НЕ трогаем (типизации нет). Перейти к шагу 4.
3. **desc!=NULL:** по `desc->fields[0..field_count)` (плоские ключи — имена полей
   `[a-z][a-z0-9_]*` НЕ коллидируют с `seq/tick/type`: E2 их зарезервировал):
   `const uint8_t *base = (const uint8_t *)e->payload;` для каждого поля СНАЧАЛА проверить bounds
   `f.offset + width <= e->size` (иначе поле ПРОПУСТИТЬ, без дереференса — LOW-6), затем `memcpy`
   в локаль по `f.offset`:
   - `BOOL` → `cJSON_AddBoolToObject(root, f.name, val!=0);`
   - `INT` → `int32_t` → `cJSON_AddNumberToObject(root, f.name, (double)val);` (число: int32 ≤ 2^31)
   - `I64` → `int64_t` → **СТРОКА** (MED-3, §14 п.8): `char b[24]; gsj_i64_to_string(val, b, sizeof b);`
     `cJSON_AddStringToObject(root, f.name, b);` (парити с `game.state.get`; НЕ double)
   - `FLOAT` → `double` → `cJSON_AddNumberToObject(root, f.name, val);`
   - `STRING` → прочитать `uint32_t soff` по `f.offset` (bounds `f.offset+4 <= e->size`); затем
     `soff < e->size`; **bounded-NUL:** `const void *nul = memchr(base+soff, 0, e->size - soff);`
     если `nul` → строка валидна `(const char*)base + soff` → `cJSON_AddStringToObject(root, f.name, s)`;
     если NUL НЕ найден в диапазоне ИЛИ `soff >= e->size` → НЕ `AddStringToObject` (over-read!),
     а `cJSON *t=AddObjectToObject(root,f.name); AddNumberToObject(t,"size",...); AddBoolToObject(t,"truncated",true);`
   - `HASH` → `nt_hash64_t` по `f.offset`; `const char *lbl = nt_hash64_label(h);`
     если lbl → `AddStringToObject(root, f.name, lbl)`; иначе `snprintf("0x%016" PRIx64, h.value)`
     → `AddStringToObject` (enum-как-hash благословлён; без NT_HASH_LABELS — хекс).
   - `BYTES` → `uint32_t boff` по `f.offset` (bounds `f.offset+4 <= e->size`), `uint32_t blen` по
     `f.len_offset` (bounds `f.len_offset+4 <= e->size` — LOW-6); затем `boff + blen <= e->size`;
     `cJSON *b = cJSON_AddObjectToObject(root, f.name);` `cJSON_AddNumberToObject(b,"size",(double)blen);`
     + hex до `GAME_EVENT_RENDER_HEX_MAX` байт из `base+boff` в `"hex"` (event §7 Q4: рекордер/
     аналитика игнорируют, DevAPI size+hex). Любой bounds-фейл → `size` без `hex` (без over-read).
4. `char *s = cJSON_PrintUnformatted(root);` `int len = s ? (int)strlen(s) : -1;`
   если `s && len < cap` → `memcpy(out, s, (size_t)len + 1);` иначе → **усечённый фолбэк**
   (валидный минимум): собрать `cJSON *t = {seq,tick,type,truncated:true}`, распечатать,
   скопировать (гарантированно влезает: ≤~90Б); если и это не влезет — `snprintf(out,cap,"{}")`.
   `cJSON_free(s); cJSON_Delete(root);` (+ `Delete(t)`). Вернуть длину.

`GAME_EVENT_RENDER_HEX_MAX` (дефолт 48) — `#define` в `.h`, ограничивает hex-дамп (bytes/unknown),
чтобы render влезал в 512Б слот. Транзиентные malloc'и cJSON (build+print) освобождаются в том
же вызове — приемлемо для devapi-debug (не release; §E3.13 отступление 2). Чтение payload'а —
ВСЕГДА `memcpy` в выровненную локаль (портируемо, `-Wcast-align`-clean).

---

## E3.5 `game_events_devapi.{c,h}` — реестр + ринг + рекордер + команда (devapi-gated)

Весь TU под `#if NT_DEVAPI_ENABLED` … `#endif` (при OFF — пустой объектник, как `game_save_devapi.c`).

### `game_events_devapi.h` (полностью)
```c
#ifndef GAME_EVENTS_DEVAPI_H
#define GAME_EVENTS_DEVAPI_H

#if NT_DEVAPI_ENABLED

#include "game_event_desc.h" /* game_event_desc_t */

/* Register a fragment's generated descriptor table (<frag>_ev_descs / _count) into the
   tail's hash->desc lookup. Call once after nt_hash_init (type hashes computed from
   desc->name). Conductor wiring (main.c) — the generator stays frozen. Duplicate
   type-hash across descriptors => debug assert (event §1 collision guard). */
void game_events_devapi_register_descs(const game_event_desc_t *const *descs, int count);

/* RECORD-phase recorder (render-at-copy). Walks game_event_log(), renders each event by
   descriptor into the fixed ring. Call ONCE per frame from game_features_record (arena
   alive). No-op until game_events_register_devapi() has enabled it. */
void game_events_devapi_record(void);

/* Registers game.events.tail. Call once from devapi_start() after nt_devapi_init(). */
void game_events_register_devapi(void);

#endif /* NT_DEVAPI_ENABLED */
#endif /* GAME_EVENTS_DEVAPI_H */
```

### `game_events_devapi.c` — контракт реализации
Инклюды (под гардом): `"game_events_devapi.h"`, `"game_events.h"` (`game_event_log`,
`game_events_dropped`, `game_event_t`), `"game_event_render.h"`, `"hash/nt_hash.h"`
(`nt_hash64_str`), `"log/nt_log.h"` (dev-warn), `"core/nt_assert.h"`, `"cJSON.h"`,
`"devapi/nt_devapi.h"`, `<string.h>`, `<stdint.h>`, `<stdbool.h>`.

**Config (#define с дефолтами, переопределяемы игрой):**
```c
#ifndef GAME_EVENTS_TAIL_RING_CAP   /* дизайн §4: N=256 */
#define GAME_EVENTS_TAIL_RING_CAP 256
#endif
#ifndef GAME_EVENTS_TAIL_ENTRY_MAX  /* дизайн §4: ≤512Б со срез-маркером */
#define GAME_EVENTS_TAIL_ENTRY_MAX 512
#endif
#ifndef GAME_EVENTS_DESC_REG_CAP    /* максимум зарегистрированных дескрипторов */
#define GAME_EVENTS_DESC_REG_CAP 64
#endif
```

**Реестр hash→desc (file-static):**
```c
typedef struct { uint64_t hash; const game_event_desc_t *desc; } tail_reg_entry_t;
static tail_reg_entry_t s_reg[GAME_EVENTS_DESC_REG_CAP];
static int s_reg_count;
```
`game_events_devapi_register_descs(descs, count)`: для каждого `d = descs[i]`:
`uint64_t h = nt_hash64_str(d->name).value;` (совпадает с `<frag>_ev_<evt>_type().value` — тот же
`nt_hash64_str`); debug-assert на дубль `h` в `s_reg` (коллизия типов, event §1); при
`s_reg_count >= CAP` → `nt_log_warn` + стоп (не крашить). Иначе `s_reg[s_reg_count++] = {h, d};`.
`static const game_event_desc_t *reg_find(nt_hash64_t t)`: линейный скан `s_reg` по `t.value`
(N мал → дёшево); нет → NULL.

**Ринг (file-static):**
```c
static char     s_ring[GAME_EVENTS_TAIL_RING_CAP][GAME_EVENTS_TAIL_ENTRY_MAX];
static uint64_t s_ring_seq[GAME_EVENTS_TAIL_RING_CAP];
static uint32_t s_ring_head, s_ring_count;
static uint64_t s_ring_evicted, s_ring_last_seq;
static bool     s_enabled;   /* set by game_events_register_devapi */
```
`game_events_devapi_record()`:
```c
if (!s_enabled) return;
int n; const game_event_t *log = game_event_log(&n);
/* LOW-7: событий за кадр может быть > CAP; в ринг влезут только последние CAP -> рендерим
   с (n-CAP), не рендерим-и-эвиктим все n. Пропущенные раньше = потери (наблюдаемы в evicted). */
int start = (n > GAME_EVENTS_TAIL_RING_CAP) ? (n - GAME_EVENTS_TAIL_RING_CAP) : 0;
s_ring_evicted += (uint64_t)start;              /* пропущенные до окна = потери */
for (int i = start; i < n; ++i) {
    const game_event_t *e = &log[i];
    const game_event_desc_t *d = reg_find(e->type);
    (void)game_event_render(e, d, s_ring[s_ring_head], GAME_EVENTS_TAIL_ENTRY_MAX);
    s_ring_seq[s_ring_head] = e->seq;
    s_ring_head = (s_ring_head + 1u) % GAME_EVENTS_TAIL_RING_CAP;
    if (s_ring_count < GAME_EVENTS_TAIL_RING_CAP) s_ring_count++; else s_ring_evicted++;
    s_ring_last_seq = e->seq;
}
```
(Проход строго линейный, ОДИН раз/кадр — курсора нет; см. §E3.2.)

**Хендлер `ep_events_tail`** (форма A5; ownership — все cJSON либо вставлены, либо удалены):
```c
static bool ep_events_tail(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    /* MED-4: ОТСУТСТВУЮЩИЙ параметр -> дефолт; ПРЕДОСТАВЛЕННЫЙ, но кривой -> bad_params
       (строгость выровнена с game.state.*; A5 §MED-3 замороженный код). */
    uint64_t since = 0; int limit = GAME_EVENTS_TAIL_RING_CAP;
    const cJSON *ps = cJSON_GetObjectItemCaseSensitive(params, "since_seq");
    if (ps) {
        if (!cJSON_IsNumber(ps) || ps->valuedouble < 0)
            return state_fail(err, "bad_params", "since_seq must be a non-negative number");
        since = (uint64_t)ps->valuedouble;
    }
    const cJSON *pl = cJSON_GetObjectItemCaseSensitive(params, "limit");
    if (pl) {
        if (!cJSON_IsNumber(pl)) return state_fail(err, "bad_params", "limit must be a number");
        int v = (int)pl->valuedouble;
        if (v < 1 || v > GAME_EVENTS_TAIL_RING_CAP)     /* LOW-5: [1,256] inclusive */
            return state_fail(err, "bad_params", "limit out of range [1,256]");
        limit = v;
    }

    cJSON *events = cJSON_AddArrayToObject(result_obj, "events");
    if (!events) return state_fail(err, "internal", "failed to build events array");
    uint64_t next = since; int emitted = 0;
    /* обход СТАРЕЙШИЙ->НОВЕЙШИЙ: oldest index = (head - count + CAP) % CAP.
       offset-курсор (поправка §E3.3): sq >= since (ВКЛЮЧИТЕЛЬНО), next = sq+1 (эхо since, если пусто). */
    uint32_t idx = (s_ring_head + GAME_EVENTS_TAIL_RING_CAP - s_ring_count) % GAME_EVENTS_TAIL_RING_CAP;
    for (uint32_t k = 0; k < s_ring_count && emitted < limit; ++k) {
        uint64_t sq = s_ring_seq[idx];
        if (sq >= since) {
            /* Слот ВСЕГДА валидный JSON (§E3.4) -> cJSON_Parse = ВТОРОЙ гейт валидности.
               cJSON_CreateRaw отвергнут: требует CJSON_RAW-сборки + transport-cJSON_Print и
               слепо пробрасывает возможный мусор в общий ответ; Parse портируем (ядро cJSON). */
            cJSON *obj = cJSON_Parse(s_ring[idx]);
            if (obj) { cJSON_AddItemToArray(events, obj); next = sq + 1u; emitted++; }
        }
        idx = (idx + 1u) % GAME_EVENTS_TAIL_RING_CAP;
    }
    cJSON_AddNumberToObject(result_obj, "next_seq", (double)next);
    cJSON_AddNumberToObject(result_obj, "dropped", (double)game_events_dropped());
    cJSON_AddNumberToObject(result_obj, "evicted", (double)s_ring_evicted);
    return true;
}
```
`state_fail` — тот же 1:1 хелпер, что в `game_save_devapi.c` (static `s_events_err[256]` +
`err->code`/`err->message`; ЛОКАЛЬНАЯ копия в этом TU — не делить со state-TU).

**Регистрация:**
```c
void game_events_register_devapi(void) {
    static const nt_devapi_command_desc desc = {
        "game.events.tail", "game",
        "Tail the per-frame feature event log (render-at-copy ring).",
        "since_seq?, limit?", "events, next_seq, dropped, evicted", "immediate", "none"};
    (void)nt_devapi_register(&desc, ep_events_tail, NULL);
    s_enabled = true;   /* включить рекордер только когда devapi реально стартовал */
}
```
(`s_enabled` гейтит рекордер: без `--devapi` команда не регистрируется → ринг не наполняется,
ноль холостого рендера.)

**Warning-заметки (`-Wconversion`/`-Wshadow`/`-Wformat=2`):** индексы ринга — `uint32_t`, касты
модуля явные; `since_seq`/`seq` — `uint64_t`→`double` в ответе явно; hex — `PRIx64`/`%02x`;
`cJSON_IsNumber`→`valuedouble` касты явные; никаких shadow с `params`/`err`.

---

## E3.6 Проводка: main.c + game_features.c

**MED-2 (все ТРИ call-site'а гейтятся `FEATURE_GAME_STATE && NT_DEVAPI_ENABLED`):** TU
`game_events_devapi.c` компилится только под `FEATURE_GAME_STATE && GAME_DEVAPI_ENABLED`
(наследует блок CMakeLists:131→202→209, §E3.7). `GAME_DEVAPI_ENABLED` и `FEATURE_GAME_STATE` —
НЕЗАВИСИМЫЕ cache-переменные → конфиг `devapi ON + game-state OFF` = undefined reference, если
call-site под одним лишь `NT_DEVAPI_ENABLED`. Зеркалим двойной гейт A5 (main.c:152-154).

### main.c
1. **Include** (после `#include "game_events.h"`, main.c:44):
```c
#if FEATURE_GAME_STATE && NT_DEVAPI_ENABLED
#include "game_events_devapi.h"
#endif
```
2. **Регистрация команды** (в `devapi_start()`, ВНУТРИ блока `#if FEATURE_GAME_STATE
   game_save_register_devapi(); #endif` main.c:152-154 — тот же двойной гейт, что у A5):
```c
#if FEATURE_GAME_STATE
    game_save_register_devapi();
    game_events_register_devapi();   // E3: game.events.tail (+ enables the recorder)
#endif
```
3. **Регистрация дескрипторов** (после `game_ev_register();` main.c:363, ВНУТРИ `#if
   FEATURE_GAME_STATE` :362-364; `game_ev_descs`/`_count` из `game_state_events.gen.h`, уже
   включён main.c:54):
```c
#if NT_DEVAPI_ENABLED
    game_events_devapi_register_descs(game_ev_descs, game_ev_desc_count); // E3: tail descriptors
#endif
```
   (Здесь внешний `#if FEATURE_GAME_STATE` уже активен → эффективный гейт = обе переменные.)
   (**Конструктор-паттерн, генератор-free:** шаблон регистрирует ТОЛЬКО фрагмент `game` — единственный
   с событиями. Игра, добавившая события другому фрагменту, добавляет свою строку
   `game_events_devapi_register_descs(<frag>_ev_descs, <frag>_ev_desc_count)` — как уже делает per-fragment
   `game_save_register_fragment`/`<frag>_ev_register`. `settings` событий не несёт → не регистрируется.)

### game_features.c (закрытие якоря E1 §E1.4 — НЕ структурная правка)
Вверху (двойной гейт MED-2 — TU определён только под `FEATURE_GAME_STATE && GAME_DEVAPI_ENABLED`):
```c
#if FEATURE_GAME_STATE && NT_DEVAPI_ENABLED
#include "game_events_devapi.h"
#endif
```
`game_features_record` (game_features.c:46-48) — заменить якорь-TODO на гейт-вызов:
```c
void game_features_record(World *w) {
    (void)w;
#if FEATURE_GAME_STATE && NT_DEVAPI_ENABLED
    game_events_devapi_record(); /* E3: DevAPI tail — render-at-copy into the ring */
#endif
    /* TODO(E4): analytics recorder (event §6) */
}
```
Рекордер зовётся В фазе RECORD (арена жива, до `frame_reset` main.c:261) — только читает лог
(emit в RECORD = assert, но рекордер не эмитит). `w` игнорируется (лог глобален).

---

## E3.7 CMake (`templates/template/CMakeLists.txt`)

1. **Два TU в game-таргет под `GAME_DEVAPI_ENABLED`** (в блок `if(GAME_DEVAPI_ENABLED)` рядом с
   `src/game_save_devapi.c`, CMakeLists:202-206):
```cmake
    if(GAME_DEVAPI_ENABLED)
        target_sources(${GAME_TARGET} PRIVATE
            src/game_save_devapi.c
            src/game_events_devapi.c   # E3: event-log tail ring + game.events.tail
            src/game_event_render.c)   # E3: descriptor-driven JSON renderer
    endif()
```
   (`src/` уже в include-path :210; оба TU видят `game_events.h`/`game_event_desc.h`/`cJSON.h`.
   Гейт на FEATURE_GAME_STATE наследуется от объемлющего блока — то же, что у `game_save_devapi.c`:
   cJSON линкуется под FEATURE_GAME_STATE :193, оба TU его требуют. §E3.13 отступление 3.)
2. **NT_HASH_LABELS на движковый `nt_hash` в devapi-сборках** (в блок `if(GAME_DEVAPI_ENABLED)`
   уровня линковки transport, CMakeLists:236-249 — `nt_hash` уже существует после
   `add_subdirectory(ENGINE_DIR)` :63):
```cmake
        # E3: readable event type/hash labels for game.events.tail. Consuming-CMake edit
        # (engine tree untouched); devapi-debug preset only -> no collision with native-debug.
        target_compile_definitions(nt_hash PRIVATE NT_HASH_LABELS=1)
```
   (Правка компайл-дефайна движкового ТАРГЕТА из потребителя — НЕ правка движкового ИСХОДНИКА
   (invariant соблюдён; прецедент — движковый тест `tests/CMakeLists.txt` ставит тот же дефайн).
   devapi-debug/native-debug — РАЗНЫЕ пресеты (build/engine/<preset>, CMakeLists:13-18) → метки в
   devapi-сборке, чистый nt_hash в human-сборке, коллизии либ нет. Стоимость: пересборка nt_hash
   для devapi-пресета + label-таблицы (NT_HASH_MAX_LABELS=4096, растяжимо) в BSS — dev-only.)
3. **Ctest `test_game_event_render`** (нативный, БЕЗ devapi; в блок `if(NOT EMSCRIPTEN)` после
   `test_game_events_typed` :396):
```cmake
    # E3: descriptor-driven renderer over the COMMITTED golden mini events + frozen E1
    # transport. Native, no devapi (renderer is pure). Unconditional (golden is committed).
    add_executable(test_game_event_render
        tests/test_game_event_render.c
        src/game_event_render.c
        src/game_state_json.c        # E3: gsj_i64_to_string (i64->string, MED-3)
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        src/game_events.c)
    target_link_libraries(test_game_event_render PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_event_render PRIVATE
        src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_event_render PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_event_render PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_event_render COMMAND test_game_event_render)
```
   (Транзитивные линки — `nt_platform` для `nt_assert` и т.п. — добавить по сообщению линкера.
   `src/game_state_json.c` слинкован ради `gsj_i64_to_string`. **HIGH-1: `nt_hash` — ОДИН таргет
   на configure и НАСЛЕДУЕТ `NT_HASH_LABELS` пресета** (engine binary-dir per-build-dir,
   CMakeLists:63): в **devapi-debug** тест линкует labels-ON `nt_hash`, а `nt_hash64_str`
   АВТО-регистрирует метку (nt_hash.c) → `nt_hash64_label` вернёт имя, НЕ NULL; в **native-debug** —
   labels-OFF → NULL → хекс. Поэтому ассерты hash/type ОБЯЗАНЫ быть **label-agnostic** (ожидание
   вычислять в рантайме через `nt_hash64_label`, §E3.10), НЕ хардкодить хекс — иначе ctest КРАСНЕЕТ
   в devapi-конфиге. Бонус: в devapi-конфиге этот ctest ТРЕНИРУЕТ label-ветку рендера — слепоты
   меток нет.)

**Warning-гейт:** `game_event_render.c` + `game_events_devapi.c` компилятся в game-таргете под
`nt_set_warning_flags`+`-Werror` (CMakeLists:265) в devapi-сборках → самый рисковый код
(оффсет-арифметика, hex, cJSON-ownership) под `-Wconversion`/`-Wformat=2`. Отдельный OBJECT-таргет
НЕ нужен (в отличие от E2 mini — там golden не был в game-таргете; здесь TU прямо в нём).

---

## E3.8 Смоук-бот (tool parity + triple-sync)

### `smoke_bot.py`
- **`REQUIRED_METHODS` (:38-50):** добавить `"game.events.tail"`.
- **Валидатор** (рядом с `validate_game_state`):
```python
def validate_events_tail(tail: Any) -> dict[str, Any]:
    # E3: game.events.tail returns the render-at-copy ring window. The template emits no
    # events by default, so `events` is typically []; validate SHAPE, tolerate empty.
    if not isinstance(tail, dict):
        raise DevApiError(f"game.events.tail returned {type(tail).__name__}, expected object")
    if not isinstance(tail.get("events"), list):
        raise DevApiError("game.events.tail missing 'events' array")
    for key in ("next_seq", "dropped", "evicted"):
        if not isinstance(tail.get(key), (int, float)):
            raise DevApiError(f"game.events.tail missing numeric '{key}'")
    for ev in tail["events"]:  # each rendered event is a self-contained object
        if not isinstance(ev, dict) or not isinstance(ev.get("seq"), (int, float)) \
                or not isinstance(ev.get("type"), str):
            raise DevApiError("game.events.tail event missing seq/type")
    return tail
```
- **`run_smoke`:** добавить `"game.events.tail"` в набор `described` (:174-177) и вызвать+валидировать:
```python
    events_tail = validate_events_tail(game.result("game.events.tail", {}))
```
  положить в `summary["events_tail"] = events_tail`; бампнуть `"schema": "template.devapi_smoke.v4"`.
  (Опрос — READ-only, безопасен на fresh-state прогоне; пустой хвост доказывает, что команда
  зарегистрирована, ринг/рекордер собраны и работают без краша каждый кадр.)

### `smoke_bot_test.py` (ОБЯЗАТЕЛЬНАЯ синхронная правка — offline, НЕ ctest)
- **FakeGame.result (:18-46):** добавить ветку
```python
        if method == "game.events.tail":
            return {"events": [], "next_seq": 0, "dropped": 0, "evicted": 0}
```
  (Ветка `endpoints` :19-20 итерирует `REQUIRED_METHODS` → `game.events.tail` попадёт в фейковые
  endpoints автоматически; `command.describe` :21-22 универсален. `run_smoke`-тест :103-113 не
  упадёт — стаб добавлен.)
- **Новый тест валидатора:**
```python
    def test_validate_events_tail_accepts_empty_and_rejects_bad(self):
        ok = {"events": [], "next_seq": 0, "dropped": 0, "evicted": 0}
        self.assertIs(smoke_bot.validate_events_tail(ok), ok)
        with self.assertRaises(smoke_bot.DevApiError):  # events not a list
            smoke_bot.validate_events_tail({"events": 0, "next_seq": 0, "dropped": 0, "evicted": 0})
        with self.assertRaises(smoke_bot.DevApiError):  # missing numeric field
            smoke_bot.validate_events_tail({"events": []})
```
- (Опц.) в `test_run_smoke_toggles...` :103-113 добавить `self.assertIn("events_tail", summary)`.

**Гейт:** `py -3.12 templates/template/devapi/smoke_bot_test.py` зелёный (offline) И `devapi_smoke`
(ctest, CMake:271) зелёный на devapi-debug (живой рантайм: команда есть, хвост валиден).

---

## E3.9 Канон-идиом бота (тайл-опрос; образец для агентов)
```python
since = 0                     # offset-курсор: 0 отдаёт ВСЁ, включая seq=0 (поправка §E3.3)
while polling:
    tail = game.result("game.events.tail", {"since_seq": since, "limit": 128})
    for ev in tail["events"]:
        handle(ev)            # {seq, tick, type, <поля...>}; since_seq ВКЛЮЧИТЕЛЬНО
    since = tail["next_seq"]  # = seq последнего + 1 (следующий оффсет); без дублей/потерь
    if tail["evicted"] or tail["dropped"]:
        warn("event backlog lost — poll faster or raise caps")
```

---

## E3.10 Тесты `test_game_event_render.c` (Unity, ctest, нативно без devapi)

`setUp`: `nt_hash_init(NULL)` (однажды, идемпотентно) + `game_events_init()`; `tearDown`:
`game_events_shutdown()`. Фаза EMIT (дефолт после init). Эмит через golden mini emit-хелперы
(`mini_emit_cell_spawned`/`mini_emit_ticked`), рендер через `game_event_render`, ассерты по
`cJSON_Parse` результата и чтению полей.

**ВАЖНО (HIGH-1): ассерты hash/type — LABEL-AGNOSTIC.** `nt_hash` наследует `NT_HASH_LABELS`
пресета (§E3.7): в devapi-debug метки ВКЛючены и `nt_hash64_str` авто-регистрирует имя →
`type`/`kind` вернут ИМЯ, не хекс; в native-debug — хекс. Ожидание вычислять в рантайме:
`const char *l = nt_hash64_label(h); const char *want = l ? l : hexstr(h);` и сравнивать с ним.
НИКОГДА не хардкодить `"0x…"`. (Это делает ctest в devapi-конфиге тренажёром label-ветки.)

1. **rich typed render:** `mini_emit_cell_spawned(42, 3.5, nt_hash64_str("Epic"), true, "hello",
   {1,2,3}, 3)`; `game_event_log(&n)` n==1; `game_event_render(&log[0], &mini_ev_cell_spawned_desc,
   buf, 512)` (desc — extern из gen.h); распарсить buf:
   `strcmp(type, want_type)==0` (label-agnostic: `nt_hash64_label(mini_ev_cell_spawned_type())`
   или хекс); **`total` == СТРОКА `"42"`** (MED-3: i64→строка, `cJSON_IsString`, `strcmp`, НЕ
   число!); `rate==3.5` (число); `urgent==true`; `label=="hello"`; `kind` == label-agnostic
   строка (`nt_hash64_label(nt_hash64_str("Epic"))` или хекс); `blob` = объект `{size:3, hex:"010203"}`;
   `seq`/`tick` присутствуют как ЧИСЛА.
2. **scalar-only:** `mini_emit_ticked(7)`; render с `&mini_ev_ticked_desc`; `count==7` (INT → число);
   `type` label-agnostic == `"mini.ticked"`|хекс.
3. **unknown (desc==NULL):** сырое событие `game_event_emit(nt_hash64_str("raw.x"), payload, size, align)`;
   `game_event_render(&log[i], NULL, buf, 512)` → `unknown==true`, `size==e->size`, есть `hex`;
   `type` == label-agnostic (`nt_hash64_label(nt_hash64_str("raw.x"))` или хекс — в devapi-конфиге
   `nt_hash64_str` уже зарегистрировал `"raw.x"` как метку, поэтому ожидание ВЫЧИСЛЯТЬ, не хардкодить).
4. **usечение:** сконструировать событие/desc с очень длинной string-полем (> 512Б payload —
   через `mini_emit_cell_spawned` с длинным `label`, но ≤ `GAME_EVENT_EMIT_MAX`), render с
   `cap=512` → результат ВАЛИДНЫЙ JSON, содержит `truncated:true` ЛИБО влезает целиком; парс успешен.
   (Малый `cap`, напр. 64, форсирует фолбэк дёшево: `game_event_render(e, desc, buf, 64)` →
   `{seq,tick,type,truncated:true}`, парс успешен.)
5. **всегда валидный JSON:** для каждого кейса `cJSON_Parse(buf) != NULL` (инвариант §E3.4).
6. **bounds/robustness (defensive):** render события, где `desc` заявляет поле за `e->size`
   (напр. подсунуть `&mini_ev_cell_spawned_desc` событию `ticked` меньшего размера) → НЕ падает,
   выдаёт валидный JSON (bounds-check пропускает/обнуляет поля). (Advisory, если хрупко — снять.)

**Компиляционный смоук:** сборка `templates/template` в **native-debug** (новые TU НЕ входят —
проверка что game_features.c гейт-строка под OFF компилится пусто), **devapi-debug** (оба TU +
рекордер-строка + регистрация компилятся warning-clean), **wasm-devapi-debug** (КОМПИЛЯЦИЯ новых
TU; полный линк красный на HEAD — движок, вне E3). `--capture` прогон devapi-debug не падает
(рекордер гоняется каждый кадр с пустым/ненулевым логом).

---

## E3.11 Критерии приёмки (бинарные)

- [ ] `game_events_devapi.c` + `game_event_render.c` компилятся под devapi-debug +
      wasm-devapi-debug (компиляция TU) warning-clean (`-Werror`+`nt_set_warning_flags`);
      native-debug/release НЕ компилируют их (пустой/отсутствующий TU); game_features.c гейт-строка
      компилится пусто под OFF.
- [ ] **Гейт-независимость (MED-2):** все ТРИ call-site'а E3 (include+register в main.c,
      include+record в game_features.c) под `FEATURE_GAME_STATE && NT_DEVAPI_ENABLED`; конфиг
      `GAME_DEVAPI_ENABLED=ON` + `FEATURE_GAME_STATE=OFF` линкуется БЕЗ undefined reference.
- [ ] `game_events.{c,h}` / `game_event_desc.h` / генератор / golden `*_events.gen.*` — **0 правок**
      (`git diff` пуст по ним).
- [ ] Команда `game.events.tail` зарегистрирована (группа `"game"`); `endpoints`/`command.describe`
      её видят; НЕ мутирует стейт.
- [ ] Рекордер render-at-copy зовётся ОДИН раз/кадр из `game_features_record` в фазе RECORD;
      ринг ФИКСИРОВАН (256×512Б, oldest-evicted), НЕ держит указателей арены между кадрами.
- [ ] `game.events.tail` возвращает `{events[], next_seq, dropped, evicted}`; params СТРОГИЕ
      (MED-4): ОТСУТСТВУЮЩИЙ → дефолт, ПРЕДОСТАВЛЕННЫЙ-но-кривой → `bad_params`+message;
      `limit` клампится `[1,256]` включительно (LOW-5); события — валидные JSON-объекты с
      `{seq,tick,type,<поля>}`; каждое `≤512Б` (усечение → валидный `truncated:true`).
- [ ] Типы полей: bool/int(число)/**i64(СТРОКА, §14 п.8, MED-3)**/float(число)/string(инлайн,
      bounded-NUL)/hash(метка|хекс)/bytes({size,hex}) — покрыто `test_game_event_render`.
      Незарегистрированное событие → `{unknown:true,size,hex,type(метка|хекс)}`. Курсоры
      `seq`/`next_seq`/`tick`/`dropped`/`evicted` — ЧИСЛА. Ассерты ctest label-agnostic (HIGH-1).
- [ ] `error.code` — только замороженный набор `bad_params`/`internal` (A5 §MED-3); описания — в
      `message`.
- [ ] NT_HASH_LABELS=1 на движковый `nt_hash` В devapi-сборках (native+wasm), НЕ в
      native-debug/release; живой devapi-смоук показывает читаемые имена типов/hash-полей (best-effort;
      имена ТИПОВ читаемы и без флага через `desc->name`).
- [ ] `test_game_event_render` зелёный; все прежние ctest (state/save/storage/json/events(+overflow)/
      events_typed/roundtrip/check_mini_state_events) зелёные.
- [ ] **Tool parity (triple-sync):** `game.events.tail` в `REQUIRED_METHODS`; `smoke_bot.py`
      валидирует; `smoke_bot_test.py` (FakeGame-стаб + тест валидатора) зелёный offline;
      `devapi_smoke` зелёный на devapi-debug (живой хвост).
- [ ] Ownership: каждый cJSON в хендлере/рендере ЛИБО вставлен, ЛИБО удалён; транзиентные
      cJSON рендера (`PrintUnformatted`+`Delete`) не текут (ручной ASan/leak-прогон devapi-debug,
      §E3.13 R3).

---

## E3.12 Порядок работ

0. **Baseline:** собрать native-debug + devapi-debug + wasm-devapi-debug (компиляция) на HEAD,
   прогнать все ctest + `smoke_bot_test.py` — зафиксировать зелёный/красный ДО правок (отделить
   предсуществующий красный wasm-линк от регрессий E3).
1. **Рендерер (deep ведёт):** `game_event_render.{c,h}` (§E3.4) — дескриптор-driven, все 7 типов,
   bounds, усечение, unknown-фолбэк. + `test_game_event_render.c` (§E3.10). Прогнать ctest.
2. **DevAPI-TU (deep ведёт):** `game_events_devapi.{c,h}` (§E3.5) — реестр/ринг/рекордер/команда/регистрация.
3. **Проводка (fast):** main.c (include+2 регистрации), game_features.c (гейт-строка), CMake (2 TU
   + NT_HASH_LABELS + ctest) (§E3.6/E3.7).
4. **Смоук triple-sync (fast):** smoke_bot.py + smoke_bot_test.py (§E3.8). Прогнать offline-тест.
5. **Гейт:** сборки native-debug + devapi-debug + wasm-devapi-debug(компиляция); все ctest;
   `smoke_bot_test.py`; `devapi_smoke`; ручной опрос `game.events.tail` через devapi-клиент
   (эмитнуть событие вручную если хочется непустой хвост — вне обязательного гейта); ASan/leak-прогон.

Зависимость: шаг 3 (проводка) требует шагов 1-2 (символы определены). Шаг 4 независим от 1-2 по
коду, но `devapi_smoke`-гейт требует всего.

---

## E3.13 Риски

- **R1 (рендер payload'а по оффсету).** Промах в чтении string/bytes-оффсетов или отсутствие
  bounds-check → мусор/чтение за буфером. Митигация: `memcpy`-чтение + bounds против `e->size`;
  `test_game_event_render` (rich/bytes/unknown/bounds кейсы) + retain-семантика E2 (оффсеты
  payload-относительны — переезда нет). Deep-review чтения полей.
- **R2 (усечение/валидность JSON слота).** Слот ОБЯЗАН быть валидным JSON (хендлер `cJSON_Parse`-ит
  его). Промах усечения → `Parse` вернёт NULL → событие тихо пропадёт из ответа. Митигация:
  усечённый фолбэк = собранный cJSON (гарантированно валиден), тест #4/#5 (`Parse != NULL`).
- **R3 (cJSON-ownership).** Рендер: `PrintUnformatted`+`Delete` в том же вызове; хендлер: `Parse`
  вставляется в массив (владение переходит), при NULL — не добавляется (не течёт); `state_fail`-путь
  не оставляет висячих. Гейт: ручной ASan/leak devapi-debug (cJSON без санитайзера — внимателен
  исполнитель; тот же гейт, что A5 §MED-4).
- **R4 (NT_HASH_LABELS — стоимость/побочки).** Флаг пересобирает движковый `nt_hash` для
  devapi-пресета + label-таблицы в BSS (растяжимо от 4096). Только dev-сборки; hash-вычисление
  НЕ меняется (флаг гейтит лишь label-хранилище, E1 §0 проверил). Риск: рост wasm-devapi BSS —
  приемлемо (debug). Если лид против — снять флаг, хвост остаётся читаем по `desc->name` (только
  hash-значения/сырые события уйдут в хекс).
- **R5 (транзиентный malloc рендера в record-пути).** cJSON build+print per-событие/кадр —
  malloc-churn. Только devapi-debug (не release; release не компилит TU). Ринг-РЕТЕНЦИЯ malloc-free
  (фикс-слоты). Приемлемо (§E3.13 отступление 2). Оптимизация (ручной JSON без cJSON) отвергнута —
  escaping-риск под `-Werror` дороже.
- **R6 (wasm-devapi красный линк на HEAD).** Предсуществующий (движок, доложено). E3-гейт для wasm =
  КОМПИЛЯЦИЯ TU, не линк. Baseline (§E3.12 шаг 0) отделяет от регрессий E3.
- **R7 (i64 > 2^53) — ЗАКРЫТ (MED-3).** Рендер FT_I64 = СТРОКА через `gsj_i64_to_string`
  (§E3.4), парити с `game.state.get` (§14 п.8). Double-потери точности НЕТ. Курсоры
  (seq/next_seq/tick/dropped/evicted) остаются числами — они счётчики сессии, всегда << 2^53
  (правило когерентности §E3.3). Риск снят.

---

## E3.14 Что СОЗНАТЕЛЬНО НЕ входит (E4 / швы) + параллельность

- **Локальная аналитика-писатель + встроенный тип `log {string}`** — **E4** (event §6). E4 садится
  ВТОРОЙ строкой в `game_features_record` (рядом с рекордером E3). `log`-тип E4 будет сырым/типизированным
  событием — хвост E3 отрендерит его generic'ом БЕЗ правок (unknown-фолбэк или дескриптор).
- **Экономический срез `game.economy.*`** (типизированный фильтр поверх хвоста, дизайн §4) — позже;
  E3 даёт базовую ленту.
- **Стрингификация крупных i64 / raw-payload запрос через сеть** — вне наблюдаемости E3.
- **ПАРАЛЛЕЛЬНОСТЬ С E4 (ответ на вопрос оркестратора):** E3 **НЕ трогает генератор и golden** (0
  правок; дескрипторы E2 достаточны). Единственные общие с E4 файлы — `game_features_record`
  (E3 добавляет строку рекордера, E4 добавит строку аналитики — тривиальный merge) и CMake
  (разные target_sources-строки). Генераторного конфликта НЕТ → **E3 и E4 можно вести параллельно**;
  синхронизация только по двум строкам в `game_features_record` + CMake-добавлениям.

---

## E3.15 Вопросы лиду (дефолт применён — спека не блокируется)

- **Q1 [ДЕФОЛТ]. Включать NT_HASH_LABELS в devapi-сборках?** ДЕФОЛТ: **ДА** (native+wasm devapi,
  НЕ в human/release). Почему: devapi = агентская/дебажная сборка, где читаемые имена hash-полей и
  сырых событий ценны; E1/E2 явно отложили решение на E3. Стоимость — пересборка nt_hash для
  devapi-пресета + BSS-таблицы (dev-only). Дешёвый реверс: снять одну CMake-строку — хвост остаётся
  читаем по `desc->name` (деградирует только hash-значения/сырые типы → хекс).
- **Q2 [РЕШЕНО, не вопрос]. i64-поля в JSON — СТРОКА** (не double). Пригвождено лидом:
  state doc §14 п.8 «i64-провод — строка»; `gsj_add_i64` = `AddStringToObject "%lld"`; reader
  отвергает числа >2^53 (game_state_json.c:222-230, 167-203). Число-рендер дал бы `"12345"` из
  `game.state.get` и `12345.0` из `game.events.tail` на ОДНОМ счётчике — ровно та порча, что лид
  законодательно запретил. E3 рендерит FT_I64 через `gsj_i64_to_string` (§E3.4). Курсоры
  (seq/next_seq/tick/dropped/evicted) — ЧИСЛА (счётчики сессии << 2^53); payload-i64 — строки
  (правило когерентности §E3.3).
- **Q3 [ДЕФОЛТ]. Params `game.events.tail` — `since_seq`+`limit` (дефолт) или богаче
  (фильтр по типу, диапазон tick)?** ДЕФОЛТ: **минимум `since_seq`+`limit`** (наблюдаемость, не
  аналитика — фильтрация по типу = E4/economy-срез). Тип-фильтр бот делает клиентски (`ev["type"]`).
- **Q5 [ДЕФОЛТ, MED-4]. Строгость params — толерантная (кривое → дефолт) или строгая
  (кривое → `bad_params`)?** ДЕФОЛТ: **строгая — строгость выровнена с `game.state.*`**
  (ПРЕДОСТАВЛЕННЫЙ, но кривой параметр → `bad_params`+message; ОТСУТСТВУЮЩИЙ → дефолт). Толерантный
  вариант ОТВЕРГНУТ ради консистентности DevAPI-поверхности (боты матчат `error.code`; расхождение с
  A5-моделью = сюрприз). Обратимо тривиально.
- **Q4 [ДЕФОЛТ]. Дом вызова рекордера — `game_features_record` (дефолт) или прямо в main.c frame()?**
  ДЕФОЛТ: **`game_features_record`** — дизайн E1 §E1.4 явно разместил «DevAPI tail E3» там (якорь-TODO);
  «рекордеры одной строкой в record-список» (E2 §E2.13). Альтернатива (main.c frame() после
  `game_features_record`) держала бы game_features.c без devapi-инклюда, но противоречит явному
  размещению дизайна. Обратимо тривиально.

Все дефолты консервативны, обратимы, не блокируют исполнение.

---

## E3.16 Отступления от буквы дизайна (с обоснованием)

1. **Рендерер вынесен в отдельный ЧИСТЫЙ TU `game_event_render.{c,h}` (не внутри devapi-TU).**
   Дизайн §4 говорит «рекордер рендерит» — не диктует раскладку. Вынос делает самый рисковый код
   (дескриптор-driven рендер) юнит-тестируемым НАТИВНО без `nt_devapi` (`test_game_event_render`).
   Devapi-TU = тонкая glue (реестр+ринг+команда). Чистое разделение риск/glue.
2. **Рендер через cJSON с транзиентным malloc (не ручной JSON, не reserve-примитив).** Корректный
   escaping строк/имён под `-Werror` дороже сделать вручную; ретенция ринга остаётся malloc-free
   (фикс-слоты — дизайн §4). Транзиентный build+print освобождается в вызове; только devapi-debug.
3. **Хвост-TU гейтится FEATURE_GAME_STATE && GAME_DEVAPI_ENABLED (наследует блок
   `game_save_devapi.c`), хотя логически универсален.** Причина: нуждается в cJSON (линкуется под
   FEATURE_GAME_STATE) и в `game_ev_descs` (генерится под FEATURE_GAME_STATE). Шаблон всегда несёт
   FEATURE_GAME_STATE=ON. Развязка (линковать cJSON под чистым GAME_DEVAPI_ENABLED) — лишний скоуп;
   следую паттерну A5.
4. **Реестр дескрипторов hand-wired в main.c (не расширение `<frag>_ev_register` в генераторе).**
   Дизайн §4 «рекордер рендерит по дескрипторам» — не диктует, КАК рекордер находит дескриптор.
   Hand-wire (`game_events_devapi_register_descs(game_ev_descs, ...)`) держит ГЕНЕРАТОР ЗАМОРОЖЕННЫМ
   (мандат E3) и следует explicit-calls философии агрегатора (feature_arch §5, hook-таблицы
   отвергнуты). Расширение `<frag>_ev_register` регистрацией descs потребовало бы golden-регена всех
   фрагментов — избегаем.
5. **NT_HASH_LABELS включён в E3 (build-config), не в E1/E2.** Прямо по плану: E1 §E1.9 / E2 §E2.13
   отнесли флаг к E3 (когда лента начнёт показывать метки). Правка — CONSUMING CMake, движок не
   тронут (invariant).
