---
id: T0242
title: "Studio site: AI chat panel on canvas - selection as context, agent acts via ops"
status: doing
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03): "Что если на сайт добавить возможность общаться с ИИ.
Если бы был чат, я бы мог выделить картинку и текстом попросить сделать
чёрную версию, или дуал путь" — an AI chat panel in the canvas page: the
current SELECTION rides along as context, the agent answers in the panel
and ACTS on the project through the same ops layer.

Why it fits the architecture as built: STRICT tool parity means the agent
can already do everything the page can (canvas CLI over the same ops);
agent steps already carry 🤖 actor attribution in the history panel; the
manual loop exists today (Copy ID -> paste canvas:// ref into agent chat).
This feature = automate that loop in place.

Concept sketch (design phase to settle):
- Chat panel (collapsible, right side or bottom) per canvas project;
  message -> POST to a local chat endpoint; the CURRENT selection is
  auto-attached as canvas:// refs (same format Copy ID emits).
- Backend spawns/talks to a headless agent session (claude CLI -p /
  session-per-project; codex exec as alt backend) primed with the
  nt-canvas-operations skill; the agent mutates via canvas CLI -> journal
  entries appear with 🤖 as usual.
- Page refresh: after each agent turn the panel triggers a project reload
  (or SSE/poll while a chat turn is in flight) so his canvas updates
  without manual reload.
- Sessions persist per project (chat history restorable); long ops stream
  progress into the panel like runLongOp toasts.

## Done when

- [ ] Design phase: backend choice, session model, refresh mechanism,
      permissions/cost guardrails, v1 scope + increments.

## Open questions

- v1 backend: claude CLI headless vs codex exec (or both via the
  cross-harness protocol)?
- How the page learns about agent-made changes mid-chat (poll vs SSE).
- Priority vs T0238/T0207/T0239 queue — lead to place it.

## Log

- 2026-07-03: created from lead proposal during live session.
- 2026-07-03: Design phase launched (deep-reasoner, doc-only): AI chat panel on the studio site - select elements, command in text, agent drives the SAME ops layer.
