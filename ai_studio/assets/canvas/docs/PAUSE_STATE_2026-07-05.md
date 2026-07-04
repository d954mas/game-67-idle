# ПАУЗА 2026-07-05 — паспорт состояния canvas/video программы

Лид поставил программу на паузу («доделать до точки, сохранить все данные,
легко потом вернуться»). Этот файл — точка входа для возврата. Задача-якорь
на борде: T0267.

## Как вернуться за 5 минут

1. Прочитать этот файл + `VERIFY_2026-07-03.md` (рядом, снапшот) — там
   верхние секции 00/0 = последнее состояние и нерешённые вопросы лида.
2. Поднять сайт: `node ai_studio/studio_shell/server.mjs 8780` → браузер
   `http://127.0.0.1:8780/canvas/`.
3. Поднять видео-стек (если нужен): команда в
   `C:\projects\video_gen_experiment\README.md` (ComfyUI на :8188).
4. Сьюты: `node --test "ai_studio/assets/canvas/tests/*.test.mjs"` (566+),
   чат `node --test "ai_studio/studio_shell/chat/tests/*.test.mjs"` (51),
   питон-тулзы через `PYTHONPATH=<repo> .venv/Scripts/python.exe -m unittest <module>`.
5. Борда: `node ai_studio/taskboard/cli.mjs summary`.

## Что построено (крупно, всё закоммичено на master)

- **Канвас** (ai_studio/assets/canvas/): проекты/группы/регионы/текст/
  трансформы/гайды/slice-9 (+ seam-линии)/клипборд/история+джамп/чат-панель
  (codex-сессии)/recipe+style карточки/extract+expand/cleanup (плавающие
  окна Quantize/Denoise)/alpha: key_matte | dual_plate | corridorkey.
  Write-lock на проект, коды 404/409, вьюпорт-каллинг + мемоизация
  (таргет «тысячи объектов»). Сьют 566.
- **Alpha corridorkey** (a229f769 + фоллоу-ап): зелёный нативно, маджента
  через hue180-шим, регионы; ~15с GPU на вызов; громкие отказы.
- **Хранилище**: journal/snapshots/.lock в локальном кэше
  `C:/Users/ROG/AppData/Local/ai_studio/canvas_cache` (НЕ в repo tmp, НЕ в
  YandexDisk); project.json+files/ синкаются YandexDisk
  (`C:/Users/ROG/YandexDisk/gamedev/ai_studio/canvas_projects`).
- **Видео-конвейер v1** (ai_studio/assets/tools/video/): generate → frames
  → matte(CorridorKey|key_matte) → sheet; run.mjs оркестратор; конфиг-ключ
  `videoGenRoot` в studio.config.json.
- **Процедурная анимация** (T0260/T0264): ПОСТРОЕНА, но лидом ОТКЛОНЕНА
  для канваса («тут будет работа через видео») — код лежит спящим
  (element.animation, сэмплер, превью, animate-from-text). Выпил — по
  слову лида. Переиспользуемое: rAF-превью → будущий флипбук-плеер.

## Внешние папки (вне репо)

