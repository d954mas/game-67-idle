---
id: T0328
title: "State: shell persistence toolkit — хирургия генератора, атомарность, localStorage, фрагменты"
status: doing
project: P001
epic: E009
priority: P1
tags: [state, persistence, template, codegen]
created: 2026-07-06
updated: 2026-07-09
---

## What

Хирургия features/game-state по итогам жёсткого ревью 2026-07-06
(NEEDS-SURGERY: ядро генератора хорошее, половина фичи мертва).
Синтез ревью: templates/design/reviews_synthesis_2026-07-06.md (§1, §2, §6, §8).

Ключевой вердикт: «фича game-state» растворяется — бэкенд + конверт +
оркестрация + JSON-хелперы + codegen-инструмент = L0 shell persistence
toolkit; фрагмент стейта + <id>_state_t + save/load-хуки + локальные
миграции = у каждой фичи.

Решения лида: int64 сейчас (большие счётчики в JSON строкой; идл-bignum =
точка расширения позже); web = localStorage за сигнатурой game_storage;
saved_at (wall-clock записи) в конверте у шелла; примитивы set_level И
reset у прогрессии.

Инкременты (каждый shippable):
S1. АМПУТАЦИЯ (ноль изменений поведения): удалить game_state.c.in (933
    строки, мёртв — маркерный путь не исполняется), затенённые первые
    def'ы в generate_state.py (~700 строк), REQUIRED_FIELDS/
    SOURCE_MARKERS, GAME_STATE_TEMPLATE DEPENDS в обоих CMakeLists,
    сироту games/rb-dark-rpg/state/migrations/v0_to_v1.c + фикстуры
    (чужой контент, не компилится, CMake не включает); починить
    references/workflow.md, review.md, feature.json (обещают миграции,
    которых нет в живом коде).
S2. Атомарность и бэкенды: перенести temp+replace_file (MoveFileEx
    REPLACE|WRITE_THROUGH — уже написан в генерированном game_state_save,
    но подключён к дебажному unsafe_path) в game_storage_save_json;
    .bak-слот (~15 строк, фолбэк на load); консолидировать разошедшиеся
    бэкенды (у rb-dark 5 функций + web localStorage) в шелл;
    web-персистентность = localStorage-ветка. POSIX fsync — опционально.
S3. Миграции: код = принятой 3-уровневой модели — пер-фрагментная
    версия + чейн шагов; мёртвую монолитную машинерию удалить.
    [ожидает подтверждения лида Р2]
S4. Шелл-хелперы game_state_helpers.{c,h} (L0: JSON-хелперы +
    транзакционный swap + конверт со schema-id/version/saved_at);
    settings пишет settings_state_t руками; шелл собирает
    {"features":{...}} — один атомарный документ на слот.
S5. Генератор: один generic-путь; режим --fragment <id> эмитит
    <id>_state_t + хуки + пер-фрагментную версию — к items/progression;
    глобальный GameState уходит. [ожидает подтверждения лида Р5]

## Done when

- [x] S1: мёртвый код удалён (~1600 строк), генератор на обеих схемах
      даёт идентичный выход до/после, тесты зелёные, доки фичи честные.
      [A0, 38dc617e]
- [x] S2: краш-тест записи не убивает сейв (temp+rename+bak); web-сборка
      шаблона сохраняет через refresh (localStorage). [A2/A3, web PASS]
- [x] S4: settings-фрагмент руками + шелл-оркестрация; сейв шаблона =
      конверт + {"features":{settings,game}}. [A3+A6, живой фрагмент]
- [x] S3/S5: механика выполнена в A4/A6 (пер-фрагментная версия +
      миграционная таблица + extern-хуки; --fragment, монолит умер);
      живой миграционный шаг/hook-провод тренируется в T0327.

- [ ] E5: `game_events`/`game_analytics` promoted from template root files into
      an L0 reusable `features/game-events` feature pack like `game-state`, with
      README/INSTALL/feature.json, stable public include path, in-place template
      CMake wiring, DevAPI tail + NDJSON analytics tests green, and no behavior
      drift in the existing event log/scorecard output.

