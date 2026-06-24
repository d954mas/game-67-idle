# Harness-diet рефактор — хэндофф для новой сессии

> Ветка `refactor/harness-diet`. **НЕ мержить в master.** Живой трекер — `REFACTOR_PLAN.md` §0b.
> Этот файл = остаток задач (п.3d, 4–7) + готовый промпт для старта. Удалить, когда рефактор закрыт.

## Сделано (НЕ переделывать) — 14 коммитов этой сессии

- **п.1** мёртвые evidence-валидаторы `validate_art_job`: `873a8ac` (−1099, тест 61→29) +
  `d65181b` (orphan scaffolder/доки) + `4d7a2e5` (план).
- **п.2** `tools/lib` — 4 либы: `9958a5b` `cli.mjs` (`fail`+`isMain`, ×12+×4) · `85c94a8` isMain ×4 ·
  `af26003` `json.mjs` (`readJson`/`writeJsonFile`, ×6) · `a4f670d` `licenses.mjs` · `0e792c1`
  `paths.mjs` (`toPosix`+`relCwdPosix`, ×3). **`args`-токенизатор ПРОПУЩЕН** (21 тул, парсеры
  фундаментально разные модели → «похожая форма», + риск сломать unknown-guard на блок-гейтах).
- **п.3** product_gate/lib — ЧАСТИЧНО: `12455d3` `visual_axes.mjs` · `38ab981` `art_contract.mjs`
  (`loadArtContract` core). `state_matrix`/`llm_json` (`extractJson`) пропущены — по 1 юзеру.

## Метод (работал, держаться его)

1. **Один инкремент = один коммит**, валидируй каждый: `node tools/ai.mjs validate --full` (зелёный).
2. **Состязательная проверка дубля ПЕРЕД слиянием:** общий on-disk-формат/контракт, который
   разойдётся = сливать; просто похожая форма кода = **оставить** (общая либа добавит связность без
   выгоды). Эта дисциплина уже подрезала json/paths/args/state_matrix/llm_json — продолжать так же.
3. После инкремента — **ревью сабагентом** (general-purpose, адверсариально), потом коммит.
4. Recovery-теги: `pre-asset-refactor-2026-06-24`, `pre-history-cleanup-2026-06-24`.

## Рецепт добавления либы (отработан)

- `tools/lib/<name>.mjs` (кросс-катющее) ИЛИ `tools/product_gate/lib/<name>.mjs` (гейт-специфика) —
  крошечный чистый лист (только node builtins; либо зависит лишь от других крошечных чистых либ).
- `+ <name>.test.mjs` рядом.
- Зарегистрировать тест в `tools/pipeline_validate.mjs`: `run("<name> lib tests", ["--test", "tools/lib/<name>.test.mjs"]);`
- Добавить в allowlist `tools/bootstrap/export_base.mjs` (для `tools/lib/*` — по файлу;
  `tools/product_gate` входит как ЦЕЛЫЙ каталог → `lib/` авто-включён, в export_base ничего не надо).
- Миграция: импорт + удалить локальную копию + подрезать ставшие unused импорты (node --check ловит
  over-removal; unused-leftover ловить грепом). Сканировать двойные пустые строки.

## РОВ — не ослаблять (нарушишь — сломаешь)

- **leak-guard** = `restricted_assets_guard.mjs` + `restricted.mjs`. Зависит ТОЛЬКО от крошечных
  чистых либ, **НИКОГДА** от `cli`/`json`/`asset_catalog`. ВАЖНО: `find_assets.mjs` (его
  `parseFrontmatter`) **на цепочке гварда** — значит `find_assets` тоже держать cli/json-free
  (поэтому isMain в `find_assets:285` НЕ мигрирован — намеренно). Его inline `repoRoot` оставить.
- **`capture_screen.ps1` НЕ удалять** — живой не-Windows фоллбэк (`devapi_client.py:427-433`).
- **8 тулов с whitelist arg-парсингом** сохраняют unknown-option guard (если вернёшься к args).
- chroma scalar/numpy twin — удалять только за `chroma_key_alpha_test` + RGBA-фикстур-дифф.
- Платный SaaS для ассетов нельзя. Не трогать: `find_assets`, keyers (`key_matte`+`dual_plate`),
  движковый packer, schema-state codegen, DevAPI smoke, таск-СТОР, цель оркестрации, гейты
  `visual_material_floor`/`repeated_failure_guard`.

## Остаток задач (по порядку приоритета/риска)

