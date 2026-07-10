---
id: T0352
title: Decompose Canvas ops, UI, chat docs, and local configuration behind stable facades
status: backlog
project: P001
epic: E015
priority: P1
tags: [canvas, architecture, decomposition]
created: 2026-07-10
updated: 2026-07-10
---

## What

Reduce Canvas context cost by splitting the large ops/UI/docs surfaces along
existing domains while retaining stable public facades and identical behavior.

This task owns physical module/file relocation after `T0351` has transport
parity. `T0351` changes transport behind the current boundary and must not
pre-emptively repeat this structural move.

## Done when

- [ ] `ops.mjs` remains the public facade while implementation moves one domain
      at a time with unchanged exports and focused parity tests.
- [ ] `inspector.js` and `workspace.js` are split behind stable UI facades with
      no DOM/CSS/interaction redesign.
- [ ] Canvas Chat runtime/docs move under Canvas ownership; Studio Shell keeps
      only hosting integration.
- [ ] Canvas README becomes a short router to domain contracts; the Canvas skill
      loads only the contract required by the request.
- [ ] Machine-local paths leave tracked `studio.config.json` for the existing
      ignored local override.
- [ ] No services layer, DI container, event system, or speculative framework is
      introduced.

## Open questions

## Log

- 2026-07-10: Mechanical decomposition only; security behavior belongs to
  `T0350`/`T0351`.
