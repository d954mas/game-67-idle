# Build-spec: паки в recipe card — оси как опциональный режим (T0332)

Status: REWORKED (2026-07-07: решение лида «Слить» — НЕ отдельный тип карты,
а расширение recipe card; предыдущая версия прошла dual-review, механика
перенесена, структура изменена; дельта ждёт фокус-ревью).
Владелец: `ai_studio/assets/canvas/`.
Родитель: `.codex/skills/nt-asset-image-generation/references/build_spec_pack_expander_2026-07-07.md` (T0330, done).

## Требование лида (2026-07-07)

Одна система, не отдельная. Видеть конфиги на канвасе и повторять прогоны.
Реф-картинка style card добавляется осознанно и ОБЯЗАНА работать в паках.
Решение: «Слить» — пак это режим recipe card, не третий тип.

## Принцип

**Одна карта — recipe.** У recipe-блоба появляется опциональный `pack`-блок.
Задан → Generate генерит ЛИСТЫ пака в run-группу; не задан → одна картинка,
как сегодня. Никакого нового типа карты: вся обвязка (отрисовка chrome/чипа,
layers panel, inspectorSig, export-фильтр, paste-ремап style_ref) УЖЕ работает
для recipe — ноль новых поверхностей видимости.

Развёртка промптов НЕ реализуется второй раз: оп собирает config и вызывает
`expand_jobs.py` (единственный экспандер; его валидации — vary<=9, кап,
коллизии слагов, слоты `{axis}` — достаются бесплатно; SystemExit
транслируется в понятную ошибку карты).

Стиль и рефы: пак идёт через ТОТ ЖЕ резолв, что одиночная генерация
(ops.mjs:2607-2637) — текст style card уходит экспандеру как `style_prefix`
verbatim ([STYLE]-секция), реф-картинка style card и картинки-члены карты
уходят референсами КАЖДОГО листа. Требование лида про реф-картинку выполняется
по построению, а не отдельной веткой.

## Блоб-дельта

`defaultRecipe` дополняется одним полем:

```js
pack: null   // | {v:1, axes:{}, vary:"", grid:[3,3], max_jobs:12}
```

Решение лида (2026-07-07, вместо ревью-варианта «поля внутри pack»): НЕ
плодить дублирование — `params` РАЗМОРАЖИВАЕТСЯ. `normalizeRecipePatch`
принимает `params` с полями bg_key, n_candidates, size, quality (model —
иммутабелен). Ревью-блокер «bg_key/n_candidates недостижимы» закрывается
корневым образом: они становятся редактируемыми (инспектор/CLI), а не
дублируются в pack. Один ключ-цвет и одно число кандидатов на карту,
потребителей двое: single-путь (bg_key — advisory для позднего cutout, в
генерацию не идёт, recipe_generate.mjs:58-60) и pack-путь (bg_key запекается
в лист [BACKGROUND]-секцией: `#ff00ff`→magenta, `#00ff00`→green, иной hex —
громкая ошибка в packPreview/pack-ветке generate, НЕ на patch-time — тот же
паттерн, что engine-гейт; candidates = n_candidates). Семантика single-пути
этой спекой не меняется.

Остальное пак берёт из СУЩЕСТВУЮЩИХ полей recipe:

| Нужно паку          | Уже есть в recipe                                  |
|---------------------|----------------------------------------------------|
| subject_template    | `prompt` — СЫРОЙ, verbatim (НЕ resolveRecipePromptText: `expanded` в pack-режиме игнорируется и не должен утечь, ops.mjs:2580,2604); `{axis}`-слоты валидирует экспандер; литеральные `{}` в промпте дадут его громкую ошибку — приемлемо |
| style_prefix        | `style_ref` → style card prompt                    |
| референс генерации  | style card ref-картинка + члены карты (как сегодня)|
| size/quality/model  | `params.*` (иммутабельны — как сегодня)            |

`normalizeRecipePatch` расширяется ДВУМЯ полями:
- `pack`: `null` (выключить режим) или объект с axes (object: имя→массив
  непустых строк, порядок ключей сохраняется), vary (строка), grid
  ([rows,cols] int 1..3), max_jobs (int>=1). Неизвестные ключи внутри pack —
  громкая ошибка. ВАЖНО: patch ЗАМЕНЯЕТ pack целиком (recipe мержится
  shallow, ops.mjs:2362) — UI/CLI шлют полный объект; зафиксировать в тесте.
