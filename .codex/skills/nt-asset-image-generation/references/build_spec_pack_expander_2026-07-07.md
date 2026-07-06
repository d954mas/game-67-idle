# Build-spec: pack expander — config → jobs.json, sheet-first (T0330)

Status: REVIEWED (dual-review 2026-07-07: философия/simplicity + execution/robustness,
оба ACCEPT-WITH-CHANGES; правки внесены). Владелец: `.codex/skills/nt-asset-image-generation/`.
Источник решения: разбор доклада Declarative Art (TiltShift, youtube y9znnFAQWOQ) +
внутренний ресёрч `ai_studio/assets/canvas/docs/research_genart_workflow_2026-07-03.md`.

## Цель

Один маленький скрипт, закрывающий единственный структурный разрыв: `gen_batch.py`
умеет исполнять `jobs.json`, но никто не генерит его из декларативного конфига.
Экспандер превращает конфиг пака (стиль + шаблон субъекта + оси, включая грейд) в
sheet-джобы с cell-манифестом. Выход адресуем: каждая картинка знает, какие
значения осей её породили.

## Non-goals (границы из философии проекта)

- НЕ DSL: без вложенных wildcard'ов, весов, шаблонизаторов. Плоский JSON +
  `itertools.product`. Тесно → пересматриваем подход, а не расширяем формат.
- НЕ seed-фичи (gpt-image seed не принимает; закрыто в research 2026-07-03).
- НЕ масштаб «1000 на конфиг»: у нас 30–60 с/вызов и платно. Кап обязателен и ГРОМКИЙ.
- НЕ новый review-UI и НЕ автоматизация canvas meta в первой фазе.
- НЕ замена одиночных генераций: для 1 ассета всё остаётся как есть.

## Место

`.codex/skills/nt-asset-image-generation/scripts/expand_jobs.py` — рядом с
`gen_batch.py` (harness, не per-game; games = fixtures).

## Config v1 (плоский JSON)

```json
{
  "pack": "g67-gen-icons",
  "style_prefix": "<verbatim style-блок: style card / T0208 / art_contract>",
  "subject_template": "a {grade} {material} generator building",
  "axes": {
    "grade": ["rusty", "plain", "gilded", "mythic"],
    "material": ["copper", "steel"],
    "shape": ["furnace", "drill", "reactor", "assembler"]
  },
  "sheet": { "vary": "shape", "grid": [2, 2] },
  "background": "magenta",
  "anchor": "games/<id>/design/art/anchor.png",
  "candidates": 1,
  "max_jobs": 12,
  "gen": { "size": "1536x1024", "quality": "high", "model": "gpt-image-2" },
  "out_dir": "tmp/packs/g67-gen-icons"
}
```

Вычтено по ревью (философия): нет поля `schema` (версионирование без второго
потребителя — церемония), нет `object` (дублировал `subject_template`), нет
`config_hash` (скип уже делает `gen_hash`-сайдкар, provenance — `.gen.json` с
полным промптом).

## Семантика развёртки

- `sheet.vary` — ЕДИНСТВЕННАЯ ось, варьируемая внутри листа (по ячейкам,
  row-major). Все остальные оси — «крупные»: их декартово произведение задаёт
  список листов. 1 лист = 1 джоба = 1 вызов генерации (при `candidates` > 1 —
  N джоб на лист, суффикс `__cN` в out; это механизация овергена, наша замена
  seed-воспроизводимости).
- Ячейки: `len(axes[vary])` ≤ rows*cols, иначе громкая ошибка с подсказкой
  сменить grid. Частичная сетка разрешена: манифест перечисляет РОВНО
  `len(axes[vary])` ячеек row-major, промпт явно требует «остальные ячейки —
  пустой фон». Жёсткий потолок vary: 9 (≈9 тайлов на 1024² — предел читаемости).
- Каждая крупная ось обязана иметь слот `{axis}` в `subject_template`;
  отсутствие слота — громкая ошибка (ось, не влияющая на промпт, — ложь конфига).
- Кап: если джоб (листы × candidates) > `max_jobs` — громкая ошибка со счётом
  и перечнем осей (закон no-silent-caps). Дефолт `max_jobs`: 12.
- `gen.size` — whitelist известных валидных: 1024x1024, 1536x1024, 1024x1536;
  иное — громкая ошибка на экспанде, а не на платном вызове.