### п.7 — мелкие слияния (НИЗКИЙ риск — хороший первый заход)
- tmp-housekeeper: `pruneOldExports` + `tmp_sweep` → один.
- `serve_tunnel` + `serve_gallery` → общий `tools/lib/mime.mjs` **ТОЛЬКО** (mime-карта), НЕ god
  static-serve.
- `ai.mjs` arg-allowlist — единый источник.
- Доки: workflow «искать→download/ingest_archive→accept→preview→pull» в game-asset скилле;
  doc-шаблоны из CLI (`new_prototype`/`export_base`).
- Проверка каждого: `validate --full`.

### п.4 — `devapi/png_io.py` (Python, средний)
- Вынести триплицированный PNG-кодек из `capture_window` / `devapi_client` / `pixel_health`.
- **НЕ удалять `capture_screen.ps1`** (см. ров).
- Проверка: pixel_health + capture_window + DevAPI smoke. (Python — паттерн либы иной, не .mjs.)

### п.5 — split god-файлов (Python, существенный)
- `build_ui_atlas_pack.py` (801 стр) → вынести `atlas_review_labels.py`, **общий с**
  `audit_ui_atlas_pack` (анти-дрейф контракта меток — делать ВМЕСТЕ со split, не потом).
- `audit_source_sheet_intake.py` (886) → component-finder / key-color-scorer / report-writer.
- Это 3 из выживших standalone python-аудит-листьев (с п.1). Проверка: их `_test.py`.

### п.6 — `taskboard/lib.mjs` split (ПОЗДНО, ВЫСОКИЙ blast)
- Импортят `cli` + `server` + `product_gate` + `game_context` — любой дрейф имён сломает много.
- Разбить: `task_store` + `orchestration_policy` + `subagent_packets`. Имена экспортов стабильны
  через **re-export shim** при миграции.
- Проверка: `node tools/taskboard/cli.mjs validate` + `node tools/taskboard/test.mjs`.

### п.3d — свернуть `visual_critique_packet` → `visual_critic_run` emit-mode (РИСКОВАННО)
- `visual_critic_run` уже имеет 2 режима: emit (default) + run (`--model-cmd`). Свернуть
  `visual_critique_packet` (билдер критик-пакета) в эмит-путь `visual_critic_run`, затем удалить тул.
- Удаление тула → правка скиллов (`generated-game-ui-assets`, `game-visual-art-direction`) + доков,
  которые ссылаются на `visual_critique_packet`. Смежно с Фазой 4 #14 (реальный vision-гейт).
- Проверка: `node tools/ai.mjs gate` (рва `visual_material_floor`/`repeated_failure_guard` не трогать).

---

## ПРОМПТ ДЛЯ НОВОЙ СЕССИИ (скопировать)

```
Продолжи рефактор харнесса на ветке refactor/harness-diet (НЕ мержить в master).
UNIX-декомпозиция + дедуп tools/. Планка: «хорошо, расширяемо, декомпозировано, без техдолга».

СНАЧАЛА прочитай: REFACTOR_NEXT_SESSION.md (полный хэндофф — что сделано, метод, РОВ, рецепт либы,
остаток задач п.3d/4/5/6/7) и REFACTOR_PLAN.md §0b. Память: pipeline-audit-2026-06-24.md.
Сделанное (п.1, п.2, п.3a/3b) НЕ переделывать.

ПРИНЦИП: листья «одна задача» + маленькие tools/lib/* и product_gate/lib/* (НЕ god-файл) +
композиция в фасадах/скиллах. Цель — поддерживаемость харнесса.

ЖЁСТКОЕ (см. РОВ в хэндоффе): leak-guard и его цепочка (find_assets) — cli/json-free; capture_screen.ps1
НЕ удалять; 8 whitelist-тулов сохраняют unknown-guard; chroma twin — только за тест+фикстуры; CC0-only.

МЕТОД: один инкремент = один коммит, валидируй каждый (node tools/ai.mjs validate --full). СОСТЯЗАТЕЛЬНО
проверяй «настоящий дубль или совпадение» ПЕРЕД слиянием — общий контракт/формат = сливать; просто похожая
форма = оставить. После каждого шага — ревью сабагентом, потом коммит.

НАЧНИ с п.7 (низкий риск: tmp-housekeeper, serve mime-lib, ai-allowlist, доки), затем по §0b: п.4
(png_io.py), п.5 (split god-файлов), п.6 (taskboard split — поздно/высокий blast), п.3d (свернуть
visual_critique_packet — рискованно, отдельным аккуратным заходом). По одному.
```
