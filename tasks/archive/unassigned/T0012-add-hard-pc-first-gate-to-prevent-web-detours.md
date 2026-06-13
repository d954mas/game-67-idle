---
id: T0012
title: Add hard PC-first gate to prevent web detours
status: done
epic: ""
priority: P1
tags: [pipeline, process, validation]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add an explicit process guard so agents do not create or validate web
prototypes for playable game work unless the user explicitly asks for web or
approves an exception.

## Done when

- [x] `AGENTS.md` contains a hard native-PC-first gate.
- [x] `AGENTS.md` requires explicit user permission before web server, browser,
      HTML/CSS/JS prototype, or frontend tooling work.
- [x] `AI_PIPELINE.md` repeats the rule as a reusable process guard.
- [x] The rule names the correct response: improve the native PC slice instead
      of switching platforms.

## Open questions

None.

## Log

- 2026-06-12: Added hard PC-first gates to `AGENTS.md` and `AI_PIPELINE.md`
  after an incorrect web-prototype detour during 67 World playable work.
