---
id: T0247
title: "Canvas: Alpha section - explicit method choice (Key matte | Dual-plate generate), drop Auto from UI"
status: review
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
- 2026-07-03: Lead 2026-07-03: 'не вижу выбора там keymatte и авто. Авто вообще как будто не нужно, я хочу явно выбирать'. Spec: renderAlpha method select = Key matte | Dual-plate (generate) - NO Auto option in UI (ops/CLI keep accepting auto additively); run button label follows method; dual path = new actions.js long-op wrapper over POST alpha-dual-generate (codex ~2-4min, progress toast, button disabled in flight, result element selected on success). BLOCKED on inspector.js until T0245 lands; orchestrator applies inline.
- 2026-07-03: Applied inline: explicit Key matte | Dual-plate (generate) select, no Auto in UI; batch = explicit Key matte button. Commits da384adc + b924aed4. F5 only - no server restart needed (route live since T0238).
