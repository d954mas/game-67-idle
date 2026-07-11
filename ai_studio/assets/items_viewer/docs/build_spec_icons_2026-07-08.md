# Build spec — мини-раунд «иконки предметов» (T0316)

Дата: 2026-07-08. Автор: deep-reasoner. Пресеты: native-debug + devapi-debug
(ctest 17/17 сейчас). Конвенция финализирована лидом 2026-07-08 (T0316 What-блок).

Цель раунда: «видеть предметы с НАСТОЯЩИМИ иконками». 6 демо-предметов шаблона
получают реальные CC0-иконки, упакованные в атлас движка `icons`; вьюер
items_viewer показывает превью КАЖДОЙ иконки из СОБРАННОГО пака; демо-id меняют
форму `icon.gold` → `icons/gold` (атлас/регион).

---

## §1. Скоуп / не-скоуп

В скоупе: 6 иконок-ассетов (source-first, provenance); атлас `icons` в шаблонном
`build_packs.c`; смена 6 `icon_asset_id` на форму «атлас/регион» + перегенерация
`items_catalog.gen.*`; `resolveIcon()` вьюера + честная деградация; расширение
node-тестов вьюера. На момент этой спеки — только `template:template`: закрытый
прототип rb-dark не имел `content/` и позднее был удалён из рабочего дерева.

НЕ в скоупе (осознанно, LEAN):
- Кодоген-разворот строки в 2 хеша (`atlas_hash`,`region_hash`) в C-таблицах —
  нужен, когда игра начнёт РИСОВАТЬ иконки; сейчас не рисует.
- Рендер иконок в самой игре (HUD/инвентарь) — исторический прецедент закрытого
  прототипа rb-dark (`equipment_screen.c` + `nt_ui_image`) доступен в Git-истории
  на коммите `86fab0254`; live-пути к этому прототипу больше нет.
- `icon-link` ops-команда (write-слой) — фаза 2.
- Любые правки движка (`external/neotolis-engine` READ-ONLY). Где упирается в
  движок — фиксирую как «нужен issue», см. §3/§8.
- Build-time сверка icon→pack в ctest — отложено до реального рендера (§5).

---

## §2. Ассеты (source-first, инвариант AGENTS)

Иконки: `gold`, `xp`, `energy`, `potion`, `sword`, `wood`. Согласованный стиль,
128×128 px (RECT, прозрачный фон, мягкие края — типовой icon-set). Куда:
`templates/template/assets/icons/` (рядом с `assets/ui/` — тот же слой «стартовый
шелл шаблона»).

Порядок источников (исполнитель, НЕ в этой спеке):
1. Shared-библиотека:
   `node ai_studio/assets/backlog/storage/search.mjs --query "item icons gold sword potion" --json`.
   Историческая разведка: подходящего 6-сета в галерее/бэклоге не было;
   найденные тогда canvas-эксперименты относились к закрытому rb-dark и не были
   общей live-фикстурой. Ожидаем miss.
2. Свободные CC0: **Kenney «Game Icons» (kenney.nl/assets/game-icons, CC0)** —
   канонический no-attribution источник, прецедент шаблона = Kenney UI Pack
   (`assets/ui/README.md`). В движке (`examples/*/raw/`) — только `icon_bunny.png`,
   не подходит.
3. Генерация — последний ресурс, НО с явным стопом (см. ниже).

**Скачивание (среда, критично):** на этой машине Avast TLS-MITM ломает node/python
TLS. Рецепт: `curl --ssl-no-revoke …` и/или `NODE_OPTIONS=--use-system-ca`
(память «Avast TLS MITM»). Иначе загрузка молча падает.

**Пер-иконочный fallback (реалистично):** `gold`/`potion`/`sword` — в Kenney Game
Icons (CC0) есть. `xp`/`energy`/`wood` могут не найтись чисто в CC0 → допустим
**game-icons.net (CC BY 3.0)**: лицензионно ОК, но provenance ОБЯЗАН зафиксировать
атрибуцию (автор иконки + ссылка). Разнобой стиля Kenney↔game-icons.net —
осознанный компромисс мини-раунда (единый визуальный тон желателен, но не блокер).

