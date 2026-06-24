# План рефактора AI-пайплайна (harness diet)

> Рабочий план ветки `refactor/harness-diet`. Личный харнесс одного лида (Claude+Codex,
> локально). Игры — намеренно одноразовые фикстуры; **продукт — сам харнесс.** Цель: убрать
> фрикцию и карго-культ, декомпозировать контекст, и установить дисциплину схождения, чтобы
> это не стало 5-й итерацией цикла «упростил → снова обросло». Источник чисел — измерение
> Phase 0 (10 агентов) + аудит (16 агентов), скорректированные состязательными линзами и
> решениями лида по ходу.

## 0. Статус исполнения (обновляется по ходу)

**Сделано (ветка `refactor/harness-diet`, всё с ревью сабагентами, quick validate зелёный):**
- `66cd3d8` единый sync Claude+Codex (skills+hooks).
- `9c5187d` Фаза 1 #1+#10+#12 (prose-аудиторы advisory, 3-тировый fast-path).
- `b6ea177` бюджет = end-of-iteration, не during-work блокер (директива лида).
- `1778c82` Фаза 1 #2 (оркестрация advisory на validate, гейт на чекпоинтах).
- `acf373a` Фаза 3: декомпозиция `AGENTS.md` 3651→3214 (Проблема 1).

