---
id: T0242
title: "Studio site: AI chat panel on canvas - selection as context, agent acts via ops"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-10
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

- [x] Design phase: backend choice, session model, refresh mechanism,
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
- 2026-07-03: Lead challenged spawn-per-message; verified codex exec resume exists on the box. Design R3: conversation = codex session (resume per message, no history replay, no resident daemon); clear context = new session; transcript jsonl = panel display only; replay window demoted to fallback.
- 2026-07-03: Backend inc 1-3 LANDED+committed (chat/context+agent+api, codex sessions w/ live MANGO continuity proof, SSE, 51 chat tests, canvas 457 intact, :8780 restarted+smoked). Panel worker (inc 4-5) launched: site/chat_panel.js follows history_panel pattern, SSE via fetch stream, selection chips, Cancel keeps session, Clear rotates; README.
- 2026-07-03: Inc 4-5 landed+committed: chat panel docked column (toggle, SSE stream, selection chips, ops range chips, Cancel keeps session, Clear two-step confirm) + chat/README. All 5 increments done. Suites canvas 457 / chat 51. :8780 restarted, smoke 200. Awaiting lead browser verify.
- 2026-07-11: T0375 reconciliation: the design criterion was already satisfied by the recorded R1-R3 decisions and the implementation evidence above. The prompt-only permission model and transport are intentionally superseded by E015 T0350/T0351; this card owns no remaining implementation.
- 2026-07-11: Quality: QTECH_001=pass; evidence: existing 457 Canvas tests, 51 chat tests, HTTP smoke, and explicit E015 successor ownership recorded above.
- 2026-07-11: T0375 status reconciliation: done; its sole design criterion is checked from recorded R1-R3 decisions and 457 Canvas/51 chat test evidence. E015 T0350/T0351 own successor permission/transport changes.