**Генерация = СТОП, вопрос лиду.** Если 1-2 не дают иконку — НЕ падать в генерацию
молча: generated меняет `origin` (sourced→generated) и требует визуального гейта
(`ai_studio/quality`), а лид сейчас не у ПК. Остановиться и спросить.

Оформление (инвариант «license/provenance/integrity/origin» на КАЖДЫЙ ассет):
`templates/template/assets/icons/README.md` по образцу `assets/ui/README.md` +
**integrity: sha256 каждого файла** (ui/README интегрити не несёт — здесь
добавляем, инвариант требует). Блок Provenance **условный от фактического
источника**, не хардкод «Kenney CC0»:
- CC0 (Kenney): Origin: sourced; License: CC0 1.0; Author/source: Kenney — Game
  Icons + URL; без атрибуции.
- CC BY (game-icons.net): Origin: sourced; License: CC BY 3.0; **атрибуция
  обязательна** (автор + URL иконки) — иначе гейт 7.
- generated (только с одобрения лида): Origin: generated; + запись визуального гейта.

Пути к исходникам = приватность пак-рецепта, в контракт (items.json) НЕ входят.

---

## §3. Атлас в `build_packs.c` + добыча координат регионов

### 3a. Атлас (код). После `end_atlas("ui")`, перед `finish_pack`:

Шаблон УЖЕ пакует атлас **`ui`** (`build_packs.c:118`; `ASSET_ATLAS_UI` +
регионы `ui/*` в `game_assets.h:29,32-37`). `icons` — ВТОРОЙ атлас в паке; это
прямо влияет на парсер (§3b, фикс: собирать ВСЕ atlas-записи, а не первую).
Опции согласованы с ui-атласом (`build_packs.c:104-117`) — паритет, не молчание;
`padding=2/margin=2/extrude=1` держат debug-обводку в гаттере/extrude-банде, а не
на пикселях иконки:

```c
    // Item icons: SECOND atlas (ui is first). Explicit region names = the
    // authored "icons/<name>" contract half. debug_png=true emits the page PNG
    // the studio items-viewer previews from. allow_transform=false keeps every
    // region an axis-aligned rect so a preview crop is a simple rectangle.
    // Opts mirror the ui atlas (build_packs.c:104-117) for parity.
    nt_atlas_opts_t icons_opts = nt_atlas_opts_defaults();
    icons_opts.shape = NT_ATLAS_SHAPE_RECT;
    icons_opts.allow_transform = false;   // -> region.transform == 0, simple rect
    icons_opts.pixels_per_unit = 1.0F;    // parity with ui atlas
    icons_opts.padding = 2;
    icons_opts.margin = 2;
    icons_opts.extrude = 1;               // outline lands in the extrude gutter
    icons_opts.premultiplied = true;      // affects ONLY the packed texture (ui parity);
                                          // debug-PNG is copied BEFORE premultiply -> straight alpha
    icons_opts.compress = NULL;           // parity with ui atlas (raw RGBA page)
    icons_opts.debug_png = true;          // -> <CMAKE_BINARY_DIR>/pack/icons_page0.png
    icons_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    icons_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    icons_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    icons_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    icons_opts.gen_mipmaps = false;
    nt_builder_begin_atlas(ctx, "icons", &icons_opts);
    static const char *icon_names[] = {"gold","xp","energy","potion","sword","wood"};
    for (int i = 0; i < 6; ++i) {
        nt_atlas_sprite_opts_t o = nt_atlas_sprite_opts_defaults();
        o.name = icon_names[i];
        char path[128];
        (void)snprintf(path, sizeof path, "assets/icons/%s.png", icon_names[i]);
        nt_builder_atlas_add(ctx, path, &o);
    }
    nt_builder_end_atlas(ctx);
```