- `params`: частичный объект с bg_key (hex-строка), n_candidates (int>=1),
  size, quality (строки); model в патче — громкая ошибка (иммутабелен).
  Валидация формата bg_key НЕ привязана к pack-режиму (advisory-hex любой);
  пара magenta/green проверяется на preview/generate. `engine` в pack-режиме НЕ гейтится на patch-time
(кросс-полевой отказ зависел бы от порядка правок) — гейт engine=codex живёт
в packPreview и pack-ветке generateFromRecipe (громко); фаза C дизейблит
select адвайзорно. `expanded`/`use_expanded` в pack-режиме игнорируются;
UI фазы C прячет кнопку Expand prompt при заданных осях.

## Опы

### 1. patchRecipe — расширение (журналируется как раньше)
Поле `pack` в патче. CLI: `recipe-set` получает `--axes-json <path>`,
`--vary a`, `--grid RxC`, `--max-jobs n`, `--pack none` (сброс режима).

### 2. packPreview — эфемерное превью (НЕ журналится, блоб не мутирует)
`recipe-pack-preview <id> --group g`. Требует `recipe.pack`. Сборка config:
`{pack: slug(имя карты), style_prefix: <style card prompt | "">,
subject_template: prompt, axes, sheet:{vary,grid}, background: <из bg_key>,
candidates: n_candidates, max_jobs, gen: params, out_dir: <tmp-заглушка>}`
(out_dir обязателен для экспандера; поля out/input_image jobs на канвас-пути
мертвы). Вызов: config во временный файл → `runToolPython(root,
[".codex/.../expand_jobs.py","--config",cfg,"--out",jobs])` → читаем jobs →
`{sheets: jobs/candidates, style_ref_image: bool, jobs:[{name,prompt,cells}]}`.
Лид видит промпты и счёт листов ДО оплаты. Кириллица безопасна (config файлом).
Реализация УЖЕ ЕСТЬ: `expandPack` из фазы A переносится под recipe
(перечитать recipe.* вместо group.pack;警 предупреждение «картинка не
отправляется» УДАЛИТЬ — картинка отправляется, см. Принцип).

### 3. generateFromRecipe — ветка pack-режима
`recipe.pack` задан → вместо одиночного минта:
- громкий гейт engine=codex (здесь и в packPreview, не на patch-time);
- пересобрать config и вызвать экспандер СВЕЖИМ (никакого стейла по
  построению; style/refs резолв — общий код с одиночной веткой;
  subject_template = recipe.prompt ВЕРБАТИМ, без resolveRecipePromptText);
- генерация листов через codex-seam, вне лока; референсы = те же refPaths,
  что собрала бы одиночная ветка;
- минт КАЖДОГО листа отдельным коротким commit ПО МЕРЕ ГОТОВНОСТИ (N листов ×
  30-60с; крах на 3-м листе не теряет два оплаченных; refuseIfHeadMoved —
  толерантность на уровне листа, не всего рана);
- первый лист минтит result-группу рядом с картой, остальные — в неё; ИМЯ
  группы = `<style card name | "no-style">/<vary> <ts>` (UX: несколько ранов
  различимы без раскопок меты); группа несёт МАРКЕР-блоб
  `pack_run = {v:1, cardId, at}` — ТОЛЬКО провенанс (резолв --run, гейт
  Slice-кнопки); экспортной роли нет — экспорт opt-in по `screen`;
- `--run <run_group_id>` — РЕЗЮМ/ДОГЕНЕРАЦИЯ в существующую группу: листы,
  чьи `sheet_axes` уже представлены в группе, ПРОПУСКАЮТСЯ (паритет с
  gen_batch skip-if-exists: убитый по таймауту ран не переоплачивается);
  `--sheet <slug>` — принудительный реген именно этого листа в ту же группу
  (UX: одна кривая градация из 21 ≠ переплата за весь пак; UI фазы C —
  кнопка Regenerate на листе);
- meta листа: `meta.pack = {cardId, at, sheet_axes, cells (полный манифест),
  prompt_snapshot, refs_snapshot, params_snapshot, style_snapshot?}` (паритет
  с meta.recipe одиночной ветки); лист = provenance-anchor своих катов —
  README/скилл фиксируют правило «не удалять листы до промоута катов»;
