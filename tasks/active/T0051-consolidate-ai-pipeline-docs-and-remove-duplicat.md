---
id: T0051
title: Consolidate AI_PIPELINE docs and remove duplicated rules
status: backlog
epic: E003
priority: P2
tags: [docs, subtraction]
created: 2026-06-15
updated: 2026-06-15
---

## What

Five root docs (AI_PIPELINE, AI_PIPELINE_ITERATION_LOG,
AI_PIPELINE_OBSERVABILITY_TOOLS, AI_PIPELINE_RETROSPECTIVE_2026-06-13,
AI_PIPELINE_SESSION_PROFILING; ~700 lines). The "tools stay passive/advisory"
rule is stated 4x; the native-PC gate 2x; profiling guidance is split across 3
docs; `observability_gate.mjs` (184 LOC) only decides whether to adopt other
observability tools. Consolidate to one short pipeline page + a dated history
file; state each rule once; delete `observability_gate.mjs`.

## Done when

- [ ] One canonical pipeline doc; ITERATION_LOG / RETROSPECTIVE moved to a dated history/archive location (kept, not deleted).
- [ ] Passivity rule and native-PC gate each stated once, with other places pointing to it.
- [ ] `observability_gate.mjs` removed (or justified if actually used).
- [ ] `node tools/taskboard/cli.mjs validate` passes; no broken cross-references.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 5 AI_PIPELINE docs ~700 lines; passivity rule stated 4x; native-PC gate 2x.
