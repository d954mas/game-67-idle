# Canvas video-animation workflow (Track B seam) — design

Design for the CANVAS-side workflow that turns art into game sprite animations via
the VIDEO route (ComfyUI WAN I2V pipeline). First-principles, built on the existing
recipe/style CARD pattern, the region-edit ISOLATION MODE, and the Track B video
pipeline. No code yet.

Author: deep-reasoner. Companion research (how FLF/раскадровка/motion-ref is done in
the wild) runs in parallel and is NOT duplicated here.

Incorporates the lead's binding additions (2026-07-05):
- разные наборы кейфреймов — first-class (не опция);
- результат = РЕДАКТИРУЕМАЯ последовательность кадров (обрезать/удалять кадры), лист
  = производный экспорт;
- настраиваемая скорость (fps);
- режим проигрывания: once / loop / **ping-pong**;
- отдельная «сцена»/режим для настройки анимации.

---

## 0. Стержневые решения (три предложения)

1. **Карточка анимации = такая же карточка, как recipe/style:** группа с аддитивным
   блобом `anim`, кейфреймы = image-мемберы группы. Даёт бесплатно
   snapshot/undo/redo/copy-paste (store спредит `groups` дословно) и симметрию с
   recipe (ops.mjs:1936-2088).
2. **Результат Generate = РЕДАКТИРУЕМАЯ последовательность кадров, а не запечённый
   лист.** Пайплайн импортирует ПОКАДРОВЫЕ RGBA-PNG (стадия matte, ДО упаковки) в
   `files/`; элемент несёт `element.flipbook = { frames:[{src,kept}], fps,
   play_mode }`. Обрезка/удаление кадров, fps, режим петли — журналируемые оп канваса
   (один жест = одна запись, Ctrl+Z). **Лист пакуется из ОТРЕДАКТИРОВАННОЙ
   последовательности при экспорте** — лист производен, primary object = кадры.
3. **Дом для покадрового редактирования — отдельный РЕЖИМ АНИМАЦИИ** (аналог
   region-edit isolation, app.js:277 / workspace.js:511,982,996): двойной клик по
   карточке → изоляция (остальное гаснет до 0.3, хлебная крошка, Esc — выход), внизу
   таймлайн-полоса кадров, крупное превью, транспорт, strip кейфреймов для след.
   генерации. Инспектор остаётся лёгким зеркалом свойств (гибрид). Обоснование — §6.

Всё остальное — следствия.

---

## 1. Схема (аддитивно, версионно)

### 1.1 Блоб `group.anim` — schema `ai_studio.canvas.anim_card.v1`

Аналог `defaultRecipe()` (ops.mjs:1953). Кейфреймы (source art + раскадровка) — это
обычные image-мемберы группы (как recipe refs), НЕ здесь; см. §5.

```jsonc
anim = {
  v: 1,
  motion: "",              // текст движения (как recipe.prompt)
  profile: "draft",        // "draft" | "final"
  seed: null,              // null = случайный на каждый Generate; number = зафиксирован
  matte: "corridorkey",    // "corridorkey" | "key_matte"  (см. открытый вопрос 1)
  gen_fps: null,           // fps ГЕНЕРАЦИИ (null = 16 из воркфлоу). Скорость ВОСПРОИЗВЕДЕНИЯ
                           //   правится на результате (element.flipbook.fps), не тут.
  loop: true,              // подсказка бесшовной петли -> 1 кейфрейм даёт same-image FLF (§5)
  columns: null, trim: false,  // упаковка листа при экспорте (pack_sheet.py)
  style_ref: null,         // id style-карточки (общая идентичность персонажа, инкремент 4)
  accepted_ref: null,      // id принятого flipbook-элемента (инкремент 2)
  last_run: null           // {at, result_element_id, verdict} — как recipe.last_run
}
```

Валидация патча = `normalizeAnimPatch` (близнец `normalizeRecipePatch`): громко на
неизвестный `profile`/`matte`, нечисловой `seed`, не-bool `loop`/`trim`, `style_ref`
не (null|id style-карточки), `accepted_ref` не (null|id flipbook-мембера). Симметричные
guard'ы вложенности (как recipe/style, ops.mjs:2060/2211).