- `anchor`: файл обязан существовать на момент экспанда (громкая ошибка ДО
  платных вызовов; codex-путь без guard'а упал бы на data_url per-job).
- Санитизация имён: значения осей слугифицируются для путей (`[a-z0-9_-]`,
  lowercase, ≤80 — по образцу `safe_name` из slice_regions.py); кириллица,
  пробелы, слэши в значениях осей НЕ ломают out-пути. Сырые значения живут
  только в `cells[].axes`.

### Детерминизм и границы кэша

Порядок осей = порядок в JSON, порядок значений = порядок в списке; повторный
прогон даёт байт-в-байт тот же `jobs.json`. Из этого границы hash-skip:

- правка ЗНАЧЕНИЯ крупной оси — локальный сброс (out контент-адресован осями,
  чужие листы держат идентичные prompt+out и скипаются);
- правка `vary`-списка, `style_prefix`, `subject_template`, `grid`,
  `background` — сброс ВСЕХ промптов пака (плановая пересборка);
- правка ФАЙЛА anchor — сброс ВСЕГО пака (gen_hash хэширует байты рефа).
  Это осознанно: новый якорь = новый стиль пака.

## background: маппинг из двух миров (блокер обоих ревью)

Конфигный `background` ∈ {magenta, green, transparent} — семантика КЛЮЧА, а не
API-параметр. `--background` в `generate_image.py` принимает только
transparent/opaque/auto, и gen_batch слепо прокидывает поле джобы — magenta в
job-поле уронит argparse. Отображение:

| config.background | [BACKGROUND] в промпте            | job.background | путь |
|-------------------|-----------------------------------|----------------|------|
| magenta           | solid uniform #FF00FF             | (опущено)      | любой; маршрут key_matte |
| green             | solid uniform #00FF00             | (опущено)      | любой; маршрут key_matte |
| transparent       | прозрачность                      | "transparent"  | ТОЛЬКО REST (sk-): codex-путь reject'ит transparent; реролл в gpt-image-1.5 есть только в gen_rest. Экспандер: громкая ошибка «transparent требует REST» |

Дефолт пилота: magenta + codex-путь.

## Сборка промпта листа (наш 7-секционный стиль)

Детерминированная офлайн-сборка тупым литералом (НЕ реюзать
`prompt_assist.expandRecipePrompt` — тот спавнит codex для разворота ОДНОЙ идеи
живой моделью; экспандеру нужна чистая функция, ноль API — ревью подтвердило:
это разные задачи, общий «модуль 7 секций» был бы overbuild).

- `[TASK]` sheet-генерация: NxM grid, по одному объекту на ячейку.
- `[SUBJECT]` `subject_template` с подставленными крупными осями + перечень
  ячеек: `cell 1 (top-left): <vary=val1>; cell 2: ...` — только vary меняется.
- `[STYLE]` `style_prefix` verbatim.
- `[COMPOSITION]` единый масштаб, объект по центру ячейки, широкие поля-гаттеры,
  без перекрытий, без рамок и линий сетки; при частичной сетке — «оставшиеся
  ячейки: пустой фон».
- `[BACKGROUND]` по таблице выше.
- `[CONSTRAINTS]` no text, no labels, no watermark, no grid lines, одинаковое
  освещение во всех ячейках.
- `[OUTPUT]` один лист, все перечисленные ячейки заполнены.

Guard: суммарный промпт > 20 KB — громкая ошибка (argv-запас на Windows ~32k;
штатный лист 3–5 KB).

## Контракт выхода

`<out_dir>/jobs.json` — массив джоб в схеме gen_batch (prompt/out/size/quality/
model/background/input_image/name; background — по таблице) + дополнительные
поля `pack` и `cells`, которые gen_batch игнорирует (build_cmd читает только
известные ключи — проверено ревью, gen_batch.py:33-42):

```json
{
  "prompt": "...", "out": "tmp/packs/g67-gen-icons/grade-rusty__material-copper.png",
  "name": "g67-gen-icons: rusty copper",
  "input_image": ["games/<id>/design/art/anchor.png"],
  "pack": "g67-gen-icons",
  "cells": [
    { "cell": [0,0], "axes": {"grade":"rusty","material":"copper","shape":"furnace"} }
  ]
}
```

`cells[].name` вычтено по ревью: имя нарезанного ассета выводится в момент
нарезки из slug(axes) — потребитель имени появляется только в фазе 2.

## Нарезка (фаза 2): реальная цепочка и недостающее звено

Сегодняшняя цепочка инструментов: `bg_fix` → `regions/detect_regions.py`
(chroma connected-components, row-major сортировка, отдаёт `region_count` и
пиксельные rect'ы) → `slice/slice_regions.py` (режет по явным rect). Ни один
не принимает grid/cell — манифест сам по себе нарезку НЕ ведёт.

Deliverable фазы 2 — маленький маппер: запускает detect_regions на листе,
**жёсткий гейт `region_count == len(cells)`** (слипшаяся/пустая ячейка иначе
сдвигает row-major соответствие и ассет получает ЧУЖИЕ оси — тихая
мис-маркировка, худший исход), затем зипует найденные rect'ы row-major с
`cells[]` → имена из slug(axes) → slice_regions. Несовпадение счёта — лист в
режект целиком (дёшево: detect уже посчитал).

## Фазы

1. **Экспандер + тесты** (чистая функция, ноль API-вызовов).
2. **Smoke-пилот: 2 листа** реального пака (первая тирная семья иконок game-67)
   ПЕРЕД полным прогоном: конфиг → 2 листа → маппер (гейт по счёту) →
   key_matte → нарезка → отбор гейт+лид; прогрессия грейдов читается.
   Если 2x2 не держит сетку/прогрессию — деградация без смены формата:
   `grid:[1,1]` + vary из 1 значения = 1-ассет-на-джобу с anchor.
   Полный прогон пака — только после smoke.
3. **Canvas meta автоматизация** (axes в meta при promote) — ТОЛЬКО если пилот
   покажет реальное трение ручного переноса.

## Тесты (фаза 1)

`expand(config) -> jobs` — чистая функция; unittest рядом со скриптом:
- golden-развёртка малого конфига (полный jobs.json, порядок стабилен);
- повторный вызов идентичен (детерминизм);
- background-маппинг: magenta/green → job без поля background; transparent →
  громкая ошибка на codex-таргете;
- санитизация: кириллица/пробелы/слэши в значениях осей → валидные out-пути,
  сырые значения сохранены в cells[].axes;
- anchor: присутствует → input_image во всех джобах; файла нет → громкая ошибка;
- candidates > 1 → N джоб на лист с __cN;
- ошибки: превышение max_jobs; vary не влезает в grid; vary > 9; крупная ось
  без слота в subject_template; неизвестная ось в sheet.vary; невалидный
  gen.size; промпт > 20 KB.

## Открытые вопросы (остаются на пилот)

- Адхезия сетки gpt-image-2 (2x2 vs 3x3) и читаемость грейд-прогрессии — smoke.
- Anchor одиночного предмета, ведущий мульти-ячеечный лист, — smoke; пилот на
  codex-пути (inline-реф), REST-edits смещён к «правке рефа».
- Magenta-spill на предметах листа: маршрут key_matte/soft_score существует,
  подтвердить на листах.
- Config-файл vs расширение canvas recipe card: v1 = отдельный файл (lean);
  интеграция с картами — после пилота, если захочется.

## Результаты пилота (2026-07-07, пак мечей грейды 00-20)

- 3 конфига × 7 грейдов (3x3, 7 ячеек) → 3 листа → 21 нарезанный ассет;
  гейт по счёту регионов 7/7 на всех листах. Канвас-проект:
  `swords-grade-test-0-20-t0330-pilot-0a9c9b`.
- Адхезия сетки gpt-image-2 на 3x3/7 ячеек: идеальная на 3/3 листах (пустые
  ячейки реально пустые, без текста и линий).
- Грейд-прогрессия читается и внутри листа, и между листами; кросс-листовая
  стилевая консистентность держится на одном verbatim style_prefix БЕЗ anchor —
  anchor-вопрос остаётся открытым (не понадобился на 3 листах).
- Magenta-spill подтверждён на мягких свечениях (грейды с glow/halo):
  key_matte-путь чист для жёстких силуэтов, soft-ассеты требуют re-key через
  `alpha --method corridorkey` (hue180 shim) или dual-plate перед приёмкой.
- Экономика: 3 платных вызова на 21 ассет (вместо 21) — sheet-first оправдан.

## Лог ревью

- 2026-07-07 dual-review (2× независимых Opus): философ — ACCEPT-WITH-CHANGES
  (блокер background-passthrough; вычесть schema/object/config_hash/cells.name;
  DSL-линия, дефолты, отсрочка фазы 3 — подтверждены); инженер —
  ACCEPT-WITH-CHANGES (блокеры: background argparse-crash, transparent на
  codex-пути, отсутствие потребителя у cell-манифеста → маппер фазы 2 с гейтом
  по счёту регионов; majors: санитизация путей, anchor-в-хэше, тихая
  мис-маркировка при слипшихся ячейках). Все правки внесены в этот документ.