CMake: добавить 6 путей `assets/icons/*.png` в `DEPENDS` custom-command'а пака
(`CMakeLists.txt:129-130`), чтобы смена иконки триггерила ребилд.

Проверено (движок): ключ региона в кодогене формируется как `"%s/%s"`
атлас/спрайт (`nt_builder_atlas.c:1939`) → `icons/gold` — уже РОДНОЙ формат нашей
конвенции. `sprite_opts.name` явный; RECT + `slice9=0`. `game_assets.h` получит
секцию `ASSET_ATLAS_REGION_ICONS_GOLD ((nt_hash64_t){0x..ULL}) /* icons/gold */`
(хеш = `nt_hash64_str("gold")`, коммент = полный путь; `nt_builder_atlas.c:1943`).
Хеш регион-макроса == `name_hash` региона в блобе пака (обе части хешат ИМЯ
спрайта) — это шов сверки/резолва.

### 3b. Координаты регионов для кропа — РЕШЕНИЕ (обосновано)

Вьюеру нужен ПРЯМОУГОЛЬНИК каждого региона в пикселях страницы. Проверил все три
дешёвых источника — координат НЕ дают:

- **(в) `game_assets.h`** — имена + хеши, но БЕЗ координат. Недостаточно (как и
  предполагалось в задаче).
- **(а) `nt_builder_dump_pack`** — `print_atlas_details` печатает на регион
  `source_w/source_h`, `origin`, `flags`, `vertex_count`, но НЕ упакованный
  x,y/UV-прямоугольник (`nt_builder_dump.c:317-325`). Текст парсить бесполезно.
- **(б) мини-эмит JSON из `build_packs.c`** — после `end_atlas` в `ctx` лежат
  только `atlas_regions` = `{path, resource_id-хеш}` (`nt_builder_atlas.c:1941`);
  placements (x/y/w/h) локальны для `nt_builder_end_atlas` и освобождаются
  (`pipeline_cleanup`). Публичного API опросить прямоугольники из `ctx` НЕТ
  (весь `nt_builder.h` просмотрен). Рецепт координаты добыть НЕ может.

Координаты РЕАЛЬНО существуют в двух местах: (1) в собранном `.ntpack` как вершины
`NtAtlasVertex` с `atlas_u/atlas_v` (нормировка 0-65535 по размеру страницы,
`nt_atlas_format.h:98-100`); (2) через ПУБЛИЧНЫЙ рантайм-ридер движка
`nt_atlas_find_region`/`nt_atlas_get_region` (`engine/atlas/nt_atlas.h`) — тот же
путь, которым рисовал закрытый прототип rb-dark (исторический `equipment_screen.c`
на коммите `86fab0254`). То есть формулировка
«единственный источник» — НЕВЕРНА; выбор — это trade-off, не безысходность.

Почему НЕ рантайм-ридер В ЭТОМ раунде: он требует standup `nt_resource` + НОВОГО
нативного CMake-таргета студийного инструмента (движок менять не надо, но это
отдельная нативная сборка + рантайм-загрузка пака) — тяжелее мини-раунда.

**РЕШЕНИЕ раунда: координаты и пиксели добывает СТУДИЙНЫЙ (JS) парсер в ops-слое
вьюера, НЕ `build_packs.c` и НЕ движок.** Ровно «превью из собранного пака, пиксели
= что видит игра»; весь новый код — в тулинге (tool parity, тесты), движок
нетронут, рецепт остаётся кодом без дополнений. Для RECT + `allow_transform=false`
(⇒ `region.transform==0`, ровно 4 вершины) min/max UV = прямоугольник кропа.
Парсер (в `ops.mjs`/новом `icon_preview.mjs`, Node, есть `fs`):