**КЛЮЧЕВОЙ ИНСАЙТ ИСПОЛНЕНИЯ (всплыл 3 раза):** агент платит ТОЛЬКО за
всегда-загруженный контекст. Единственный такой док — `AGENTS.md`. Всё остальное
(`tools/assets` ~17.6k LOC, `tasks/archive`, триада делегирования, 17 скиллов) —
**on-demand**, его срез даёт **~0 снижения фрикции** (LOC-тщеславие). Поэтому
большие-LOC фазы плана пересмотрены:
- **Фаза 2 (#3-5 удаления):** под дисциплиной «только proven-dead» + «не терять
  задачи» дала ~0 безопасных удалений — и это не снижало бы фрикцию. ЗАКРЫТА как минимальная.
- **#6 (слияние триады делегирования):** доки on-demand (не в HOT_DOC_BUDGETS) →
  ДЕПРИОРИТИЗИРОВАНО (фрикция ~0, риск потерять operator-контент + ратчет).
- **Фаза 5 (#7/#8 срез ассетов ~12k LOC):** on-demand код → фрикция ~0. ОПЦИОНАЛЬНО,
  только если важна чистота кода сама по себе, не ради скорости агента.

**Реальная оставшаяся ценность:** Проблема 2 (Фаза 4 — настоящий vision-гейт против
«дебаг-визуал прошёл как готовый»; нужна активная игра для постройки/теста) +
опц. поджать `quality-validation.md` (on-demand, 3124>2600 строгий).

---

## 0b. Tools-duplication + UNIX review — оставшаяся работа (для следующей сессии)

**Сделано (ветка `refactor/harness-diet`, всё с ревью + `validate --full` зелёный):** sync
Claude+Codex; Фаза 1 (prose-аудиторы/оркестрация advisory, бюджет=end-of-iteration,
3-тировый fast-path); декомпозиция `AGENTS.md` (3651→3214); очистка истории (−170 файлов);
**удаление мёртвого generated-UI пайплайна (~4.5k LOC, gen-UI audit-гейты + python proof-гейты)**;
дедуп `DEFAULT_LIBRARY`/`KIND_DIR` (канон `find_assets`); **`ingest_archive`** (zip/папка →
`_incoming`) + `tools/lib/hash.mjs`; **писатель каталога слит 3/3** (accept/promote/import →
`tools/lib/asset_catalog.catalogFrontmatter`, дрейф `publish` закрыт);
**п.1 — мёртвые evidence-валидаторы `validate_art_job` удалены** (`873a8ac` −1099,
тест 61→29; `d65181b` orphan-cleanup scaffolder+доки). Recovery-теги:
`pre-asset-refactor-2026-06-24`, `pre-history-cleanup-2026-06-24`.

**UNIX-цель:** 3 слоя — листья «одна задача» · маленькие `tools/lib/*` (НЕ god-утилита) ·
композиция в фасадах (`ai.mjs`/`sync.mjs`) + скиллах.

**Оставшийся план (по value/risk; поправки состязательной вшиты):**
1. ✅ **СДЕЛАНО** (`873a8ac`+`d65181b`). Удалены 4 evidence-валидатора +
   helpers (asArrayField/collectCropIds/collectRuntimeAssetIds/REVIEW_ATLAS_PURPOSE) +
   strict-call/final-art-requirement блоки; 32 теста + 3 фикстур-writer'а; спайн оставлен
   (Rect/Margins/Content/Group/GenerationContract/PromptPacket/GenerationRecord/validateJob +
   runtime↔crop matching + final-art provenance readiness). **Поправка состязательной проверки:**
   формулировка «0 продюсеров» была неточной — продюсеры 5/8 evidence (derivation/composition/
   atlas_metadata/slice9_design/source_family_coverage) + tier-оркестратор `run_ui_asset_tier`
   удалены в `0894234`; 3 выживших python-аудита (`audit_source_sheet_intake`/`build_ui_atlas_pack`/
   `audit_ui_atlas_pack`) — НЕсвязанные standalone-листья (вопрос п.5), не job-evidence. Drift из
   `0894234` (scaffolder seed'ил 2 producerless поля; 3 over-claim в доках) убран в `d65181b`.
2. ✅ **СДЕЛАНО — 4 либы** (по одному, ревью+`validate --full` зелёный на каждом):
   `9958a5b` `lib/cli.mjs` (`fail`+`isMain`, ×12+×4 миграций); `85c94a8` isMain idiom;
   `af26003` `lib/json.mjs` (`readJson`/`writeJsonFile`, ×6); `a4f670d` `lib/licenses.mjs`
   (`LICENSE_URLS`); `0e792c1` `lib/paths.mjs` (`toPosix`+`relCwdPosix`, ×3).
   Каждая либа — крошечный чистый лист (node builtins). Ров на каждом шаге: leak-guard и его
   цепочка (`find_assets`/`restricted`) проверены cli/json-free; `find_assets` НАМЕРЕННО исключён
   из isMain-миграции (он на цепочке гварда). **Состязательные правки объёма (по правилу
   «похожая форма → оставить»):** json — слиты только реальные дубли (2 writeJson РАЗНЫЕ:
   file-writer vs stdout-printer — НЕ сливать; readJson divergent по resolve/onError); paths —
   `toPosix` НЕ форс-мигрирован в ~25 bare-сайтов (однострочник в разных контекстах), `findRepoRoot`
   пропущен (единств. walk-up impls = гвард+`find_assets`, оба inline по рву), слит реальный
   3-way дубль `relCwdPosix`.
   **`lib/args.mjs` (токенизатор) — ПРОПУЩЕН** (состязательно): 21 тул с unknown-guard, но парсеры
   ФУНДАМЕНТАЛЬНО разные модели (`new_art_job` = строгий per-flag whitelist с array-флагами;
   `validate_art_job` = generic `--key value` принимает любой ключ). Общий тут только тривиальный
   for-loop; spec (boolean-vs-value/array/aliases/camelCase/defaults) — целиком per-tool. Общий
   токенизатор = тяжёлая config-spec индирекция на 21 тул + реальный риск сломать unknown-guard на
   БЛОКИРУЮЩИХ гейтах (close_slice/review/ai) — ровно предупреждение лида. Чистая «похожая форма»,
   `fail`-разделение уже соблюдено (cli≠args). Если нужен — минимальный opt-in хелпер для НОВЫХ
   тулов без форс-миграции 21 (форвард, как licenses); решение лида.
   **Пропустить/последним:** `lib/text` (slugify — хвосты разные, низкий ROI), `lib/active_concept`
   (крошечный), `lib/frontmatter` (НЕ плодить — `parseFrontmatter` остаётся в `find_assets`),
   `python_runner` (1 caller — преждевременно).
3. **`product_gate/lib`**: VISUAL_AXES (×4) + state_matrix-ридер + art_contract-лоадер (twin в
   `review.mjs`+`visual_critic_run`). Держать `visual_axes` (константы+матем) ОТДЕЛЬНО от
   `state_matrix` (IO) от `llm_json` (парс) — не в один god-файл. Свернуть `visual_critique_packet`
   → `visual_critic_run` emit-mode. Проверка: `node tools/ai.mjs gate` (рва `visual_material_floor`/
   `repeated_failure_guard` не трогать).
4. **`devapi/png_io.py`** — вынести триплицированный PNG-кодек (capture_window/devapi_client/
   pixel_health). **НЕ удалять `capture_screen.ps1`** — это ЖИВОЙ не-Windows фоллбэк
   (`devapi_client.py:427-433`). Проверка: pixel_health + capture_window + DevAPI smoke.
5. **Split god-файлов:** `build_ui_atlas_pack` (801 → вынести `atlas_review_labels.py`, общий с
   `audit_ui_atlas_pack` — анти-дрейф контракта меток, делать ВМЕСТЕ со split, не потом);
   `audit_source_sheet_intake` (886 → component-finder/key-color-scorer/report-writer).
6. **`taskboard/lib.mjs` split** (ПОЗДНО, высокий blast — импортят cli+server+product_gate+
   game_context): task_store + orchestration_policy + subagent_packets; имена экспортов стабильны
   через re-export shim при миграции. Проверка: `taskboard cli validate` + тесты.
7. **Мелкие слияния:** tmp-housekeeper (`pruneOldExports`+`tmp_sweep`→1); `serve_tunnel`+
   `serve_gallery` → общий `lib/mime.mjs` ТОЛЬКО (не god static-serve); `ai.mjs` arg-allowlist
   единый источник. **Доки:** workflow «искать→download/ingest_archive→accept→preview→pull» в
   game-asset скилле; doc-шаблоны из CLI (`new_prototype`/`export_base`).

**Жёсткие поправки (иначе ошибка):** `capture_screen.ps1` НЕ мёртвый; libs = много мелких листьев
не god-`cli.mjs`; whitelist-тулы сохраняют unknown-option guard; `pull` (game-local writer,
restricted-routing) и `bootstrap` (template-scaffolds) — НЕ писатели каталога, не сливать;
leak-guard зависит только от крошечных чистых либ; license-ФАЙЛ = проза (per-caller, оставить),
каталог-FRONTMATTER = общая схема (слито); chroma scalar/numpy twin — удалять только за
`chroma_key_alpha_test` + RGBA-фикстур-дифф. **Метод проверки:** байт-дифф (чистый рефактор) /
parse-check `find_assets.parseFrontmatter` (нормализация) / `validate --full` (реальный ассет-гейт).

---

## 1. Диагноз (почему мы здесь)

Аддитивная кривая улучшений **насытилась**: первые итерации дали скачки (закодировали ценные
инварианты), последние 3-4 — ~0, а накопленная всегда-загруженная поверхность **тормозит каждый
прогон**. Маржинальный гейт теперь стоит больше фрикции, чем приносит качества, потому что
оставшийся запас — в ВЫХОДЕ/ВКУСЕ (процессом не кодируется), а не в ПРОЦЕССЕ (переотстроен).
Подтверждено фактами: 0 гейтов считают качество из пикселей; 69% коммитов — тулинг; цикл
«слишком тяжело → упростил → снова обросло» прошёл 4 раза.

Четыре названные проблемы → куда едут в плане:

| Проблема лида | Фаза |
|---|---|
| Качество выхода стоит | Фаза 6 (освобождённая фрикция → больше итераций визуальной петли) + дисциплина схождения |
| Фрикция / «скорость падает» | Фаза 1 (non-invocation флипы + fast-path) |
| Архитектура харнесса разрослась | Фазы 2,3,5 (мёртвое, доки/контекст, ассеты) |
| Не сходится / осцилляция | Фаза 0-дисциплина (ниже) + Фаза 6 (храповик-закон) |

## 2. Дисциплина схождения (управляет ВСЕМИ фазами)

**Бенчмарк ОТМЕНЁН** (решение лида): число за качество = фальшивая точность (качество
субъективно), а оптимизация против одной задачи = overfitting, усугублённый накоплением памяти.
Вместо него — то, что уже есть:

1. **Объективно «ничего не сломал»** — существующие автовалидаторы зелёные (у каждого шага ниже
   указана команда).
2. **Качество** — глаз лида по **разным** играм. Разные игры каждый раз = защита от подгонки.
   Никаких рубрик/баллов.
3. **Цена (опц., статическая)** — строки горячего контекста + шаги-на-изменение по тирам,
   до→после. Не прогон задачи, просто счётчик. Только если глаз перестанет ловить регресс.
4. **Храповик «DO NOT add» (закон, Фаза 6)** — новый гейт/гард/валидатор/скилл/правило можно
   добавить ТОЛЬКО если (i) он убирает названный, воспроизведённый провал, который глаз+валидаторы
   пропускают, И (ii) парно с удалением/переводом-в-advisory поверхности ≥ равного объёма
   (one-in-one-out). `repeated_failure_guard` — единственный детектор петель, заморожен.

**Метод реза:** вычитание **не-вызовом** (флип в advisory/off-by-default) по умолчанию — обратимо,
без рефактора, не провоцирует ре-аккрецию. Жёсткое удаление — только для доказанно-мёртвого.

## 3. Неприкосновенное и защищённый «ров»

**НЕ резать (решения лида):**
- **Таск-СТОР** (`active/epics/STATUS`, `new/set/show`, evidence-привязка, ядро taskboard CLI) —
  долговременная кросс-сессионная память; host-инструменты эфемерны. Режем только мета-процесс
  ВОКРУГ (71 мета-задача, prose-валидация статуса).
- **Цель оркестрации** — рефакторим механизм (end-gate-на-всё → start-nudge на параллелизуемом +
  host-native + advisory-счётчик), НЕ убираем цель.

**Защищённый «ров» (резать только с сильным обоснованием):** движок+`template`+bootstrap;
`CONVENTIONS.md` анти-god-file; каталог provenance + **leak-guard**; инварианты
fonts-only/engine-API/Y-up; DevAPI screenshot-smoke; секция 6 «DO NOT add»; schema-state codegen
(keep-frozen); `find_assets`/intake-каталог; CC0-source-first (Kenney/Quaternius).

**Отклонено:** маршрутизация ассетов в платный SaaS (Avast TLS-MITM + инвариант no-paid-binaries).

## 4. Уже сделано

- ✅ **Единый sync Claude+Codex** (коммит `66cd3d8`): `tools/hooks_sync.mjs` (канонический
  `HOOK_SOURCE` → `.codex/hooks.json` + `.claude/settings.json` хирургически) + `tools/sync.mjs`
  (зонтик skills+hooks, `--check`) + тесты + проводка в `validate`. Воспроизводит оба файла
  байт-в-байт; закрыта дыра дрейфа хуков. Инструкции остаются на `AGENTS.md`.

## 5. Фазы (порядок: обратимое и дешёвое → дорогое и связное)

Каждый пункт несёт `#ранг` из реестра Phase 0, дельту LOC и команду-проверку. Цель итогов:
**tools ~38.5k→~24.2k LOC, горячие доки 714→~400 строк, блокирующих проверок ~10→6,
скиллы 17→~12, гейт-скрипты 34→~20, мета-задачи 71→1, тиров фрикции 1→3.**

### Фаза 1 — Быстрые победы: non-invocation (низкий риск, обратимо, сразу скорость)
*Главный удар по «скорость падает».*
- **#1** `skills_eval` (699) + `doc_reference_check` → из блокирующего `validate` в `--review`.
  Это presence-lint и link-rot, не судят выход. Флип флага, 0 удаления.
  `node tools/ai.mjs validate && node tools/ai.mjs validate --review`
- **#2** Оркестрация-LABEL-гейт (`taskboard lib.mjs`) → из `exit(1)` в advisory **start-nudge**
  (цель сохраняется, механизм меняется; −270 LOC тестов). `node tools/taskboard/cli.mjs validate && node tools/taskboard/test.mjs`
- **#10** **3-тировая лестница фрикции (fast-path)**: TIER1 спайк/тривиал = 1 проверка + скриншот,
  без packet/report/4-вердиктов; TIER2 = полная церемония только на принятом слайсе; TIER3 =
  визуальный оверлей. +37 строк доков, убирают тяжёлый путь с каждой мелкой итерации.
  `node tools/ai.mjs validate --review && node tools/doc_reference_check.mjs`
- **#12** `workflow_guard` сам пропускается когда нет активной игры (уже это вычисляет), блокирует
  только при активной игре. `node tools/game_context/workflow_guard.mjs`

### Фаза 2 — Удалить доказанно-мёртвое (жёсткое удаление только для husk)
- **#3** Пустой скилл `game-texture-atlas-pipeline` (нет SKILL.md, не в git). 17→16 реальных.
- **#4** `devapi/iterate.py` (69 LOC, 0 ссылок, ничем не импортируется).
- **#5** 71 мета-задача `archive/unassigned` (~3984 LOC процесса-про-процесс) → один
  `CEREMONY-RETIRED.md` + git-тег. **Стор не трогаем.** `node tools/taskboard/cli.mjs validate`

### Фаза 3 — Декомпозиция контекста (ПРОБЛЕМА 1)
*Чинит «много больших MD» + пре-существующий `context_budget` red.*
- **#6** Триада делегирования (`subagent-protocol` 131 + `orchestration-playbook` 169 + секция в
  `agent-workflow`) = 363 строки / 44% горячего пути → один `docs/ai-pipeline/delegation.md` ~60
  строк (7-полевой пакет, handoff, секция-6 «DO NOT add» **дословно**); 10 URL/анекдотов → 3-строчный
  футер. `node tools/ai.mjs validate && node tools/taskboard/cli.mjs validate`
- **НОВОЕ (эскиз лида): декомпозиция `AGENTS.md`** → крошечный спайн (всегда-загружен) + слои
  по ссылке:
  ```
  AGENTS.md (~40 строк): жёсткие инварианты + КАРТА «где что»
  └─ по ссылке: base→docs/ai-pipeline/* | template→CONVENTIONS.md | project→per-game | gates→quality-validation.md
  ```
  Поджать `quality-validation.md` (3124>2600) и два толстых SKILL.md под бюджет.
- **#11** Дублированные правила горячего пути («fails-twice» ×5, validate-matrix ×3) → канон +
  указатели; спайн `AGENTS.md` дословно.
- **#13** Source-first преамбула (копипаст в 7 скиллах) → одна строка-ссылка на инвариант.

### Фаза 4 — Гейты: убить карго-культ, поставить ОДНУ реальную проверку (ПРОБЛЕМА 2)
*Чинит «трачу время на проверки-факты-проверок; дебаг-визуал проходит как готовый».*
- **#14** Кластер визуал-критика: `visual_critique_packet`→emit-mode `visual_critic_run`;
  `visual_rejection_lock`→`review.mjs --reject` (флаги). Оставить `review.mjs` (JSON-контракт для
  `close_slice`) + `visual_critic_run` (единственный путь screenshot→vision-LLM).
- **НОВОЕ: одна РЕАЛЬНАЯ блокирующая проверка.** Корень «дебаг-визуал проходит» — ни один гейт не
  смотрит на пиксели. На закрытии **визуального** слайса `close_slice` ОБЯЗАН прогнать
  `visual_critic_run` (vision-LLM по скриншоту) с конкретным вопросом **«это дебаг/плейсхолдер-арт
  или реальный?»**; FAIL → слайс не закрывается. Не 34 церемонии, а одна, которая ловит твой кейс.
  `node tools/ai.mjs close-slice --help && node tools/product_gate/test.mjs`

### Фаза 5 — Срез ассет-подсистемы (наибольший LOC, больше всего связей — аккуратно)
*Главный удар по «архитектура разрослась» (ассеты = ~46% тулинга).*
- **#7** Мёртвый generated-UI asset-job пайплайн (7 audit-гейтов + tier-orchestrator + pack +
  intake/derivation аудиты + тесты): **~11000→~1650**. Оставить тонкий каркас (new_art_job,
  new_generation_record, plan_source_sheet_prompt, один validate_art_job, proof-renderer,
  normalize_source_sheet_chroma). `node tools/ai.mjs validate --full`
- **#8** Cutout-коллапс: один keyer (`dual_plate_alpha`), убрать `key_matte` (миграция не
  завершена) + `plan_runtime_crops_from_intake`, **починить импорты `assemble/`** в том же коммите.
  Движковый packer защищён. `node tools/ai.mjs validate --full`