## Open questions

- `game_events`/`game_analytics` promotion is now a required remaining slice:
  they should become an L0 reusable `features/game-events` pack like
  `features/game-state`, not stay as anonymous template-root shell files.
  Acceptance: README/INSTALL/feature.json, stable public include path,
  in-place template CMake wiring, DevAPI tail + NDJSON analytics tests green,
  and no behavior drift in the existing event log/scorecard output.

- Р2 (миграции) и Р5 (--fragment тайминг) — у лида, рекомендации в
  синтезе.
- Находка A0: templates/template/state/migrations/v0_to_v1.c ДОКАЗУЕМО
  мёртв (компилится через target_sources, но вызова чейна нет — он жил
  только в удалённом .c.in). Не сирота, поэтому оставлен; удалить при
  S3/A-миграциях.
- РЕШЕНО лидом 2026-07-06 (гэп NEWER vs Р7 из ревью build-спеки):
  NEWER = только версии (format/save_version/v знакомого фрагмента);
  незнакомый ключ ≠ новизна (сирота → warn + грузимся); незнакомые
  ключи features{} сохраняются verbatim при save (round-trip, данные
  не теряются). Retired-списков нет. Внесено: state doc §14 п.16 +
  аннотация Р7/Р13.
- Судьба int-полей: генератору нужен int64-тип (сейчас int32, cap
  999999 в шаблоне) — добавить тип в схему при S5.
- РЕШЕНО лидом 2026-07-07 (утреннее ревью ночных дефолтов, все 4
  пункта обсуждены по очереди):
  1. Сироты в DevAPI get "" — ВКЛЮЧАТЬ отдельной секцией "orphans"
     (вариант б; бот видит полную JSON-структуру блоба, read-only;
     секция опускается когда сирот нет). Реализация follow-up.
  2. wasm-devapi линк — НЕ заводить issue вслепую: лид помнит, что в
     движке это работало → сперва РАССЛЕДОВАНИЕ (движковый харнесс vs
     наш consuming-конфиг: кто включил ASan в _engine-архивах, почему
     EM_JS-символ теряется у нас; прецедент 8c2295d2 — корни были в
     шаблоне). Issue только с доказанным корнем.
     ИТОГ РАССЛЕДОВАНИЯ (2026-07-07): лид был прав, ОБА корня в шаблоне,
     движок чист (его devapi_host зелёный). (1) движок инструментирует
     Debug-модули ASan/UBSan на не-Windows, потребитель обязан нести те
     же флаги → nt_set_sanitizer_flags(game) (решение лида: вариант «а»,
     полная инструментация); (2) контракт web-devapi хоста требует
     экспортов _nt_devapi_web_submit/_nt_devapi_web_poll (они же
     вытягивают EM_JS-объект из архива) + EXPORTED_RUNTIME_METHODS=ccall.
     ПОЧИНЕНО: wasm-devapi-debug линкуется (game.wasm 12.4МБ), бутается
     под ASan headless-Chrome. Issue движку НЕ нужен (ручка opt-out
     NT_ENABLE_SANITIZERS отклонена лидом как спекулятивная). Хвост:
     живой shim round-trip в браузере — вместе с web-упаковкой шаблона
     (пак по HTTP) и будущим web-ботом.
  3. E3 offset-курсор — РАТИФИЦИРОВАН (since_seq = «отдай начиная С
     этого id», next_seq = «с какого спрашивать дальше»).
  4. E4 кап — семантики оставить, кап поднять до штатно недостижимого:
     native 8МБ→256МБ, web-ring 256КБ→1МБ (~7к событий); firehose
     (писать все события) ратифицирован; пересмотр по объёмам T0327.
  Плюс новое решение: лог-зеркало событий — флаг GAME_EVENTS_LOG_MIRROR
  (дефолт OFF, доступен где есть рендерер), формат `[ev] <полная
  JSON-строка рендерера>` (короткой формы нет — envelope это всего
  seq/tick/type, tick вяжет к кадру, seq сшивает с tail/NDJSON).

