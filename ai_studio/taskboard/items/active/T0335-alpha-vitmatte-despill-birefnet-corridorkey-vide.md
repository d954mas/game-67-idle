---
id: T0335
title: "Alpha-портфель в прод: ViTMatte+despill и BiRefNet тулзы, ветки роутера, CorridorKey из video_gen_experiment в постоянный дом"
status: doing
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-07
updated: 2026-07-07
---

## What

По итогам альфа-бенча 2026-07-07: ввести в прод портфель вырезания —
ViTMatte (авто-тримап+дочистка, ниша «тонкое», запасной на свечении; гейт:
лицензия весов) и BiRefNet (ниша «произвольный фон», MIT) как тулзы
assets/tools/image/<tool> + ветки роутера + методы canvas alpha (parity).
CorridorKey остаётся первым приоритетом свечения — переезд из
video_gen_experiment/tools в постоянный дом ДО сноса папки. Таблица
«что и когда» -> README роутера. Бенч в репо не коммитится.

## Done when

- [ ] Лицензия весов ViTMatte проверена; вердикт записан (гейт п.1).
- [ ] Тулзы vitmatte_matte и birefnet_cutout живут по law per-tool+venv, роутер и canvas alpha их зовут (CLI/API/сайт parity).
- [ ] CorridorKey переехал, пути обновлены, video_gen_experiment можно сносить.
- [ ] Канвас-проект-улика с фикстурами/вырезками бенча создан и упомянут в логе.

## Open questions

## Log
- 2026-07-07: Скоуп по бенчу 2026-07-07 (лид ратифицировал приоритеты): (1) тул vitmatte_matte (авто-тримап из кей-фона + математическая дочистка) — ниша ТОНКОЕ (паутина/мех/волосы) + запасной на свечении; ГЕЙТ: лицензия весов Composition-1k до коммита; (2) тул birefnet_cutout (rembg, MIT) — ниша ПРОИЗВОЛЬНЫЙ ФОН (нюанс: слаб на лайн-арте); (3) ветки роутера assets/tools/image/route + методы canvas alpha (tool parity CLI/API/сайт); (4) CorridorKey — ПЕРВЫЙ приоритет свечения, переезд из video_gen_experiment/tools в постоянный дом до сноса папки, правка путей в канвас-вайринге и freeze-паспорте; (5) таблица маршрутизации -> README роутера; (6) бенч-скрипты/фикстуры в репу НЕ идут — улика = канвас-проект с результатами. Метрики: tmp/alpha_bench/final/ (worktree anim-card-t0265) + study_draft.md
- 2026-07-07: Старт (го лида 2026-07-07). Ветка alpha-portfolio-t0335 (worktree anim-card-t0265). Порядок: лицензия ViTMatte (гейт, ресёрч запущен) -> переезд CorridorKey (corridorKeyRoot) -> тулзы -> роутер/canvas parity -> улика.
- 2026-07-07: ГЕЙТ лицензии закрыт (ресёрч Opus, первоисточники): BiRefNet birefnet-general = MIT код+веса (репо LICENSE + HF card) — ALLOW коммит и коммерция; НЕ подменять на briaai/RMBG-2.0 (NC). ViTMatte vitmatte-base-composition-1k: код MIT, автор в issue#9 явно разрешил веса ('Feel free to use our model weights... MIT'), НО дообучение на Adobe DIM (Composition-1k) с noncommercial-условием на веса — вердикт ALLOW-WITH-CONDITIONS: local-only, веса не редистрибутим/не коммитим, каветс в README тула, второй приоритет. Финальное слово по коммерции аутпутов — за лидом.
- 2026-07-07: CorridorKey ПЕРЕЕХАЛ: C:/projects/ai_studio_tools/CorridorKey, конфиг corridorKeyRoot (env CORRIDOR_KEY_ROOT), venv жив (torch cu128+CUDA), live ck_smoke green+magenta ALL PASS с нового пути, канвас-сьют 710/710, паспорт/PAUSE_STATE/README правлены (коммит 13e9634c4). Снос video_gen_experiment для лида РАЗБЛОКИРОВАН. Тулзы vitmatte_matte + birefnet_cutout строятся параллельно (2 fast-worker).
- 2026-07-07: Канвас-улика создана: проект alpha-bench-2026-07-07-t0335-329849 (canvasProjectsRoot) — борды поста (hero_full, routing_full) + 12 кураторских вырезок по нишам (свечение/тонкое/призрак/произвольный фон/опак) + 6 заметок с вердиктами и указателем на полный архив YandexDisk alpha_bench_2026-07-07. Группы по рядам.