- `C:\projects\video_gen_experiment\` — ComfyUI portable + WAN 2.2 Q4 +
  Lightning (~28GB; модели передокачиваемы, README с инструкцией удаления);
  tools/CorridorKey (венв ~8GB, коммит запинен в README), tools/MatAnyone
  (резерв, НЕкоммерческая лицензия!); draft/final_workflow_api.json
  (профили скорости T0262); video_runs/ (golden-прогон крыльев);
  phase3/r2/compare/ + static_eval/ (доски улик вырезания, magenta_tricks).
- `C:/Users/ROG/AppData/Local/ai_studio/canvas_cache` — undo-история
  проектов (локальная по дизайну, между машинами не ездит).

## Ключевые доки (сохранены сюда из подметаемого tmp/)

canvas/docs/: VERIFY_2026-07-03.md (живой верифай-лист + решения лида),
design_video_anim_canvas_2026-07-05.md (ДИЗАЙН карточки анимации — по нему
строить T0265), research_T0256_* (анимационное исследование),
review_2026-07-04_SYNTHESIS.md + review_T0253/T0254_* (большое ревью, tier
2/3 бэклог внутри), research_cleanup_ux_*, design_T0232/33/39/42/44,
research_canvas/pipeline_map/perf (истоки), research_art_cleanup/
genart_workflow/style_objects/agy_refs, CHECKLIST 07-02, project_review.

video/docs/: t0257_phase3_report.md (GO/NO-GO эксперимента),
t0257_setup_log.md (полная хронология установки), research_T0262_speedup
(профили 35с/54с), research_video_api (облако: гибрид fal.ai = победитель),
research_corridorkey_static + magenta (вердикты + hue180),
research_video_anim_practice (FLF/LoRA/ToonCrafter — что реально на 12GB).

## Решения лида, принятые и действующие

- Анимации = ВИДЕО-путь (процедурные трансформы в канвасе отклонены).
- Раскадровка: начальный кадр обязателен, конечный опционален (Loop =
  конец=начало); удар = 3 кейфрейма (idle→апекс→idle) = piecewise FLF.
- Результат = редактируемая последовательность кадров (обрезка/удаление,
  fps, once/loop/pingpong-режим); спрайтшит/набор картинок = производный
  экспорт; кадры выровнены холстом, смещения не хранить.
- Экспорт = набор картинок; ping-pong = режим проигрывания (не запекать).
- Отдельный РЕЖИМ-сцена для настройки анимации (таймлайн) — одобрен.
- Matte дефолт для кадров = corridorkey; «арт со свечением генерим на
  ЗЕЛЁНОМ»; маджента = key_matte (шим — для явного выбора CK).
- LoRA: не превентивно; черновая ~30-60мин для проверки датасета (T0266).
- Лицензии: CorridorKey CC-BY-NC-SA с карв-аутом на обработку ассетов —
  принято; MatAnyone строго некоммерческий — только резерв.
- Облако: Veo/подписки — нет; кандидат на пилот <$5 — hosted WAN на fal.ai
  (гибрид: черновики локально, финалы облаком) — РЕШЕНИЕ ЛИДА НЕ ПРИНЯТО,
  вопрос открыт в VERIFY 00.

## Очередь на возврат (в порядке приоритета)

1. **T0265 — карточка анимации, инкремент 1** (todo, пакет готов):
   арт → карточка → Generate (черновик) → RGBA-флипбук играет на канвасе.
   Строить строго по design_video_anim_canvas_2026-07-05.md. Инкремент 2 =
   режим-сцена с таймлайном (обрезка/удаление/fps/режимы). Инкремент 3 =
   FLF (нужны FLF-workflow JSON — T0266).
2. **T0266** — FLF2V workflow + LoRA-замер (черновая/полная) + ToonCrafter
   пилот (backlog).
3. Вопрос лиду: пилот fal.ai (<$5) — да/нет (VERIFY 00).
4. Вердикты лида по VERIFY: пункты 1-5 (чат/fit/cleanup-окна/slice9),
   6-bis (key_matte: оставить/spill-gate/удалить за 2.9x), 8 (каллинг).
5. Tier 2/3 большого ревью (review_2026-07-04_SYNTHESIS.md) — по выбору.
6. Спящий процедурный код (T0260/T0264): выпилить или оставить — слово
   лида.

## Санитарные заметки

- Коммиты НЕ пушились (лид не просил); всё на локальном master. Для
  сохранности стоит запушить — решение лида.
- Сервер :8780 можно оставлять/гасить свободно — состояние в сторе.
- tmp/ подметается — НЕ хранить там ничего ценного; всё ценное уже здесь.
- Память ассистента: canvas-2d-conveyor.md (журнал вех) — обновлена
  паузой.