### 1.2 Результат — `element.flipbook` — schema `ai_studio.canvas.flipbook.v1`

**Решение: результат = image-элемент + аддитивный блоб `flipbook`, а НЕ новый тип
элемента.** Так selection/move/resize/duplicate/delete/hit-test/tree работают без
правок (как recipe/style остались группами). `element.src` = ПЕРВЫЙ кадр (вменяемый
fallback + тумбнейл), а поле `flipbook.frames` — источник истины.

```jsonc
element = {
  id, type: "image",
  src: "files/<frame0>.png",     // кадр 0 — fallback/тумбнейл
  x, y, w: frame_w, h: frame_h,  // бокс = ОДИН кадр (не лист)
  source_w: frame_w, source_h: frame_h,
  name: "Idle · take 2",
  flipbook: {
    v: 1,
    frames: [ { src: "files/<h1>.png", kept: true }, { src: "files/<h2>.png", kept: false }, … ],
    fps: 12,                     // СКОРОСТЬ воспроизведения (правится; экспортится в meta)
    play_mode: "loop",           // "once" | "loop" | "pingpong"
    frame_w, frame_h
  },
  meta: { anim_run: { motion, profile, seed, keyframes, runDir, source_chain… } }
}
```

- **Эффективная последовательность = `frames.filter(kept)`.** Обрезка/удаление
  переключают `kept`; файлы остаются на диске (content-addressed, неизменяемы —
  дешёвый undo, ровно паттерн non-destructive src-swap из alphaCutout, store.mjs:435).
- Импортируются matte-кадры (по одному в `files/`), НЕ лист. Лист собирается позже.

---

## 2. Операции (op-слой) с сигнатурами

Скелет — близнец recipe-опов (lock/re-read/`refuseIfHeadMoved`/один commit).

```
createAnimCard(root, { projectId, name?, x?,y?,w?,h?, parentId? }) -> { project, group }
  // Близнец createRecipeCard (ops.mjs:2040). defaultAnim(); fit-to-content при промоушене.

patchAnim(root, { projectId, groupId, patch }) -> { project, group }
  // Близнец patchRecipe. patch ⊆ {motion,profile,seed,matte,gen_fps,loop,columns,trim,
  // style_ref,accepted_ref}. Громко на не-anim-группе.

generateAnimFromCard(root, { projectId, groupId, generators? }) -> { project, element, group, run }
  // Близнец generateFromRecipe (ops.mjs:2343), но импортирует КАДРЫ, не лист:
  //  1. validate loud (не-anim, пустой motion, 0 кейфреймов).
  //  2. мемберы-картинки в X-порядке -> abs-пути (§5). Инкремент 1: 1 кейфрейм = I2V.
  //  3. СНАРУЖИ журнала: generators.run({keyframePaths, motion, profile, seed, matte,
  //     gen_fps}) -> { framePaths[], meta:{frame_w,frame_h,fps}, runDir }.
  //     Дефолт = tools/anim_generate.mjs — гоняет generate->frames->matte СТАДИИ видео-
  //     тула и ОСТАНАВЛИВАЕТСЯ на matte (лист НЕ пакуется здесь). Тесты инжектят фейк.
  //  4. withProjectLock: re-read + refuseIfHeadMoved (генерация шла минуты).
  //  5. store.addFile каждого кадра -> flipbook-элемент (src=кадр0, frames[], fps из
  //     meta, play_mode из anim.loop) РЯДОМ с карточкой в PARENT-scope (НИКОГДА внутрь).
  //  6. anim.last_run; ОДИН commitMutation.

// ---- покадровое редактирование результата (журналируемо, оба клиента) ----
editFlipbookFrames(root, { projectId, elementId, gesture }) -> { project, element }
  // gesture = {kind:"trim", start, end}          // оставить [start,end], остальным kept=false
  //         | {kind:"delete", indices:[…]}        // kept=false выбранным
  //         | {kind:"restore", indices:[…]}       // вернуть kept=true
  // Один журнал = один жест ("Trim frames 3–20" / "Delete frame 12"). Файлы не удаляются.
  // Громко, если после жеста не осталось ни одного kept-кадра (пустая анимация запрещена).

patchFlipbook(root, { projectId, elementId, patch }) -> { project, element }
  // patch ⊆ { fps (>0), play_mode ("once"|"loop"|"pingpong") }. Один журнал. Близнец patchRecipe.

exportFlipbookSheet(root, { projectId, elementId }) -> { sheetPngBytes, meta }
  // ПРОИЗВОДНЫЙ артефакт: пакует ТОЛЬКО kept-кадры (в порядке) через pack_sheet.py в лист
  // + sheet.json; в meta пишет fps + play_mode + loop, схема ai_studio.video.spritesheet.v1.
  // pingpong -> либо в meta для движка, либо запекается (kept + reversed(kept[1:-1])) — воп.3.
```

