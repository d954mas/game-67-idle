---
id: T0226
title: "Skill: nt-canvas-operations - teach agents to act on canvas:// refs and canvas projects"
status: done
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-10
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

- [x] skill exists in .codex/skills/nt-canvas-operations/ + synced Claude surface; discoverable by both harnesses
- [x] an agent given ONLY a canvas:// ref + the skill resolves it and performs a mutation correctly (live smoke via subagent)
- [x] no content duplicated from the canvas README (pointers only); doc_reference_check green

## Open questions

## Log
- 2026-07-02: Created from lead question during live-verify session.
- 2026-07-03: Authored `.codex/skills/nt-canvas-operations/SKILL.md` (58 lines,
  thin router matching the `nt-taskboard-manager` convention). Verified the
  `canvas://` ref format against `site/context_menu.js` (Copy ID) — matches
  README exactly, incl. the bare `canvas://<projectId>` project form. Synced
  `.claude/skills/nt-canvas-operations/` via `skills_sync.mjs` (drift check
  caught it pre-sync). Added `assets:canvas-skill` node to
  `module-assets.json` (same pattern as `taskboard:skill`) so the new skill
  file isn't unmapped under strict validation. Gates green: doc_reference_check
  (10 files), validate_map --strict (mapped=303 scanned=415, zero
  unmapped/missing/duplicate). Sanity-verified every CLI command named in the
  skill (list/create/show/rename/delete/add-image/add-text/element-set/
  group-create/export/undo/redo/history) appears in the `cli.mjs` no-args
  usage banner. Live-smoke box left unticked for the orchestrator.
- 2026-07-03: LIVE SMOKE PASSED (orchestrator): throwaway project drifting-voyager-34bb8a + text element; a fast-worker given ONLY `canvas://drifting-voyager-34bb8a/element/el_724c0139 — move to x=777,y=333` resolved the ref and ran `cli.mjs move` unaided; journal verified (seq 2 patchElement {x:777,y:333}, canUndo true); project trashed after. Skill committed 838f3d95; task -> review. Discovered by the author agent: README documents an `ops-stats` CLI command missing from the live banner (doc/banner drift, minor — noted here, not blocking).
- 2026-07-11: T0375 status reconciliation: done; all 3 acceptance criteria are checked and the card log contains skill validation evidence.