## Log

- 2026-07-09: Lead corrected the event-spine classification: `game_events` and
  `game_analytics` should become an L0 feature pack like `game-state`, not stay
  as anonymous template shell files. Added explicit remaining acceptance for
  `features/game-events`; T0339 platform SDK event bridging depends on this
  pack rather than on ad hoc root template files.
- 2026-07-07 (ночь): E4 ГОТОВ — ВСЕ ИНКРЕМЕНТЫ ПЛАНА (A0-A6, E1-E4)
  ЗАКРЫТЫ. Аналитика-писатель: game_analytics.{c,h} — пассивный
  подписчик RECORD-фазы, реюз E3-рендерера (0 правок), NDJSON:
  header-строка сессии + событие-в-строку; native = буферизованный
  append в build/analytics/session-<wall>-<pid>.ndjson (посегментный
  mkdir, флаш по порогу, кап 8МБ keep-oldest-stop, override =0); web =
  in-memory ring 256КБ keep-newest-roll + инертный export (localStorage
  запрещён квотой itch); evicted() отдельно от dropped() («0 on
  healthy» — правда). Встроенный fragment-less log-тип
  (game_log.{c,h}, рукописный дескриптор, оба реестра: виден и в
  game.events.tail, и в NDJSON — доказано живьём). FEATURE_GAME_ANALYTICS
  определён всегда (=0/1), дефолт = devapi-семья, flag-not-mute и
  gate-independence доказаны сборками. DevAPI-команды нет — смоук не
  тронут, tool parity не задет. 9/9 ctest ×2 пресета, 4 сборки чистые,
  wasm TU компилятся, живой NDJSON 294 события seq 0→293. Спека 2
  ревью (16 фиксов) + deep-ревью реализации (ACCEPT-WITH-ADDITIONS,
  всё LOW/doc — внесено). Наблюдение: bot-harness гасит процесс
  terminate'ом — shutdown-флаш не бежит под ботом (потеря ≤ порога
  флаша, by design; чистый выход флашит).
- 2026-07-07 (ночь): E3 ГОТОВ: DevAPI-команда game.events.tail —
  фикс-ринг 256×512 render-at-copy (события рендерятся в RECORD-фазе
  при живой арене в самодостаточные JSON-строки; указатели арены через
  кадры не живут), offset-курсор since_seq/limit (амендмент спеки:
  спека-буквальный `seq > since` молча терял seq=0 — вопрос лиду),
  eviction наблюдаема. Чистый рендерер game_event_render.{c,h} по
  E2-дескрипторам (все 7 типов, i64=СТРОКА по §14 п.8, bounds на
  каждое чтение) + юнит-ctest в ОБОИХ пресетах (native=hex-ветка,
  devapi=label-ветка, label-agnostic ассерты). NT_HASH_LABELS=1 на
  nt_hash только в devapi (consuming-флаг, движок нетронут). Все E3
  call-sites под FEATURE_GAME_STATE && NT_DEVAPI_ENABLED
  (gate-independence доказана no-state сборкой). Смоук v4 с
  validate_events_tail. 8/8 ctest ×2, 8 smoke-тестов, 31 py, живой
  прогон с error-путями. Спека 2 ревью + deep-ревью реализации
  (ACCEPT чистый). Остался E4 (спека готова, 2 ревью).
