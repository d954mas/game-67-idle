---
id: T0053
title: Cut per-session context load across skills docs and tool output
status: backlog
epic: E003
priority: P1
tags: [context, speed, skills, docs, ai-workflow]
created: 2026-06-15
updated: 2026-06-15
---

## What

The lead reports context windows overflow and compact often. The pipeline loads
heavy context every session: large skills (generated-game-ui-assets 538 lines,
game-asset-pipeline 389, game-visual-art-direction 289), 5 root AI_PIPELINE docs
(~700 lines), duplicated reference blocks (~150 lines across 3 skills), and
verbose tool output (e.g. find/list dumps, full-status reads). Reduce the
standing context cost of a normal iteration so the agent compacts less.

This direction overlaps with but is distinct from T0048 (merge skills) and
T0051 (merge docs): those cut duplication; this one targets the *loaded*
footprint and tool-output verbosity.

## Done when

- [ ] Skills trimmed so the always-loaded SKILL body is lean; heavy apparatus moved to `references/` loaded on demand (coordinate with T0048).
- [ ] Tool output is quiet by default: summaries/counts not full dumps; long lists behind a flag (taskboard, profiler status, validators).
- [ ] A documented "context budget" guideline: what an agent loads by default vs on demand (extends the Minimal Current Context rule in `tasks/README.md`).
- [ ] Measured before/after: approximate token/line load of a normal startup (`iteration_context` + active task + relevant skill) drops materially.
- [ ] `node tools/skills_eval.mjs` + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

- Is the biggest win in skills, in docs, or in tool-output verbosity? Measure first.

## Log

- 2026-06-15: Created from lead goal-set: context overflow/compaction is an optimization direction. Measure startup load before cutting.
