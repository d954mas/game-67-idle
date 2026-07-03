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
- 2026-07-03: Design done: tmp/design_T0242_chat_panel_2026-07-03.md. Chat = thin transport around the existing loop: headless agent spawn per message (codex exec default, injectable seam), page sends {message, projectId, selection refs}, agent drives cli.mjs (one-ops-layer by construction), T0234 expect-head inherited, SSE stream + cancel, replies list journal seq ranges. 5 increments in new ai_studio/studio_shell/chat/. Open Qs for lead: codex vs claude backend; may v1 CREATE objects or selection-only; queue priority.
- 2026-07-03: Lead: chat must support a working session - persistent per-project history + Clear context button (design R1). Spawn-per-message stands; history replayed into each prompt (bounded window), transcript jsonl persisted + archived on clear. Remaining open Qs: codex vs claude backend; may v1 create objects; queue priority.
- 2026-07-03: Lead decisions (design R2): backend = codex exec EXPLICITLY (no claude branch; seam stays for tests). Permissions = any requested op EXCEPT destructive/non-undoable - project deletion denied by name; rule in system prompt + loud post-check (honest: no hard sandbox in v1, terminal-equivalent trust). Queue: after T0239. All design questions closed - ready to build when files free up.