- 2026-07-07 (ночь): A6 ГОТОВ: мультифрагмент доказан вторым живым
  фрагментом settings — НЕГАТИВНЫЙ ГЕЙТ прошёл (game_save.*,
  game_save_devapi.c, generate_state.py ВНЕ диффа: реестр/диспатч/
  генератор универсальны как есть). settings.schema.json (3 громкости)
  → генерённый settings_state_* + рукописная логика по Р9
  (src/features/settings.{c,h}: clamp+mark_dirty; образец hooks-free
  фрагмента — step/hook-провод тренируется в T0327). Вынос settings из
  game-схемы = template-clean-break (§14 п.12), снятые пути в reserved;
  громкости dev-сейва сбрасываются к дефолтам (шипнутых сейвов нет).
  Живьём: get ""={settings,game}, кросс-фрагментный patch fail-isolation
  (побитый фрагмент откачен, второй применён), round-trip оба, старый
  A5-сейв: settings=дефолты, game жив, НЕ NEWER, orphans пусты.
  31 py + 7 smoke-bot + 7/7 ctest ×2; сборки native/devapi/release/
  no-state чистые; wasm TU компилятся (линк — известный движковый
  блокер). Спека 2 ревью + deep-ревью реализации (ACCEPT, дефектов нет).
  Осталось: E3, E4; items/progression = T0327.
- 2026-07-07 (ночь): A5 ГОТОВ (edc44baa4): генерируемый game_state_devapi.c
  умер; рукописный src/game_save_devapi.c — универсальный диспатч 7 команд
  над реестром GameSaveFragment (агрегат get ""/schema, patch пофрагментно
  атомарен: снапшот to_json → откат from_json, доказано живым прогоном;
  reset=game_save_new_game). Провод = паритет A4: имена команд/группа
  байт-в-байт, error.code заморожен {bad_params, internal}. Compat-обёртки
  A2 сняты (последний потребитель). Golden .h диф = ровно 4 строки,
  остальной golden байт-идентичен, devapi-golden удалён. Смоук-бот на
  фрагментных ключах + его юнит-тесты. Спека 2 ревью + deep-ревью
  реализации (ACCEPT-WITH-ADDITIONS, дефектов кода нет); ASan-харнесс
  чист (LeakSan на Windows нет — компенсировано ownership-аудитом).
  30 py-тестов, 7 smoke-bot-тестов, ctest 7/7 native+devapi. −216 строк
  нетто. Осталось: A6 (мультифрагмент), E3, E4.
- 2026-07-07: E2 ГОТОВ: typed events из схем v2 — секция events (провод
  "<fragment>.<event>", типы bool/int/i64/float=f64/string/hash/bytes),
  генерируемые <frag>_state_events.gen.{h,c}: структуры <Frag>Ev<Evt>,
  emit-хелперы с ALIGNED-стейджингом поверх замороженного E1 (overflow:
  assert+warn+NULL), ленивые type-аксессоры, дескрипторы для E3,
  <frag>_ev_register(); рукописный game_event_desc.h; retain-канон через
  выровненный union (не bare uint8_t[] — UB). Спека 2 ревью + deep-ревью
  реализации (ACCEPT-WITH-ADDITIONS: резерв desc/fields + имена событий
  vs descs/desc_count/register, assertRaisesRegex-якоря — внесены).
  29 py-тестов, 7/7 ctest (+test_game_events_typed,
  check_mini_state_events под -Werror), golden game+mini 12 файлов
  одобрен глазами, state-golden байт-идентичен. Осталось: A5, A6, E3, E4.
- 2026-07-07: A4 ГОТОВ: генератор v2 --fragment (неймспейсы, схема v2
  имя-ключи, i64-строки, дескриптор GameSaveFragment из 11 членов,
  миграционная таблица + extern-хуки, legacy-гард с понятной ошибкой,
  transitional 7-командный devapi через шелл, монолит g_game_state
  умер). Golden game+mini одобрен ревью глазами (байт-воспроизводим);
  спека 2 ревью + реализация deep-ревью (ACCEPT); 18 python-тестов,
  6/6 ctest, smoke-бот зелёный без правок, A3-сейв с числовым i64
  грузится. rb-dark = документированный clean-break (гард). Осталось:
  A5 (DevAPI-диспатч над реестром, снятие compat-обёрток), A6
  (мультифрагмент: items/progression), E2-E4.