0. **VERSION-ASSERT ПЕРВЫМ:** `NtPackHeader.version==NT_PACK_VERSION(2)` и
   `NtAtlasHeader.version==NT_ATLAS_VERSION(6)`. Несовпадение → НЕ парсить, вернуть
   `reason:"pack format newer than viewer (pack vN / atlas vM)"` → честный
   плейсхолдер, не молчаливый мусор.
1. `NtPackHeader` (32 б, `nt_pack_format.h:58`, magic `NT_PACK_MAGIC` 0x4B41504E) →
   `NtAssetEntry[]` (24 б, off. 32..сразу за хедером).
2. **СОБРАТЬ ВСЕ записи `asset_type == NT_ASSET_ATLAS (6)`, не первую** (в паке ДВА
   атласа: `ui` и `icons`). По каждой: `NtAtlasHeader` (28 б, `nt_atlas_format.h:36`)
   → пропустить `page_count × uint64` → `NtAtlasRegion[]` (48 б) →
   `NtAtlasVertex[]` (8 б, at `vertex_offset`) → построить ОБЩУЮ карту
   `name_hash → {rect, page_index}`. `ui`-хеши просто не референсятся (безвредны).
3. `px = uv/65535 × page_dim`; `page_dim` — из PNG-хедера страницы. **`page_index`
   ЧИТАТЬ** (сейчас 6×128 лягут на page0, но поле обязательно к чтению — не хардкод).
4. Имя→хеш: распарсить `game_assets.h` (как `nt_builder_dump.c:22-59`:
   `{0x..ULL}` + `/* icons/gold */`), **фильтруя по префиксу `icons/` в комменте**
   (отсечь `ui/*`). Join с `name_hash` региона.
5. **BigInt везде:** `name_hash` 64-битный (0x15E2… > MAX_SAFE_INTEGER). Читать
   `DataView.getBigUint64(off, true)` (LE); из макроса — `BigInt("0x…")`; сравнение
   BigInt-ами. `Number`/`parseInt` молча теряет точность → join тихо промахнётся.

Join имя↔хеш корректен ПО ПОСТРОЕНИЮ: и макрос (`nt_builder_atlas.c:1943`), и
`region.name_hash` хешат ТОЛЬКО имя спрайта (`gold`), не путь.

Где искать выходы: `templates/template/build/<preset>/pack/{game.ntpack,
icons_page0.png}` (`GAME_PACK_DIR = ${CMAKE_BINARY_DIR}/pack`, CMakeLists:108;
debug_png пишется в тот же dir, точный путь `<CMAKE_BINARY_DIR>/pack/icons_page0.png`,
`nt_builder_atlas.c:1533-1544`). `game_assets.h` пишется нативной сборкой в
`templates/template/src/generated/game_assets.h` (GITIGNORED, §4 — не коммит,
существует после нативной сборки). Кандидаты-пресеты перебрать (native-debug,
devapi-debug), взять первый где есть pack+png+hdr. Лукап PNG — **толерантный**
(glob `icons*page*.png`), и **различать причины деградации**:
- ни pack, ни hdr → `"pack not built (cmake --build …/native-debug)"`.
- pack+hdr есть, атлас `icons` в паке есть, но PNG страницы нет →
  `"atlas built but page PNG missing (debug_png off?)"` (≠ «pack not built»).
Это в духе фазы-1 (честные «not connected / unavailable»), без committed-бинарей и
нового генератора.

**Отклонённые/отложенные альтернативы (одной строкой):**
- **Рантайм-ридер движка `nt_atlas_*` (`nt_resource` + нативный студийный таргет)** —
  РЕКОМЕНДУЕМАЯ ДОЛГОСРОЧНАЯ ЗАМЕНА JS-парсера: движок менять НЕ надо, тот же путь,
  что рисует игра; убирает формато-связанную хрупкость (items_viewer = kept
  deliverable харнесса, а бинарный JS-парсер = долгосрочный техдолг). Тяжелее
  мини-раунда — не сейчас. Предпочтительнее движкового issue `dump --json`.