- `last_run = {at, verdict: ok|partial, run_group_id, failed:[{sheet_axes,
  error}]}` — обновляется ПОСЛЕ КАЖДОГО листа (убитый посреди ран не
  оставляет orphan-группу без last_run; unattended-агент видит, ЧТО не
  сгенерилось) + запись в tool_runs (паритет ops.mjs:2748-2765). CLI-вызов
  документирует таймаут-риск: N×30-60с, агентам — timeout=max и резюм
  через --run.

### 4. packSlice (фаза B)
`recipe-pack-slice <id> --group g [--run <run_group_id>]` — для каждого листа
рана (кандидаты режутся независимо): detect-regions → ЖЁСТКИЙ гейт
`count == len(cells)` (несовпадение = REJECT листа, остальные режутся) →
slice. `sliceRegions` расширяется `perRegionMeta[]` + `targetParentId`
(сегодня: жёсткая meta ops.mjs:4007, своя группа ops.mjs:4024-4047);
slice-группа листа реparent'ится в run-группу. Мета ката МИНИМАЛЬНА:
`meta.pack = {cardId, sheet_element_id, cell, axes}` — своя ячейка + указатель
на лист (полный манифест/промпт там; иначе 21 кат × манифест ≈ 100+КБ).
Журнал: один detect + один slice на лист. Возврат — per-sheet контракт для
unattended-агента: `[{sheet_element_id, verdict: OK|REJECT|MISSING,
region_count, cells_len, cut_ids[]}]`; REJECT обязан называть got/expected —
агент сам решает re-detect/реген без человека.

### 5. Инспектор meta (фаза B, мелкое)
Рендер meta.pack: лист — оси/время/промпт-модал (переиспользовать модал
recipe, inspector.js:1931-2009); кат — ЕГО оси + ссылка на лист.

### Фаза C. renderRecipe — суб-блок «Pack»
Внутри СУЩЕСТВУЮЩЕЙ секции recipe (inspector.js:2406-2511): тумблер/кнопка
«Pack mode»; при recipe.pack — поля axes (JSON-textarea), vary (select из
ключей axes), grid (2x2|3x3), max_jobs; кнопки **Preview pack** (эфемерно:
N листов, промпты; подпись: «единственное честное превью клетки пака —
single-Generate использует другую сборку промпта»), **Generate** (та же
кнопка — ветка по recipe.pack; busy-счётчик «лист k/N»), **Slice pack**
(активна при last_run.run_group_id; single-форма last_run её дизейблит),
**Regenerate** на meta.pack листа (→ --sheet).

UX-находки ревью 2026-07-07 (обязательные к фазе C):
- `recipe.expanded` непустой при включённом pack — inline-баннер: «Pack
  генерит из базового промпта; expanded не используется — перенесите детали
  в промпт или в style card» (иначе тихая деградация и исчезнувшая кнопка
  Expand читаются как поломка).
- axes-textarea: ошибка парсера с ПОЗИЦИЕЙ (строка/столбец), префилл валидным
  скелетом-примером, нормализация «умных кавычек» перед parse; валидация на
  blur.
