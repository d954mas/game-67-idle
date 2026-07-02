---
id: T0226
title: "Skill: nt-canvas-operations - teach agents to act on canvas:// refs and canvas projects"
status: backlog
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Lead (2026-07-02): "есть ли у меня скилл, нужен ли скилл, который обьясняет ии
агенту как он может работать с канвасом?" - there is none; the generation
skill only mandates the canvas HANDOFF. Real friction exists: the Copy ID
workflow (canvas:// refs pasted into agent chat) needs any agent - Claude or
Codex - to resolve refs and act without hunting for docs.

Author a THIN skill `nt-canvas-operations` in .codex/skills/ (synced to
.claude/skills via ai_studio/core_harness/agent_surfaces/skills_sync.mjs):
- canvas:// ref format and how to resolve (CLI `show <projectId>`, find
  element/group/region by id).
- The CLI surface pointer: node ai_studio/assets/canvas/cli.mjs (usage banner
  is self-documenting) + HTTP API existence for page parity.
- The LAWS an agent must respect: non-destructive (files/ immutable), every
  mutation journaled (undo must restore), tool parity (never bypass ops),
  projects root on YandexDisk via studio.config.json.
- Routing: ai_studio/assets/canvas/README.md is the single source of truth -
  the skill points, never duplicates.
- When to use: any request mentioning canvas://, canvas projects, screens/
  groups/slicing/export on the canvas.

Keep it LEAN (lean-process law: skills grow from real friction, cut meta).

## Done when

- [ ] skill exists in .codex/skills/nt-canvas-operations/ + synced Claude surface; discoverable by both harnesses
- [ ] an agent given ONLY a canvas:// ref + the skill resolves it and performs a mutation correctly (live smoke via subagent)
- [ ] no content duplicated from the canvas README (pointers only); doc_reference_check green

## Open questions

## Log
- 2026-07-02: Created from lead question during live-verify session.
