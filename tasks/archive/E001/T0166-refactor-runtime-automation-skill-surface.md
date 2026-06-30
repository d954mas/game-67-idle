---
id: T0166
title: Refactor runtime automation skill surface
status: done
epic: E001
priority: P2
tags: [runtime, skill, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What

Refactor the legacy `game-runtime-automation` skill into the reviewed AI Studio
runtime module as `nt-runtime-automation`.

The runtime automation code already lives in `ai_studio/runtime_automation/`.
The remaining legacy risk is that agents can still trigger old skill guidance
with stale `tools/devapi` paths or duplicated visual quality policy.

## Done when

- [x] Skill is renamed to `nt-runtime-automation`.
- [x] Skill points at `ai_studio/runtime_automation/` helpers.
- [x] Old `tools/devapi` references are removed from current skill docs.
- [x] Runtime Automation tree owns the skill and references.
- [x] Generated `.claude/skills` pointers are synchronized.
- [x] Runtime tests and AI Studio validation pass.

## Open questions

None.

## Log

- 2026-07-01: Started after `tools/devapi` moved to
  `ai_studio/runtime_automation`; selected old `game-runtime-automation` as the
  next legacy surface.
- 2026-07-01: Renamed skill to `nt-runtime-automation`, shortened references,
  updated paths to `ai_studio/runtime_automation`, and added the skill to the
  Runtime Automation map node.
- 2026-06-30: Runtime automation skill surface refactored to nt-runtime-automation; old generated pointer removed; validation passed.
