---
id: T0244
title: "Canvas: smart guides + snap while dragging (Figma-style alignment guides to edges/centers of siblings and parent frame)"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Deep-reasoner design packet launched: smart guides + snap during drag (Figma-style), builds on T0236 world-space drag pipeline.
- 2026-07-03: Design packet done: tmp/design_T0244_smart_guides_2026-07-03.md (no op/CLI/API surface - pure UI drag aid; snap.mjs pure math + workspace.js integration; Ctrl/Cmd bypass; v1 = line guides+snap only). 3 open questions for lead (frame center lines / region drags / bypass key). Increment 1 (pure math, new files only) launched with recommended defaults.
- 2026-07-03: Lead decision Q2: region drags get NO snap - regions are pixel work, neighbor alignment is meaningless there. v1 exclusion confirmed permanent. Q1 (frame centers) and Q3 (Ctrl/Cmd bypass) still on recommended defaults.
- 2026-07-03: Lead delegated Q1/Q3 to orchestrator. Decisions: Q1 = all 6 parent-frame lines incl. centers (centering a widget in a screen is the primary use case; reversible filter if stickiness annoys). Q3 = Ctrl/Cmd bypass (Figma muscle memory; composes with deep-select). All design questions closed.
- 2026-07-03: Increment 1 landed: snap.mjs + 16 headless tests, reviewed, committed. Increment 2 (workspace.js integration + guide overlay + Ctrl/Cmd bypass) launched.
- 2026-07-03: Increment 2 landed + committed: snap live in all three move-drag branches, pink guides, Ctrl/Cmd bypass. 20/20 gate tests + syntax OK. Awaiting lead live verify (page reload only - site JS).
- 2026-07-03: Accepted by lead 2026-07-03 (items 1-6 verify batch)
