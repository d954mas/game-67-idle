# Cleanup UX redesign — inspector (T0207 follow-up)

Date: 2026-07-04 · Scope: site JS/CSS only · Ops/API/CLI unchanged · Single preview slot kept

---

## Проблема (verbatim lead feedback → что сломано)

1. «квантование и денойз — разные несвязанные процедуры? или как?» — они ЧИТАЮТСЯ как настройки одной процедуры.
2. «шум сбрасывает квантование, а квантование сбрасывает шум» — тихая подмена превью читается как потеря состояния.
3. «по очереди применять подходит, но тогда либо не давать менять шум пока есть квантование (и наоборот), либо давать но явно говорить что состояние сброшено».
4. «К работе нет претензий, ui ux неудачный» — бэкенд/ops оставляем, проблема в подаче.

## Корневая причина (архитектура, не косметика)

Есть ДВА инструмента, но ОДИН слот превью и ОДНА общая полоса контролов, где оба набора всегда «живые».

- Один слот: `cleanupPreview = {elementId,bitmap,tool,params,report}` — `workspace.js:351`. `setCleanupPreview()` молча ПЕРЕЗАПИСЫВАЕТ прошлый — `workspace.js:361-365`. Слот единолично владеет и подменой пикселей (`workspace.js:515-516`), и амбер-чипом на канвасе (`workspace.js:582-595`).
- Оба контрола держат свои позиции независимо от того, кто владеет слотом: слайдер стоит на 64, сегмент — на 2, но превьюит только ОДИН. Двинул другой — слот молча украден. Ничто на экране не говорит, что слот односместный.
- То есть «сброс» — это иллюзия: значения контролов НЕ сбрасываются, сбрасывается невидимое владение слотом. Именно поэтому misread «одна процедура с общими настройками» неизбежен: один Apply, одна полоса, один чип.

Что переживает ребилд (важно для реализуемости): `render()` (`workspace.js:223`) перерисовывает только канвас, инспектор НЕ трогает. `renderInspector()` (`inspector.js:2699`) ребилдит по сигнатуре ЭЛЕМЕНТА (`inspectorSig`, line 2630) — слота превью в сигнатуре нет. Значит set/clear/compare превью НЕ ребилдят секцию; ребилд только на смене выделения или записи `meta.cleanup` (Apply — сигнатура включает `JSON.stringify(e.meta)`, line 2663). Любое «сессионное» состояние переживёт обновления превью, но после настоящего ребилда должно ВОССТАНАВЛИВАТЬСЯ из `getCleanupPreview().tool`.

## Инварианты (не трогаем)

- Один слот превью. Никакого compose/chain — lead принял последовательный Apply.
- Ops/API/CLI без изменений; строгий tool-parity site+CLI (та же `cleanupPreview`/`cleanupApply`).
- Оставляем счётчик палитры («Current palette: N») и реальный OFF на 0 силы денойза.
- Компактность: колонка ~240px полезной ширины. Секции collapsible.
- Live-preview на канвасе (скраб слайдером без кнопки Preview) — это ПОХВАЛЕННАЯ фича, не ломать.

---

## Вариант A — две collapsible-секции + жёсткий lock соседа

Разбить одну «Cleanup» на две независимые секции `QUANTIZE` и `DENOISE` (каждая — свой `collapsible()`), у каждой СВОИ контролы, свой report и своя полоса действий. Слот один; кто им владеет — тот «активен», второй ЖЁСТКО заблокирован (все инпуты disabled + одна строка-причина), пока в соседе живёт превью. Это буквально lead-choice #3-A.

DOM:
```
Live preview on canvas — art doesn't change until Apply.   ← один muted-хинт над обеими

▾ QUANTIZE
   Reduce the color palette.                                ← muted sub-line
   Current palette: 128 colors
   [====|=======] [ 64] ⟳                                   ← range 2..256 + num + spinner
   ☐ Dither
   palette 128 -> 64, 12% pixels changed                    ← report (пусто в idle)
   [ Hold to see original ] [ Reset ] [ Apply ]             ← ТОЛЬКО когда эта секция владеет слотом
   Applied: quantize 64 colors — Ctrl+Z reverts.            ← если meta.cleanup.tool==quantize

▾ DENOISE
   Remove speckle.
   [0][1][2][3]   0 off · 1 light · 3 strong  ⟳
   ( report )
   Locked — Reset or Apply Quantize first.                  ← когда слотом владеет QUANTIZE
```