- Движковый `dump --json` / чистый превью-сайдкар — ЭНЖИН-ЧЕЙНДЖ → issue+PR;
  запасной долгосрочный вариант, если нативный студийный таргет не захотят.
- Сайдкар-JSON из `build_packs.c` — рецепт координат не имеет (см. (б)).
- Читать исходные PNG по пути — нарушает «id ≠ путь» и приватность рецепта.
- Атлас-на-иконку — деградирует семантику id, лишний.

---

## §4. items.json + перегенерация + ассерты

Сменить 6 `icon_asset_id`: `icon.gold`→`icons/gold`, `icon.xp`→`icons/xp`, …,
`icon.wood`→`icons/wood` (`templates/template/content/items.json:15-35`).

Перегенерация: CMake custom-command (`CMakeLists:184-200`) регенерит
`src/generated/items_catalog.gen.{c,h}` при смене `items.json` (нативная сборка).
**Эти файлы GITIGNORED** (`.gitignore:43` `templates/*/src/generated/`, `git
ls-files` пуст) — НЕ коммитятся; после сборки `.icon_asset_id = "icons/gold"` в
`gen.c:48…123` появляется локально как build-продукт. Раунд НИЧЕГО из
`src/generated/` в git не кладёт. Баннер генератора уже корректный.

Кто ассертит старые строки — ПРОВЕРЕНО, ломать нечего:
- `test_items_catalog.c` — `icon` НЕ упоминает (grep пуст). ctest не тронется.
- `items_ops_test.py:48` (`f"icon.{id}"`) и `items_ops.py:140` (passthrough) —
  формат `icon_asset_id` НЕ валидируют; фикстуры само-содержащиеся. Не трогать.
- `tests/fixtures/items_bad.json` (`icon.gold`) — фикстура ДРУГИХ провалов
  (namespace и т.п.); формат иконки нерелевантен. Не трогать (опц. выровнять).
- Схема `item_fields.schema.json` — `icon_asset_id: {type:string,required}`, БЕЗ
  паттерна. Слэш-форма проходит без правок. Icon-format валидацию НЕ добавляем
  (LEAN, §5).

---

## §5. Вьюер: `resolveIcon()` + деградация

Разделение как в фазе-1 (ops считает → api отдаёт JSON → страница рендерит;
страница НЕ импортит Node-`ops.mjs`). Значит превью считает ops-слой:

- `ops.mjs` (`loadCatalogView`) добавляет в view поле `icons`:
  `{ page_data_uri, page_w, page_h, regions: { "icons/gold": {x,y,w,h,page_index}, … },
  reason? }`. `page_data_uri` = `data:image/png;base64,…` страницы
  (одна на все 6 кропов — экономно; ~50-150 КБ для локального тула, влезает в JSON,
  НОВЫЙ HTTP-роут не нужен — api.mjs остаётся JSON-only). Промах/несобранный
  пак/чужая версия → `regions:{}` + различимый `reason` (§3b: «pack not built» ≠
  «page PNG missing» ≠ «pack format newer»).
- `site/items.js` — `resolveIcon()` СЕГОДНЯ возвращает строку → `<img>`
  (`items.js:55-73`), canvas-пути НЕТ; это НЕ чистая замена источника, нужен новый
  путь рендера:
  - В `loadCatalog` ПОСЛЕ получения view и ДО `render(view)` — один `await`
    декода страницы: `const img = new Image(); img.src = view.icons.page_data_uri;
    await img.decode();` (сохранить в state). Пусто/reason → пропустить, флаг «нет
    иконок».
  - `renderIconSlot`/`resolveIcon(assetId)` берёт `regions[assetId]`; есть →
    per-item `<canvas w×h>` + `ctx.drawImage(decodedPage, x,y,w,h, 0,0,w,h)` из
    ОБЩЕГО декодированного изображения. **Никакой альфа-математики**: debug-PNG =
    ПРЯМАЯ альфа (premultiply — только в текстур-кодировщике на приватной копии,
    `nt_builder_texture.c:181,262`; debug-PNG копируется ДО,
    `nt_builder_atlas.c:1513`). Деление RGB/alpha выжгло бы края — голый
    `drawImage`.
  - **Обводка = 2px маджента** `{255,0,255,255}` (`nt_builder_atlas.c:245,251`),
    в extrude-гаттере (opts §3a). Подстраховка: либо **2px внутренний инсет** при
    кропе, либо кей-аут точного `0xFF00FF` в canvas. Не 1px.
  - Нет региона → существующий плейсхолдер `?` + подсказка из `reason`.

