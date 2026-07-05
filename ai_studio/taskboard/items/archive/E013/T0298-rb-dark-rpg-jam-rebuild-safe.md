---
id: T0298
title: "rb-dark-rpg jam: rebuild-safe цикл итерации и запуск всех тестов одной командой"
status: done
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, tech]
created: 2026-07-05
updated: 2026-07-05
---

## What

Линковка падает, пока запущен game.exe (lld-link permission denied), а ctest
не видит 8 тест-exe — их запускают вручную. Нужен безопасный цикл итерации на
время кранча. Фаза 0 (час 0-1).

## Done when

- [ ] Скрипт: убить game.exe → пересобрать target game → опционально перезапустить.
- [ ] Одна команда гоняет все 8 тестов и даёт суммарный pass/fail.

## Open questions

## Log
- 2026-07-05: tools/dev_rebuild.sh (taskkill+build+relaunch) и tools/run_tests.sh (8 тестов, timeout 60s = FAIL); 8/8 PASS проверено в сессии
