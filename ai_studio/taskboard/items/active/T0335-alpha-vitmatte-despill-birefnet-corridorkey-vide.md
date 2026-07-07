---
id: T0335
title: "Alpha-портфель в прод: ViTMatte+despill и BiRefNet тулзы, ветки роутера, CorridorKey из video_gen_experiment в постоянный дом"
status: backlog
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