Клиент (site) + CLI — строгий паритет:
```
actions.js: createAnimCardAction / patchAnimAction / generateAnimFromCardAction (runLongOp)
            trimFramesAction / deleteFramesAction / setFpsAction / setPlayModeAction / exportSheetAction
anim mode:  enterAnimEdit(cardId) / exitAnimEdit()  (page-only, §6)
cli.mjs:    anim-card / anim-patch / anim-generate / anim-frames-edit / anim-flipbook-patch / anim-export
api.mjs:    POST  /projects/<id>/anim-cards
            PATCH /projects/<id>/anim-cards/<gid>
            POST  /projects/<id>/anim-cards/<gid>/generate
            POST  /projects/<id>/flipbooks/<eid>/frames        {gesture}
            PATCH /projects/<id>/flipbooks/<eid>               {fps?, play_mode?}
            POST  /projects/<id>/flipbooks/<eid>/export-sheet
```

### 2.1 Генераторный шов — `tools/anim_generate.mjs` (близнец recipe_generate.mjs)

Единственное место, тянущееся из канваса в `ai_studio/assets/tools/video`. Оркестрирует
`runGenerate`/`runFrames`/`runMatte`, возвращает `{framePaths[], meta, runDir}` (лист НЕ
пакует — это делает `exportFlipbookSheet` из отредактированных кадров). Run-папка —
снаружи репо под `videoGenRoot`; кадры копируются В проект (самодостаточно), runDir —
провенанс. Тесты инжектят фейк, отдающий N синтетических RGBA-кадров — GPU/ComfyUI в
сьюте не крутится (контракт T0238).

---

## 3. Отрисовка последовательности (переиспользование rAF-плеера)

Теперь рисуется НЕ под-рект листа, а `<img>` текущего kept-кадра (per-frame decode,
кэш по `src` — content-addressed, кэш не протухает, как paletteCountCache в
cleanup_dialog.js:31). Кадр:

```
K = kept.length;  adv = floor((now - previewClockT0)/1000 * fb.fps)
покой:   idx = 0
loop:    idx = adv % K
once:    idx = min(adv, K-1)                       // держит последний кадр
pingpong:P = 2*(K-1); p = adv % P; idx = p < K ? p : P - p
ctx.drawImage(kept[idx].img, origin.x, origin.y, w, h)   // тот же alpha/rotate/flip-враппер
```

Переиспользуем `previewingElementIds` + `previewClockT0` + самочистящийся rAF
(workspace.js:412-454); обобщаем прунинг «нет `.animation`» -> «нет `.animation` И нет
`.flipbook`». Каждый элемент сэмплит СВОИМ fps -> два дубля с разными fps крутятся
синхронно от общего t0 (сравнение бесплатно, §5). `elementCullable` уже не куллит
проигрываемое.

---

## 4. Состояния персонажа (idle/walk/attack)

**Одна карточка на состояние** (idle/walk/attack = ряд карточек). Одна карточка = один
лист = один engine-flipbook, со своим motion/раскадровкой/принятым дублем/целью
экспорта. Ряды-состояния в одной карточке ломают симметрию с recipe и модель «одна
карточка — один рельс дублей».

**Идентичность персонажа — из существующих примитивов, без новой сущности:** общий
source art (один image первым кейфреймом в каждую карточку — content-addressed, ноль
дублей байтов) + общий `anim.style_ref` -> ОДНА style-карточка (палитра) + при желании
общий зафиксированный `seed`. «Character-группа» возможна позже, сейчас не нужна.