- bg_key в pack-режиме: mode-aware подсказка под полем («пак: только #ff00ff
  / #00ff00, цвет запекается в лист») и проверка пары на blur, а не только
  на generate.
- engine select задизейблен С ПОДПИСЬЮ «Packs are codex-only in v1».
- Карта показывает строку «last pack: <ts>, N листов, M failed».
- Чекбокс **Screen** — в блоке Position&Size группы (рядом с Visible/Clip,
  inspector.js:2597-2619); у верхнеуровневой видимой НЕ-screen группы —
  строка-хинт «Отметьте Screen, чтобы группа попала в Export project».

## Правки существующего (ПОЛНЫЙ перечень — заметно короче отдельного типа)

- ops.mjs `defaultRecipe`: + `pack:null`; `normalizeRecipePatch`: + поле pack.
- ops.mjs `generateFromRecipe`: ветка pack-режима (п.3).
- ops.mjs `sliceRegions`: + `perRegionMeta`/`targetParentId` (п.4); проверить
  журнальный лейбл для packSlice-коммитов (sliceRegions-лейбл ops.mjs:4025).
- ЭКСПОРТ — ИНВЕРСИЯ НА OPT-IN (решение лида 2026-07-07: «чтобы группа
  считалась экраном и экспортировалась, я явно ставлю галочку»). Группа —
  экран ТОЛЬКО при явном флаге `group.screen === true`. exportProject
  (6217-6218) и visibleScreenCount (inspector.js:2808) фильтруют по флагу;
  все спец-скипы (recipe/style, планировавшийся pack_run-скип) УМИРАЮТ —
  карточки и run-группы просто не отмечены. CLI: `group-set --screen
  true|false`; UI: чекбокс «Screen» в инспекторе группы (фаза C). Миграция:
  одноразовый проход по существующим проектам — `screen:true` всем
  верхнеуровневым видимым группам БЕЗ recipe/style-блоба (сегодняшний экспорт
  сохраняется byte-exact), дальше только руками. `renderGroup` НЕ гардить —
  явный рендер любой группы остаётся штатным. Маркер `pack_run` на run-группе
  ОСТАЁТСЯ, но только как провенанс (резолв `--run`, гейт Slice-кнопки) —
  экспортной роли у него больше нет.
- cli.mjs: флаги recipe-set + верды recipe-pack-preview / recipe-pack-slice.
  CLI-семантика pack-флагов = READ-MODIFY-WRITE (прочитать текущий pack,
  влить присутствующие флаги, отправить полный объект): `--vary` без
  `--axes-json` ОБЯЗАН сохранять axes — тест обязателен (op остаётся
  replace-wholesale).
- ДОКИ КАК DELIVERABLES (DX-ревью: агенты маршрутизируются по ним, не по
  спеке): (1) ai_studio/assets/canvas/README.md — pack-подсекция в Recipe
  card (поля, верды, params теперь редактируемы, screen-флаг групп, правило
  «лист = provenance anchor»); (2) throughput-and-handoff.md — блок «Disk vs
  Canvas pack» (правило выбора: disk = разовый tmp-конвейер → handoff;
  canvas = конфиг живёт на карте, повторы/резюм/style card) + таблица
  маппинга расхождений (bg_key hex ↔ background enum; transparent недоступен
  на канвасе v1; anchor-файл ↔ style-ref-image; grid 1..3; out/input_image
  мертвы) + правило «ВСЕГДА recipe-pack-preview перед generate»; (3)
  .codex/skills/nt-canvas-operations/SKILL.md — строка-указатель «паки
  иконок/тиров на канвасе → recipe card pack mode»; после правок скиллов —
  sync.mjs.
- inspector.js: суб-блок в renderRecipe (фаза C; кнопка Slice pack гейтится
  на `last_run?.run_group_id` — single-форма last_run её дизейблит); рендер
  meta.pack (п.5).
- НЕ нужны (в отличие от отдельного типа): workspace.js (chrome уже рисуется),
  layers_panel.js (чип Recipe уже есть), inspectorSig (group.recipe уже в
  сигнатуре — recipe.pack едет внутри JSON.stringify, проверено ревью),
  paste-ремап (recipe.style_ref уже ремапится; pack своих id-указателей не
  несёт).

## Судьба кода фазы A (отдельный тип, в рабочей копии, не закоммичен)

Пересадка, не переписывание: валидации normalizePackPatch → внутрь
normalizeRecipePatch (pack-поле: axes/vary/grid/max_jobs; background/candidates
НЕ в pack — params размораживается, см. Блоб-дельта);
expandPack → packPreview (чтение из recipe.*, удалить warning про картинку —
картинка ОТПРАВЛЯЕТСЯ, вернуть info-флаг style_ref_image);
УДАЛИТЬ ЯВНО: createPackCard/patchPack/defaultPack/normalizePackPatch как
отдельные; верды pack-create/pack-set/pack-expand; кейсы historyEntryLabel
`createPackCard`/`patchPack` (ops.mjs:4050-4051); нестинг-гарды `parent.pack`
в createRecipeCard (ops.mjs:2305-2307) и createStyleCard (ops.mjs:2460-2462)
— после слияния мёртвые. Тесты pack.test.mjs — переписать на
recipe-расширение (кейсы те же + новые: background/candidates в pack-объекте,
полная замена pack при patch, prompt-verbatim, pack:null выключает режим,
engine-гейт на preview/generate, style ref-image идёт в рефы).

## Non-goals

- Отдельный тип карты (решение лида: слить).
- Персист превью развёртки / механизмы свежести (превью эфемерно).
- hash-skip на канвасе; gemini/both для паков в v1 (codex only; engine
  в pack-режиме валидируется = codex); авто-alpha после slice; форма-редактор
  осей (JSON textarea).
- Отдельный anchor: якорь пака = реф-картинка style card / члены карты
  (общий резолв с recipe).

## Тесты

Паттерны сьюта: платные seam'ы — фейки; expand_jobs.py — реальный
(skip-without-venv, прецедент alpha.test.mjs:99). Кейсы:
- patchRecipe: pack-валидация (grid/vary/axes/max_jobs/unknown), pack:null,
  journal/undo byte-exact, bg_key не из пары hex в pack-режиме — громко;
- packPreview: config-сборка (style_prefix verbatim; bg_key→magenta/green;
  n_candidates; prompt→subject_template), реальный экспандер, эфемерность,
  style_ref_image флаг;
- generateFromRecipe pack-ветка (fake generators): per-sheet commits, partial,
  run-группа, meta листов С refs_snapshot (style-картинка в рефах!), tool_runs,
  last_run; одиночная ветка НЕ регрессирует (recipe.pack null);
- packSlice: гейт, минимальная мета катов, reparent, --run, журнал по листам;
- UI-фаза: скрытие Expand prompt в pack-режиме, счётчик Export.

## Поправка по deep-ревью полного диффа (2026-07-07, SHIP-WITH-FIXES)

`--sheet`/Regenerate — семантика REPLACE, не дубль: forced-реген атомарно
(тем же коммитом) удаляет из run-группы прежний лист с теми же `sheet_axes`
И его slice-подгруппу. Обоснование: реген делают из-за брака листа — его каты
тоже брак; правило «лист = provenance anchor» относится к КАТАМ, которые
живут/продвинуты, а продвинутые за пределы run-группы копии не затрагиваются.
Дополнительно: `--sheet` без `--run` резолвит `last_run.run_group_id`
(громкая ошибка, если его нет) — тихий форк новой группы запрещён; README —
правило «не переименовывать листы до regen/slice» (идентичность форс-джобы —
по имени листа).

## Известный пробел (исполнитель B2, 2026-07-07)

Резюм `--run` дедуплицирует листы по `sheet_axes` (крупные оси без vary) —
при `n_candidates > 1` первый севший кандидат комбо «удовлетворяет» его, и
остальные кандидаты при резюме пропускаются. Для v1 (кандидаты по умолчанию 1)
приемлемо; пакет candidates>1 должен добавить индекс кандидата в идентичность
листа. НЕ чинить попутно.

## Открытые вопросы

- UX (лиду, фаза C): axes JSON-textarea? Generate/Slice раздельно? run-группа?
  (рекомендации: да/да/да — ответ ожидается)
- Smoke: один референс-предмет (реф style card), ведущий мульти-ячеечный лист
  — пилот T0330 не проверял.

## Лог ревью

- 2026-07-07 v1 (отдельный тип): dual-review 2× Opus, оба ACCEPT-WITH-CHANGES
  — механика (мост к экспандеру, per-sheet commits, минимальная мета катов,
  расширение sliceRegions, реальный экспандер в тестах) подтверждена и
  перенесена сюда без изменений.
- 2026-07-07 v2 (слияние в recipe): решение лида; фаза A реализована по v1
  (643/643 тестов) и пересаживается.
- 2026-07-07 фокус-ревью v2 (Opus): ACCEPT-WITH-CHANGES — блокер
  «background/candidates недостижимы через params» (перенесены внутрь
  recipe.pack), протечка run-группы в export/счётчик (маркер pack_run),
  явные удаления при пересадке, engine-гейт на preview/generate, prompt
  verbatim без resolveRecipePromptText, patch заменяет pack целиком.
  Все правки внесены.
- 2026-07-07 два решения лида поверх фокус-ревью: (1) вместо
  background/candidates внутри pack — РАЗМОРОЗИТЬ params (bg_key/n_candidates/
  size/quality патчабельны; корневой фикс вместо дублирования); (2) экспорт —
  инверсия на opt-in флаг `group.screen` (спец-скипы карточек/run-групп
  умирают; одноразовая миграция сохраняет текущее поведение).
- 2026-07-07 UX/DX dual-review (человек + агент, 2× Opus, по запросу лида;
  вердикты: удобно/понятно risky, гибко ok). Внесено: резюм `--run` +
  `--sheet` реген одного листа (закрывает и «переплату за весь пак», и
  таймаут-риск); last_run после каждого листа + failed[]; per-sheet контракт
  packSlice; баннер про expanded; bg_key blur-подсказка; screen-чекбокс с
  хинтом; axes-ошибки с позицией; имена run-групп; README/скиллы как
  deliverables; CLI read-modify-write. Не внесено осознанно: prompt_hash в
  мете ката (lean: правило «лист = anchor» вместо дублирования данных).