State machine (общий `syncActions()` дирижирует ОБЕИМИ секциями — обе строятся в одном `renderCleanup`, closure видит оба набора):
- idle (слот пуст): обе секции интерактивны; полос действий нет, lock-строк нет.
- previewing(quantize): Quantize — Reset/Apply/Compare активны, report виден; Denoise — все инпуты disabled + «Locked — Reset or Apply Quantize first.».
- previewing(denoise): зеркально.
- Apply → пишет `meta.cleanup` → сигнатура меняется → обе секции ребилдятся заново в idle; появляется «Applied: … — Ctrl+Z reverts.» в секции по `meta.cleanup.tool`.

Сосед пока превьюишь: жёсткий lock (disabled инпуты + строка-причина, как разблокировать). Chip на канвасе: без изменений; опц. префикс имени инструмента («quantize preview — Apply to keep», tool уже в слоте — 1 строка в `paintElement`). «Applied — Ctrl+Z reverts»: одна строка, в секции инструмента, что реально записан в `meta.cleanup` (последний). Collapse активной секции чистит превью (как сейчас, `inspector.js:690-692`), но пер-секционно; collapse залоченной — no-op.

Copy: заголовки `QUANTIZE`/`DENOISE` (uppercase из CSS); sub-lines «Reduce the color palette.» / «Remove speckle.»; кнопка просто `Apply` (инструмент уже в имени секции — уходит морфинг «Apply Quantize/Denoise»); lock «Locked — Reset or Apply {Other} first.».

Tradeoffs:
- Discoverability: ЛУЧШАЯ на вопрос #1 — две именованные секции = структурно «две несвязанные процедуры», без словесной эквилибристики.
- Steps-to-apply-both: Quantize→Apply→(unlock)→Denoise→Apply. ~2 цикла, как сейчас; lock навязывает модель «сначала доведи один».
- Accidental discard: НУЛЕВОЙ — слот нельзя украсть, сосед залочен. Прямо убивает баг #2. Явный жест сброса — Reset.
- Visual noise: два заголовка + две полосы действий = выше; митигируется collapse (бесплатно, localStorage) и тем, что полоса/лок-строка видны только при живом превью.
- Impl size: средний. Раздвоить на два `collapsible()`, продублировать actions-row, общий `syncActions` тоглит обе + lock-строку. ~+40 строк JS, CSS почти весь переиспользуется. Слот и ops НЕ трогаются.

Риск: жёсткий lock кажется строгим, если хочется просто «прыгнуть» на другой инструмент — но это ОДИН лишний клик Reset, и он честно называет сброс. Lead сам указал lock как приемлемый.

---

## Вариант B — один раздел, таб-переключатель (segmented `Quantize | Denoise`), переключение анонсирует сброс

Одна секция «Cleanup». Сверху 2-позиционный сегмент `Quantize | Denoise` = активный инструмент. Ниже видны контролы ТОЛЬКО активного (второй скрыт, не просто disabled). Одна полоса действий. Переключение таба при живом превью: либо (b1) confirm «Switch to Denoise? Quantize preview will be dropped.», либо (b2) просто переключает и показывает inline «Quantize preview dropped.». Это lead-choice #3-B.

DOM:
```
▾ CLEANUP
   [ Quantize | Denoise ]                                   ← активный инструмент
   Live preview on canvas — art doesn't change until Apply.
   Current palette: 128 colors                              ← блок Quantize (виден только на этом табе)
   [====|=======] [ 64] ⟳
   ☐ Dither
   ( report )
   [ Hold to see original ] [ Reset ] [ Apply ]
   Applied: quantize 64 colors — Ctrl+Z reverts.
```

State machine: `activeTool ∈ {quantize,denoise}` в closure/модуле, ПОСЛЕ ребилда восстанавливается из `preview.tool`.
- idle(tool): виден блок tool, превью нет.
- previewing(tool): контролы живы, полоса активна, report виден.
- switch-tab пока previewing(текущий): drop превью + анонс (b2) ИЛИ confirm-then-drop (b1).
- Apply → ребилд → idle, «Applied».

Сосед: физически скрыт (другой таб). Chip/Applied — как в A (одна строка).

Tradeoffs:
- Discoverability: сегмент «Quantize | Denoise» читается как «выбери один из двух» — норм, но таб может прочитаться и как «два вида одного». Слабее A по вопросу #1.
- Steps-to-apply-both: Quantize→Apply→таб Denoise→Apply — примерно равно A.
- Accidental discard: СЛАБОЕ место. Клик по табу дёшев; b2 (тихо + ярлык «dropped») = ровно та «потеря состояния», на которую жаловались, только с подписью; b1 (confirm) защищает, но это диалог/модалка — трение, а этот app не любит web-chrome. Discard в одном шальном клике.
- Visual noise: САМЫЙ низкий — одна секция, один блок, одна полоса. Максимально «lean».
- Impl size: средний. Добавить таб-сегмент, show/hide блоков, wire switch→discard/announce, восстановление activeTool на ребилде. ~+30 строк, но edge-кейсы (preview.tool != activeTool после ребилда) фидбл.

