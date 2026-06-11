---
id: T0010
title: Retire implementation_tasks.json in favor of tasks/ store
status: backlog
epic: ""
priority: P2
tags: [ai-pipeline, cleanup]
created: 2026-06-11
updated: 2026-06-11
---

## What

Remove the duplicate work-tracking source gamedesing/fantasy-pocket-rpg/data/implementation_tasks.json
now that tasks/ (E001, T0001-T0005) is canonical per AGENTS.md policy.

## Done when

- [ ] gamedesing/fantasy-pocket-rpg/site.js no longer loads implementation_tasks.json
- [ ] tools/validate_site.mjs and tools/validate_package.py no longer require it
- [ ] handoff_status.md source-of-truth list updated to point at tasks/
- [ ] implementation_tasks.json deleted

## Open questions

- Coordinate with the parallel GDD session: site.js, validators, and handoff_status.md
  are currently modified in the working tree by other work. Execute after that lands.

## Log

- 2026-06-11: Investigated references: site.js renders it on the visual GDD site;
  both validators require it; handoff_status.md lists it as source of truth #10.
  Blocked until parallel gamedesing/ changes are committed.
