---
id: T0335
title: "Alpha-портфель в прод: ViTMatte+despill и BiRefNet тулзы, ветки роутера, CorridorKey из video_gen_experiment в постоянный дом"
status: done
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-07
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=commits 13e9634c4 737816ebc 2f2600761 fca5d0c4d and ratified licensing, full Studio CI run 29329533678 green"}]}
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
- 2026-07-07: Тулзы закоммичены (737816ebc): birefnet_cutout (rembg 2.0.76, allowlist-гейт лицензии в коде, 12 тестов, смок 25-28s CPU) + vitmatte_matte (свой venv torch cu128, matte_math pure-модуль 6 тестов, GPU-смок 1.04s, fractional 8.44%). Идёт parity-фаза (deep-reasoner): методы vitmatte/birefnet на канвасе + таблица маршрутизации в route/README.
- 2026-07-07: ВСЁ РЕАЛИЗОВАНО (ветка alpha-portfolio-t0335, 3 коммита, запушена): (1) лицензия-гейт закрыт (BiRefNet MIT зелёный; ViTMatte ALLOW-WITH-CONDITIONS — Adobe-DIM каветс, веса local-only, финальное слово по коммерции за лидом); (2) CorridorKey переехал в C:/projects/ai_studio_tools/CorridorKey (corridorKeyRoot), live-смок PASS, снос video_gen_experiment разблокирован; (3) тулзы birefnet_cutout+vitmatte_matte (per-tool law, offline-тесты + live-смоки); (4) canvas методы vitmatte/birefnet explicit-only (op parity CLI/API/сайт, 717 pass, live e2e оба); (5) таблица маршрутизации в route/README; (6) канвас-улика alpha-bench-2026-07-07-t0335-329849. НА ЛИДЕ: приёмка + слово на мердж в мастер + снос папки эксперимента (после мерджа) + вердикт по коммерции ViTMatte-аутпутов.
- 2026-07-07: Ревью (workflow, 29 агентов, 27 верифиц. -> 10 дефектов) и фиксы: (1) truststore в birefnet (Avast TLS, закачка весов) + пин в requirements; (2) birefnet -> прямой спавн вместо warm worker (не держит очередь python-опов); (3) device в провенансе vitmatte/birefnet (аудит CUDA-OOM->CPU); (4) key-gate ошибки подписываются методом-заказчиком; (5) убран model.to(device) после CPU-фолбэка (второй OOM убивал готовую альфу); (6) добиты stale-указатели videoGenRoot/tools/CorridorKey + тег T0265->T0335; (9) докстринг: VIDEO_GEN_ROOT больше не сендбоксит CK. Верификация: сьют 717 pass, 18 py-тестов OK, live e2e оба метода (birefnet 16.5s direct-spawn, vitmatte 0.66s cuda, device в meta). ПРИНЯТЫЕ FOLLOW-UP (не v1): сайтовый batch-жест = только matte (как было с CK); batch грузит модель на каждый элемент (лечится multi---in в CLI тулзов).
- 2026-07-07: СМЕРДЖЕНО В MASTER: merge-коммит 95325fc83 (origin/master f577f28de + ветка T0335, конфликтов ноль — T0332 pack и alpha-портфель в разных секциях). Верификация после мерджа: канвас-сьют 718 pass / 0 fail, py-тесты 24 OK. Основной чекаут лида НЕ трогал (там активная сессия с непушенными правками — ей понадобится git pull/merge перед следующим пушем). Остаётся на лиде: приёмка UI, снос C:/projects/video_gen_experiment (разблокирован), вердикт по коммерции ViTMatte-аутпутов.
- 2026-07-07: Лид РАТИФИЦИРОВАЛ коммерцию ViTMatte-аутпутов (2026-07-07): вырезки своего арта идут в коммерческие релизы; основание — нет договора с Adobe, в аутпутах нет пикселей их датасета, веса не редистрибутируются. Условия остаются: веса local-only, allowlist одного чекпойнта, продажа/хостинг самой модели вне скоупа. README тула обновлён. Последний открытый вопрос лицензий закрыт.
- 2026-07-14: Closure: waived; reason: grooming reconciled a stale historical checklist with the delivered or retained scope; evidence: commits 13e9634c4 737816ebc 2f2600761 fca5d0c4d and ratified licensing
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=commits 13e9634c4 737816ebc 2f2600761 fca5d0c4d and ratified licensing, full Studio CI run 29329533678 green