---

## 5. Раскадровка / вариативность кейфреймов (core, лид подтвердил)

**Кейфреймы = упорядоченные image-мемберы карточки**, создаются/правятся ЛЮБЫМ
инструментом канваса (нарисовать, дропнуть, сгенерить recipe-карточкой и перетащить).
**Порядок = слева-направо по X** (tie-break Y, затем id — детерминированно); на каждом
номерной бейдж (как «ref»-чип, workspace.js:596). В режиме анимации (§6) они же
показаны отдельной strip-полосой с add/remove/reorder.

| Кейфреймов | Что | Пайплайн |
|---|---|---|
| **1** | plain I2V: source + motion | ЕСТЬ сегодня (generate.mjs:178) |
| **2** | FLF: kf0=старт, kf1=конец; `loop:true`/тот же кадр = бесшовно | нужен FLF-workflow |
| **3-4** | piecewise-FLF: N−1 сегментов FLF(kf_i,kf_{i+1}), кадры конкатенируются, затем matte | нужен FLF-workflow |

Канвас лишь отдаёт список путей в X-порядке; FLF/piecewise — забота стадии generate
(появится нативный multi-keyframe workflow — меняем только её, контракт неизменен).

---

## 6. ГЛАВНОЕ UX-решение: отдельный РЕЖИМ АНИМАЦИИ (сцена)

**Рекомендация: выделенный РЕЖИМ АНИМАЦИИ как аналог region-edit isolation +
инспектор-зеркало (гибрид).** Не floating-dialog, не только инспектор.

### 6.1 Почему — честное сравнение

- **Только инспектор (260px): отвергнут как основной.** Горизонтальный таймлайн из
  25-33 тумбнейлов с ручками обрезки + плейхедом + крупное превью для оценки движения
  физически не влезают в 260px. Инспектор ОСТАЁТСЯ лёгким зеркалом (motion/profile/
  seed + кнопки Generate/Edit) для быстрой правки без входа в режим (паритет).

- **Floating-dialog (cleanup_dialog.js): отвергнут для покадрового.** Лид полюбил его
  ЭТУ неделю — и он правильный для НЕСКОЛЬКИХ слайдеров на ОДНОМ элементе с live-превью
  на канвасе. Но: (1) таймлайн+крупное превью+strip+транспорт занимают почти всю сцену
  — палитра перекрыла бы редактируемое искусство; (2) его модель Apply/Cancel/zero-trace
  и «один незакоммиченный превью» (cleanup_dialog.js:4-8) прямо ПРОТИВОРЕЧИТ требованию
  «каждая правка — своя журналируемая undoable-оп»: редактирование анимации —
  длинная многожестовая сессия (trim, delete, fps, re-roll), а не один превью под Apply.
  *Ниша для палитры остаётся:* когда дубль уже хорош и надо только быстро подкрутить
  fps/loop на выделенном flipbook — опциональная мини-палитра транспорта БЕЗ входа в
  режим. Режим её покрывает; палитра — необязательный бонус.

- **Режим анимации (изоляция): выбран.** Совпадает с инстинктом лида («встроенное меню
  сцена») и переиспользует проверенный, адверсарно-ревьюнутый паттерн region-edit со
  всей машинерией: page-only флаг (не журналируется), вход по dblclick, гашение
  остального до 0.3 (workspace.js:511), хлебная крошка (workspace.js:982), mode-tool-зона
  (workspace.js:996), ограничение хит-теста (workspace.js:1785), Esc — выход,
  **клэмп undo** (`animEditBaseSeq`: внутри режима Ctrl+Z не откатывает за вход в режим —
  правило лида, app.js:70,282), `reconcile` page-only состояния против перезагрузки
  (app.js:310). Риск низкий, поведение когерентно остальному канвасу.

### 6.2 Механика (близнец region-edit)