Тесты (`tests/ops.test.mjs` + новый юнит парсера, чистая функция как `routeIssues`,
без HTTP): фикстура = **реальный ДВУХ-атласный пак (`ui`+`icons`)** + его
`icons_page0.png` + срез `game_assets.h` (обе секции), снятые с настоящей сборки, во
временной папке. Хотя бы одна **полу-прозрачная** иконка (проверить, что кроп НЕ
выжжен, т.е. отсутствие un-premultiply корректно). Проверить: только `icons/*`
регионы попадают в резолв (ui отфильтрован), BigInt-join совпадает, различимые
reason'ы деградации. **Фикстуры items — в СЛЭШ-форме** (`makeItem` в
`ops.test.mjs` эмитит старую точечную `icon.{id}` — обновить на `icons/{id}`).

---

## §6. Гейты (перед коммитами)

1. **native-debug**: `cmake --build templates/template/build/native-debug` —
   регенерит (в gitignored `src/generated/`) `game_assets.h` (+секция
   `ATLAS_REGION_ICONS_*`), `items_catalog.gen.*` и `pack/{game.ntpack,
   icons_page0.png}`. Без ворнингов.
2. **devapi-debug**: тот же билд второго пресета зелёный.
3. **ctest 17/17**: `ctest --test-dir …/native-debug --output-on-failure` — без
   регрессий (иконочные строки нигде не ассертятся, §4).
4. **wasm**: `build_game_packs` собирается только `if(NOT EMSCRIPTEN)`
   (CMakeLists:113) → под wasm пак НЕ пересобирается; `game_assets.h` и пак берутся
   из выхода нативной сборки (пак едет в web КОПИРОВАНИЕМ нативного `game.ntpack`,
   `build_web.sh` + `CMakeLists:436-444`, НЕ через git). **Нативную сборку прогнать
   ПЕРЕД wasm** — это пре-существующее ограничение шаблона, НЕ вводится этим
   раундом. Собрать wasm-пресет, убедиться что новые `#define`-ы компилируются (игра
   их пока не референсит → линк не ломается).
5. **node-тесты вьюера**: `node --test ai_studio/assets/items_viewer/tests/`
   (существующие 15 + новые парсер/деградация зелёные).
6. **Смоук со скрином**: `node ai_studio/studio_shell/server.mjs` →
   `http://127.0.0.1:8765/items` → Template → 6 карточек показывают РЕАЛЬНЫЕ
   иконки (не `?`); headless-скрин (`nt-runtime-automation`) как evidence.
7. **git-инварианты**: `git status external/neotolis-engine` ПУСТ (движок
   нетронут); в диффе — только 6 PNG + `assets/icons/README.md` (provenance+sha256),
   `build_packs.c`, `items.json`, CMake DEPENDS, файлы вьюера; **НИЧЕГО из
   `src/generated/`** (gitignored, §4); никаких платных/нередистрибутируемых.

**Приёмка лида — НЕ плодить вторую.** Дописать пункты раунда в СУЩЕСТВУЮЩИЙ
отложенный чек-лист приёмки фазы 1 (карточка T0316, Log 2026-07-08, 6 пунктов):
(7) 6 карточек с НАСТОЯЩИМИ иконками; (8) соответствие исходникам, чистые края без
ореолов; (9) честная деградация при несобранном паке (различимый reason). Один
проход приёмки, не два.