- **#9** Слить 7 ассет/визуал-скиллов → ~3-4 по швам, которые описания уже признают. Делать
  ПОСЛЕ #1 (skills_eval в advisory), перетестировать триггеры (`texture/sprite/glb`).
  `node tools/skills_eval.mjs && node tools/skills_sync.mjs --check`

### Фаза 6 — Заморозить выживших + закон храповика (точка схождения)
- **#15** Заморозить блокирующий набор (6: `close_slice`, `visual_material_floor`,
  `restricted_assets_guard`, `visual_invariant_guard`, `repeated_failure_guard`,
  `sync --check`) + защищённый ров; объявить `ai_profile`/`state_codegen`/`skills_eval`
  feature-complete; внести правило one-in-one-out в секцию 6. **Это и есть фикс «не сходится» —
  не опционально.** `node tools/ai.mjs validate`

## 6. Риски и митигации (из состязательных линз)

- **Stale-claim:** срез ассетов (#7,#8) — это `--full`/surface-LOC, НЕ quick-path скорость.
  Felt-ускорение даёт Фаза 1. Не путать порядок (не резать ассеты первыми ради «скорости»).
- **Регресс цели оркестрации:** start-nudge держать дешёвым/объективным; `repeated_failure_guard`
  + resolved-rejection в `close_slice` остаются блокирующими; пере-вооружить, если глаз увидит,
  что агент снова пишет всё соло.
- **Мис-роутинг при слиянии скиллов (#9):** широкие описания, перетест коллизий, только ПОСЛЕ
  advisory skills_eval.
- **Поломка импортов ассетов (#7,#8):** чинить импорты в том же коммите, `validate --full`,
  движковый packer не трогать.
- **Ре-аккреция:** без храповика+дисциплины это станет 5-й осцилляцией. Фаза 6 — суть, не полировка.
- **Задеть защищённое:** спайн `AGENTS.md` + секция-6 дословно; `doc_reference_check` после каждого
  слияния доков; правка рва — вне скоупа.

## 7. Порядок и приёмка

Каждая фаза = отдельные атомарные коммиты на `refactor/harness-diet`, каждый: **рез → валидаторы
зелёные → глаз лида**. Дифф показывается до мержа в master. Pre-существующий `context_budget` red
закрывается в Фазе 3 (он же Проблема 1), не раньше. Бенчмарка нет — приёмка по §2.