```
state.animEditId      // page-only, id карточки в режиме; null = обычный канвас
state.animEditBaseSeq // journal seq на входе -> клэмп undo
state.animTakeId      // какой дубль загружен в редактор (по умолчанию accepted_ref/последний)
enterAnimEdit(cardId) // dblclick карточки / "Edit animation" в инспекторе / контекст-меню
exitAnimEdit()        // Esc — один шаг; чистит режим/плейхед/выделение
reconcileAnimEdit(project, animEditId)  // выйти, если карточка исчезла; иначе остаться (чистый, тестируемый)
```

### 6.3 Разметка сцены (ASCII)

Вход: dblclick карточки → изоляция, остальное гаснет, крошка «Project ▸ Idle — Esc».

```
┌ Project ▸ Idle ─────────────────────────────────────────── Esc to exit ┐
│ KEYFRAMES  [①▣][②▣] (+ перетащи арт)   motion:[ gentle idle bob…      ] │
│ profile[draft▾]  seed(rnd|701)  matte[key_matte▾]      [ Generate ][⟳] │
│ ┌─────────────────────────────────────────┐   TAKES ───────────────┐   │
│ │                                          │   ▸ take1 · draft       │   │
│ │            ▷   КРУПНОЕ ПРЕВЬЮ            │   ▸ take2 · draft   ✓    │   │
│ │            (текущий дубль)               │   ▸ take3 · final        │   │
│ │                                          │                          │   │
│ └─────────────────────────────────────────┘                          │   │
│  ⏮  ▷once  ⟳loop  ⇄ping     fps[■■■■□ 12]  ───playhead──○──────       │   │
│                                                                          │
│ TIMELINE ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐   [ Accept ✓ ][Export]  │
│  keep →  │▓1│▓2│▓3│░4│░5│▓6│▓7│▓8│▓9│10│11│12│   ░ = removed (kept:false)│
│          ╟──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──╢   ╟ ╢ = trim-ручки        │
└──────────────────────────────────────────────────────────────────────────┘
```

- **TOP** — вход след. генерации: strip кейфреймов (add/remove/reorder, нумерованы) +
  motion + profile/seed/matte + [Generate]/[Re-roll(⟳)].
- **CENTER** — крупное превью текущего дубля, играет по fps/play_mode. Транспорт снизу:
  once/loop/pingpong + слайдер fps + скраб-плейхед.
- **BOTTOM (TIMELINE)** — тумбнейлы kept-кадров по порядку; удалённые тусклы/зачёркнуты;
  ручки обрезки по краям (тянуть = crop диапазона -> `editFlipbookFrames trim`); клик =
  скраб; Del/ПКМ = delete/restore кадра; плейхед бежит по превью.
- **RIGHT** — рельс дублей карточки (каждый Generate = дубль); клик грузит в редактор;
  принятый помечен ✓ (`anim.accepted_ref`). [Export] пакует kept-кадры в лист+json.

---

## 7. Петля итерации (UX)

`Generate(draft) → войти в режим → обрезать/удалить плохие кадры → выставить fps + режим
петли → (принять) → Export`.

- **Draft-first.** `profile=draft` (~35s тёплый + frames+matte ≈ 1.5-2 мин; matte —
  длинный шест). Ре-ролл = Generate (при `seed:null` новый сид). «Lock seed & go final»
  = patch `seed`=сид дубля + `profile:final`, один Generate финала тем же сидом.
- **Занятость/очередь:** тот же `long_op_queue.mjs` (max 2) + `runLongOp`-тост, что у
  recipe/alpha (actions.js:988). Видео-generate GPU-эксклюзивен (один ComfyUI) — второй
  прогон сериализуется на сервере; max 2 безопасен (заметка, не блокер).
- **Правка кадров дешёвая и локальная** (kept-флаги, undo мгновенный) — можно резать
  агрессивно и откатывать.
- **Сравнение дублей бесплатно и лучше recipe-«both»:** превью-луп играет НЕСКОЛЬКО
  элементов в фазе от общего клока (workspace.js:406), каждый со своим fps -> два дубля
  крутятся бок о бок, лид выбирает. Спец-compare-режим не нужен.
- **fps и play_mode** живут на дубле (`element.flipbook`), правятся в режиме, пишутся в
  meta листа при экспорте (движок знает once/loop/pingpong).

---

## 8. План инкрементов (каждый проверяем лидом в браузере)