- 2026-07-07: E1 ГОТОВ: game_events (ФИКСИРОВАННАЯ арена — решение лида,
  поправка в event-доке §2; engine-issue neotolis-engine#266 на generic
  nt_arena_t заведён) + game_features 7 фаз + двухфазный кадр в main.c
  (якорь автосейва закрыт). Спека build_spec_e1 прошла 2 ревью + deep-
  ревью реализации (ACCEPT-WITH-ADDITIONS: тест округления align +
  гейт death-тестов — внесены). 5/5 ctest, 3 конфига, смоук ок.
  Открытие: _Alignof(max_align_t)==8 на MSVC ABI (тесты производные).
- 2026-07-07: A1 ГОТОВ (c8bcbb63): game_state_json 11 gsj_* + 4 i64,
  40 тестов, warning-clean все конфиги. A2 ГОТОВ (aef4a88d): slot API
  7 функций + 3 compat-обёртки (DevAPI жив), атомарность+bak+карантин
  (уникальные имена), web localStorage APP_ID; краш-набор прошёл deep-
  ревью (ACCEPT-WITH-ADDITIONS → 18 тестов, все добивки внесены).
  Попутно: web parity шаблона ПОЧИНЕН (8c2295d2) — wasm-release снова
  линкуется (capture.c мёртвый guard, main.c unused под web, EMSCRIPTEN-
  ветка NT_PRESET_NAME против interleave кэша движка; корни в шаблоне,
  движок чист, issue не нужен). A3 ГОТОВ: game_save конверт+реестр+
  load-автомат (FRESH/LOADED/RECOVERED_BAK/CORRUPT_RESET/NEWER)+debounce
  +export/import+transform-шов+web flush; фрагмент game последним;
  13/13 тестов, 5 конфигов чистые; advisory web-проверка PASS —
  конверт переживает reload в localStorage (исходный web-блокер T3
  ЗАКРЫТ). S2+S4-эквиваленты в Done when выполнены.
- 2026-07-06 (ночь): A0/S1 ампутация ВЫПОЛНЕНА и проверена независимо:
  выхлоп генератора байт-идентичен на обеих схемах, тесты 4/4. Удалено
  ~1538 строк (933 .c.in + 493 генератор + 108 сирота rb-dark + 4 CMake);
  затенённых строк оказалось ~493, не ~700 — часть хелперов реально
  общая. generate_state.py: 2179 → 1686 строк.
- 2026-07-06 (ночь): СТАРТ реализации (лид дал добро). Параллельно
  запущены: doc-sync четырёх дизайн-доков (Opus, чек-листы synthesis §7 +
  event_system §8) и A0/S1 ампутация генератора (Sonnet, критерий =
  байт-идентичный выхлоп). Дальше: build-spec инкремента 1 → A1-A6.
- 2026-07-06 (веч.): полный план v2 готов и прошёл вторую волну ревью:
  features/game-state/references/state_system_design_2026-07-06.md — инкременты A0-A6 там СУПЕРСИДЯТ
  S1-S5 этой карточки; §14 плана = обязательные поправки ревью (ротация
  одним rename + bak-при-загрузке, on_new_game() хук, web save-fail
  статус + probe, экспорт/импорт строкой в первый web-инкремент,
  синхронный visibility-flush, dirty_at=первая пометка, i64-провод,
  правила миграций, clean-break шаблона, честный скоуп генератора).
- 2026-07-06: создана по итогам жёсткого код-ревью generate_state.py
  (двойная реализация, мёртвый .c.in, неподключённые миграции,
  инвертированная атомарность) + индустрия-ревью сейв-систем (web не
  сохраняет = блокер, JSON остаётся, автосейв-топология предрешена:
  debounced-on-dirty + visibilitychange + saved_at).
- 2026-07-06: status fix: in_progress -> doing (invalid enum, board validate)
- 2026-07-07: последний хвост «живой web-shim round-trip» ЗАКРЫТ через
  T0333 (613afce55): tests/web_devapi_check.py PASS — endpoints → 49
  команд, command.describe → дескриптор, всё через window.__devapi.submit
  в headless Chrome. Карточка полностью готова, закрывает лид.