---

## §7. Нарезка (коммиты)

1. **assets**: 6 иконок-PNG в `templates/template/assets/icons/` + `README.md`
   (provenance/license/integrity/origin, условный блок §2). Source-first по §2.
2. **pack+content**: `build_packs.c` атлас `icons` (§3a) + CMake `DEPENDS` +
   `items.json` 6× слэш-форма (§4). Прогнать нативную сборку локально для гейтов
   1-4 — регенерённые `src/generated/*` GITIGNORED, в коммит НЕ идут.
3. **viewer**: `ops.mjs`/`icon_preview.mjs` парсер пака (§3b) + `view.icons`;
   `site/items.js` canvas-путь резолва + деградация; тесты (§5). Гейт 5-6.

(1 — отдельно, т.к. это внешние ассеты с provenance-ревью; коммитов из
`src/generated/` нет ни в одном слайсе.)

---

## §8. Риски / LEAN

- **Хрупкость JS-парсера пака** (главный риск): дублирует бинарный формат движка в
  JS, связан с `NT_PACK_VERSION=2` / `NT_ATLAS_VERSION=6`. Митигация: version-assert
  ПЕРВЫМ → честный плейсхолдер «pack format newer than viewer» (не молчаливый мусор);
  парсер крошечный, локализован, под фикстур-тестами. **Рекомендуемая долгосрочная
  замена** — не движковый issue, а студийный нативный таргет поверх ПУБЛИЧНОГО
  рантайм-ридера `nt_atlas_find_region`/`nt_atlas_get_region` (`engine/atlas/nt_atlas.h`;
  движок менять не надо), тогда JS-парсер удаляется (§3b).
- **ДВА атласа в паке** (`ui` первый, `icons` второй): парсер, ищущий ПЕРВУЮ
  atlas-запись, схватит `ui` и все иконки промахнутся МОЛЧА. Митигация: собирать ВСЕ
  `asset_type==6` + фильтр `icons/` по комментам `game_assets.h`; фикстур-тест —
  на реальном двух-атласном паке (§5).
- **debug_png = ПРЯМАЯ альфа** (копируется ДО premultiply-кодировщика,
  `nt_builder_atlas.c:1513` vs `nt_builder_texture.c:181,262`): кроп = голый
  `drawImage`, БЕЗ деления RGB/alpha (иначе выжигание краёв). `premultiplied=true`
  в opts влияет ТОЛЬКО на пак-текстуру (паритет с ui). Прошлая версия спеки требовала
  un-premultiply — это была ОШИБКА, исправлено.
- **debug_png рисует 2px обводку** маджентой `{255,0,255,255}` на границе региона
  (`nt_builder_atlas.c:245,251`; с `extrude=1` — в гаттере). Митигация: 2px
  внутренний инсет ИЛИ кей-аут точного `0xFF00FF`. Не 1px.
- **Ephemeral build-tree**: pack/png в `build/<preset>/pack`, `src/generated/*`
  gitignored. Осознанно: честная деградация (различимые reason'ы) вместо
  committed-бинарей/нового генератора — в духе фазы-1.
- **allow_transform=false обязателен**: иначе регион повёрнут/отражён (`transform!=0`)
  и кроп-прямоугольник неверен. Зафиксировано в §3a.
- **Сверка (часть E)**: per-card плейсхолдер+reason УЖЕ показывает промахи визуально.
  Advisory-строку в Summary добавлять ТОЛЬКО если реально ≤3 строки из готового
  `view.icons`; иначе — оверкилл (промахи и так видны). Build-time/ctest сверку НЕ
  добавляем (LEAN, отложено до реального ин-гейм рендера и кодоген-разворота хешей).
- **Провенанс**: CC BY (game-icons.net) требует атрибуции в README (иначе гейт 7);
  generated — стоп и вопрос лиду (§2). Блок provenance условный от источника, не
  хардкод.