### Инкремент 1 — ШОВ + ядро петли *(маленький, первый)*
Select art → New animation card → motion → Generate(draft) → **последовательность кадров
проигрывается петлёй на канвасе**.
- `createAnimCard`/`patchAnim`/`generateAnimFromCard` (импорт matte-КАДРОВ, не листа);
  `element.flipbook` v1 (frames/fps/play_mode="loop").
- `tools/anim_generate.mjs` (1 кейфрейм = plain I2V; инжектируемый генератор).
- Per-frame flipbook-отрисовка (reuse rAF-плеера) + Play/Stop.
- Инспектор `renderAnim` (клон renderRecipe) + `renderFlipbook` (Play + счётчик кадров).
- API/CLI паритет; контекст-меню «New animation card» + промоушен «Animate this image».
- Зависимость: ComfyUI поднят вручную (v1 не автостартит, громкая ошибка), `videoGenRoot`.
- Проверка: арт → карточка → Generate → кадры крутятся петлёй.

### Инкремент 2 — РЕЖИМ АНИМАЦИИ + покадровое редактирование + скорость + режим петли
*(здесь приземляются новые требования лида, сразу после ядра)*
- **Режим анимации** (аналог region-edit): `enterAnimEdit`/`exitAnimEdit`, изоляция +
  гашение 0.3 + крошка + Esc + клэмп undo + reconcile; таймлайн-полоса + крупное превью
  + транспорт + strip кейфреймов + рельс дублей (§6).
- **Покадровые оп:** `editFlipbookFrames` (trim/delete/restore) + `patchFlipbook`
  (fps/play_mode: once/loop/**pingpong**) — журналируемы, undoable; превью честно
  отражает fps + режим live.
- **Экспорт-лист производен:** `exportFlipbookSheet` пакует kept-кадры → PNG + sheet.json
  (fps + play_mode в meta). Хендофф в движок.
- **Accept:** `anim.accepted_ref` + ✓-бейдж (клон ref-бейджа style, workspace.js:602).
- Проверка: Generate → режим → обрезать/удалить кадры → fps 12 + loop/once/pingpong →
  Export из отредактированной последовательности.

### Инкремент 3 — вариативность кейфреймов / раскадровка (2-4 → FLF/piecewise)
- strip кейфреймов в режиме: add/remove/reorder source-картинок (нумерованы, X-порядок).
- Стадия generate получает FLF(2) + piecewise(3-4); `loop:true` → same-image FLF.
- **ГЕЙТ:** нужен FLF-способный WAN workflow (downstream — исследование параллельного
  агента). 1 кейфрейм = plain I2V (путь инкремента 1) неизменен.
- Проверка: 2 кейфрейма start/end → бесшовная петля; 3-4 → многопозная анимация.

### Инкремент 4 — состояния + общая идентичность
- `anim.style_ref` → style-карточка; «Duplicate card for next state» (копия, чистый
  рельс, сохранён source art + style_ref); ряд idle/walk/attack.
- Проверка: три state-карточки с общим стилем, каждая экспортит свой лист.

---

## 9. Открытые вопросы (нужен выбор лида, максимум 3)

1. **Дефолтный matte на карточке?** `corridorkey` (мягкое свечение/полупрозрачность, но
   лицензия CC-BY-NC-SA-4.0, asset-processing carve-out) против `key_matte` (плотные
   спрайты, чистая лицензия, ~0.28s/кадр). Пайплайн дефолтит corridorkey; для игровых
   спрайтов без свечения чистолицензионный `key_matte` может быть лучшим дефолтом.

2. **Куда «промоутится» принятый лист?** Плоский экспорт листа+json из канваса — ИЛИ
   прогон через asset-пайплайн (`ai_studio/assets`, origin/license/provenance, галерея/
   promote)? Маршрутизация «готовый спрайтлист → библиотека игры».

3. **Ping-pong при экспорте:** писать `play_mode:"pingpong"` в meta и отдать движку —
   ИЛИ ЗАПЕКАТЬ (kept + reversed(kept[1:-1])) в сам лист, чтобы любой loop-only движок
   получил ping-pong даром (ценой ~2× кадров в листе)? Зависит от возможностей
   engine-flipbook рантайма.
```
