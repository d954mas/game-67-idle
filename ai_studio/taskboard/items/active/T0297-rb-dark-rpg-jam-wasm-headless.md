---
id: T0297
title: "rb-dark-rpg jam: долинковать wasm-билд и headless-смоук"
status: todo
project: P003
epic: E013
priority: P1
tags: [rb-dark-rpg, jam, tech, wasm]
created: 2026-07-05
updated: 2026-07-05
---

## What

В build/wasm-debug только game.data — линковка .wasm/.js/.html не завершилась.
Это риск №1 для сдачи на джем: проверить в первый час, а не в 23-й. emcc лежит в
C:/develop/emsdk (не на PATH — нужен emsdk_env). Фаза 0 (час 0-1).

## Done when

- [ ] `cmake --build build/wasm-debug` доходит до game.html/.js/.wasm без ошибок.
- [ ] Билд отдан через `python -m http.server`, первый экран рендерится в Chrome `--headless=new` (SwiftShader WebGL2); скрин в Log.
- [ ] Ассеты/аудио из game.data грузятся без ошибок в консоли.

## Open questions

## Log