Риск: семантика табов подсказывает, что инструменты КОМПОЗЯТСЯ (оба вклада), чего НЕТ (один слот, последовательно). Приходится словами объяснять, что переключение = отказ — копирайт-нагрузка против самого UI.

---

## Вариант C — per-tool «session»-карточки: инертно до [Preview], активная bordered «uncommitted»-карточка владеет слотом, сосед disabled с причиной

Одна секция, два стек-tool-карточки `Quantize` и `Denoise`. В idle карточка = контролы + кнопка `[Preview]`; ничто не живо, слот пуст. Нажал `[Preview]` — инструмент считается, карточка становится АКТИВНОЙ (амбер left-border, свои `[Apply]`/`[Cancel]`, live report, compare). Второй уходит в disabled с inline-причиной. Ровно один uncommitted по построению — рамка буквально обводит «единственное несохранённое».

DOM:
```
▾ CLEANUP
   ┌ Quantize ─────────────────────────┐   ← амбер-бордер = uncommitted
   │ Current palette: 128 colors        │
   │ [====|====] [64] ⟳  ☐ Dither       │
   │ palette 128 -> 64, 12% changed     │
   │ [ Hold original ] [ Cancel ] [Apply]│
   └────────────────────────────────────┘
   Denoise — finish Quantize first        ← disabled, причина inline
```

State machine: `session ∈ {none, quantize, denoise}`, после ребилда = `preview.tool ?? none`.
- none: обе карточки в idle с `[Preview]`.
- session(tool): карточка bordered/активна с Apply/Cancel; вторая disabled + причина.
- Apply/Cancel → session=none; Apply также ребилдит через meta.

Проблема: сейчас превью ЖИВОЕ на каждый драг слайдера (без кнопки Preview) — фича, что lead хвалит («ползунок от текущего значения», live preview на канвасе). Кнопка `[Preview]` до показа убивает live-скраб = регресс против контракта и «работает визуально». Смягчение (слайдер превьюит живо, первый превью НЕОТКРЫВАЕТ сессию/бордер и лочит соседа) → сходится к A с более тяжёлой рамкой.

Tradeoffs:
- Discoverability: САМАЯ явная про один слот — bordered «pending, один за раз», явные Apply/Cancel. Сильно.
- Steps-to-apply-both: при буквальном `[Preview]`-гейте: Preview→настрой→Apply→Preview→настрой→Apply = БОЛЬШЕ кликов (Preview = чистая церемония). Хуже A/B.
- Accidental discard: НИЗКИЙ (как A — сосед disabled, явный Cancel).
- Visual noise: САМЫЙ высокий — две bordered-карточки, per-card Preview/Apply/Cancel. Тяжелее всего в 240px.
- Impl size: БОЛЬШИЙ. Новый card-компонент, per-card actions, session-состояние, Preview-гейт, восстановление на ребилде. Плюс регресс live-скраба (или смягчение → это A с рамкой).

Риск: Preview-гейт против похваленного live-preview и визуального стиля работы; самый тяжёлый DOM; переинжинирено для однопользовательского локального инструмента.

---

## Рекомендация — Вариант A

Беру A (две collapsible-секции `QUANTIZE`/`DENOISE` + жёсткий lock соседа). Он структурно, без словесной компенсации, отвечает на главный вопрос lead («разные несвязанные процедуры?» — да, две именованные секции), реализует принятый выбор #3-A (лочим соседа), из-за чего слот НЕВОЗМОЖНО тихо украсть — это прямо убивает баг «шум сбрасывает квантование»; при этом сохраняется похваленный live-скраб (без церемонии Preview из C) и уходит морфящаяся кнопка «Apply Quantize/Denoise» (Apply per-section). Он переиспользует уже существующие примитивы (`collapsible()`, сегмент, `.insp-cleanup-*` CSS), не трогает единственный слот, ops/API/CLI, и держит высоту под контролем пользователя через per-section collapse. B компактнее, но таб-семантика подсказывает несуществующую композицию и держит сброс в одном шальном клике; C честнее всех про «один незакоммиченный», но платит Preview-церемонией (регресс live-preview) и самым тяжёлым DOM. Единственная цена A — вертикальная высота и один лишний клик Reset при желании «прыгнуть» на другой инструмент; это честный, названный жест сброса, а не тихая потеря.
