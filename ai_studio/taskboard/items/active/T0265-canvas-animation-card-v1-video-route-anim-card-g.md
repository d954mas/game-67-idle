---
id: T0265
title: "Canvas animation card v1 (video route): anim card + generateAnimFromCard -> editable element.flipbook + rAF flipbook player; inc2 animation MODE w/ timeline trim/delete/fps/play-mode; inc3 FLF multi-keyframe"
status: backlog
project: P001
epic: E010
priority: P1
tags: [canvas, animation, video]
created: 2026-07-04
updated: 2026-07-07
---

## What

Build the Canvas video-route animation card: describe animation, generate a
flipbook element, preview it on canvas, then expand toward timeline editing and
multi-keyframe FLF.

## Done when

- [ ] Increment 1 creates an anim card and generates editable flipbook output.
- [ ] The canvas preview uses the existing rAF loop without procedural transforms.
- [ ] Later increments for timeline editing and FLF are accepted or split.

## Open questions

## Log
- 2026-07-04: Lead decisions 2026-07-05: matte default = corridorkey; EXPORT = frame-image SET (uniform size, aligned by the fixed video canvas - NO per-frame offsets/pivots stored; sheet packing stays optional; per-frame trim+offsets only if ever explicitly asked); ping-pong = play MODE (meta/preview property, frames never duplicated). Inc 1 launches when the corridorkey-wiring worker frees canvas files.
- 2026-07-04: Lead UX decision: START keyframe = always required (the source art, auto); END keyframe = OPTIONAL slot -> absent = plain I2V (text drives), =start via Loop toggle = seamless cycle, different pose = FLF inbetween. Attack nuance explained+accepted: full game attack (idle->strike->idle) = 3 keyframes = two chained FLF segments = increment 3 piecewise.
- 2026-07-06: Возобновление canvas-программы точечно по очереди паспорта паузы (PAUSE_STATE_2026-07-05, п.1). Инкремент 1 стартует в git worktree .claude/worktrees/anim-card-t0265 (ветка anim-card-t0265 от 30da65d8) — master занят другим агентом. Строю по design_video_anim_canvas_2026-07-05.md.
- 2026-07-06: Инкремент 1 ПОСТРОЕН на ветке anim-card-t0265 (worktree): anim-блоб v1 + element.flipbook v1, опы createAnimCard(memberId-промоушен одной записью)/patchAnim/generateAnimFromCard, шов anim_generate.mjs (generate→frames→matte, gen_fps реально в workflow, разрешённый seed в провенанс), API/CLI паритет, rAF-флипбук-плеер (once/loop/pingpong, no idle rAF, tint per-frame), инспектор renderAnim/renderFlipbook, контекст-меню. Ревью: 23/23 находки подтверждены и закрыты. Сьют 646/646 x2; headless-смоук 12/12. Осталось: живой прогон лида с ComfyUI (:8188 вручную) + merge ветки в master.
- 2026-07-06: ЖИВОЙ ПРОГОН OK (демо-проект anim-demo-t0265-78af36): wings-фикстур -> anim-card --member (fit 528x528, одна запись) -> anim-generate draft: WAN ~2мин полный цикл, 25 кадров 384x384, corridorkey 36s, фракционная альфа 12.6% (свечение сохранено), flipbook 16fps loop рядом с карточкой, разрешённый seed 637238 в провенансе (anim.seed=null остался). GIF-превью отправлен лиду. НАЙДЕН ШОВ для inc2: generate-стадия не компонует RGBA на зелёный — RGBA-вырез как кейфрейм упрётся в отказ corridorkey на нейтральном фоне; нужен пре-степ composite-onto-green в anim_generate.
- 2026-07-06: Дубль seed637238 = почти статика (bbox 1-2px, центроид <1px, diff 0.74/255 — только свечение; WAN-вариативность на мягком 'gently'). Сырое видео отправлено лиду. СТИР ЛИДА для inc2: показывать ВИДЕО отдельно ДО вырезки — двухфазный флоу: Generate video (превью сырого дубля-mp4) -> accept -> cut (frames+matte) -> flipbook; matte платим только за принятые по движению дубли (экономия ~36s+/ре-ролл). Ре-ролл с усиленным motion-текстом запущен.
- 2026-07-06: UX-фидбек лида: image / flipbook / (будущее) видео неразличимы на канвасе в покое. Сейчас: добавляю chrome-бейдж на flipbook-элементы (▶ кадры·fps, не попадает в экспорт/renderGroup). Для inc2: видео-дубли получают свою метку; различимость типов = требование дизайна режима.
- 2026-07-06: ЗАМОРОЗКА (решение лида 2026-07-06): локальная WAN Q4 ненадёжна по движению (улики: 3 живых дубля, взмахи только на golden-сиде 70263 — пайплайн исправен, репродукция детерминистична), ~2мин/дубль, греет ноут. Инкремент 1 ГОТОВ и запушен (anim-card-t0265, e4845708+3f9eb9c1, бейдж типа включён); merge в master ПОЗЖЕ по слову лида. Паспорт: docs/FREEZE_VIDEO_ANIM_2026-07-06.md; setup-знания и workflow JSONs в docs/video_freeze/; тяжёлые улики на YandexDisk (145MB); video_gen_experiment (~41GB) удаляет лид одной командой из паспорта. Разморозка: fal.ai hosted WAN (пилот <$5) или новые локальные модели; критерий = надёжность движения по сидам.
- 2026-07-14: E018 supersedes the deleted pause/freeze documents as active
  routing. Current freeze/resume rules live in
  `ai_studio/assets/canvas/contracts/animation.md`; video setup remains in the
  owned video-tool documentation.
- 2026-07-07: MERGED в master (11256890b): инкремент 1 + заморозка + гейтинг UI + freeze-знания. Конфликт с pack-картой T0332 разрешён (обе фичи целиком, anim вплетён в screen-флаги как recipe/style), сьют 710/710 на слитом состоянии. Ветка anim-card-t0265 остаётся для истории.
