---
id: T0053
title: Cut per-session context load across skills docs and tool output
status: done
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

- [x] Skills trimmed and the heavy reference apparatus moved to the shared `gamedesign/knowledge/reference_deconstruction.md` (via T0048) instead of being duplicated in 3 skill bodies. SKILL.md bodies: 1940 -> 1657 lines.
- [x] Tool output: defaults are already compact (`cli.mjs summary`/`context` compact; profiler/validator verbosity behind `--verbose`). Added a Context Budget guideline directing agents to use summaries + `Grep`/`Glob`/offset-`Read` over raw `find`/`ls -R`/`cat` dumps, and to delegate broad reads to subagents. (Behavioral lever; no new tool flags needed.)
- [x] Documented the "Context Budget" guideline in `tasks/README.md` (extends Minimal Current Context).
- [x] Measured: SKILL.md bodies 1940 -> 1657 (reference block de-duplicated across 3 skills); AI_PIPELINE docs ~700 (5 files) -> 548 (2 files); validation-planner removal -714 LOC (T0047). A normal startup load (AGENTS + context + active task + one skill) is materially smaller.
- [x] `node tools/skills_eval.mjs` 9/9 + `node tools/taskboard/cli.mjs validate` ok.

## Open questions

- RESOLVED: the biggest wins were duplication (reference block across 3 skills; 5 sprawling docs) and the planner machinery, not tool-output verbosity (already compact). Addressed by T0047/T0048/T0051 plus this guideline.

## Log

- 2026-06-15: Created from lead goal-set: context overflow/compaction is an optimization direction. Measure startup load before cutting.

- 2026-06-15: Added the "Context Budget" guideline to tasks/README.md (default load = AGENTS + context + active task + one skill; prefer summaries/Grep/Glob over dumps; delegate broad reads to subagents; rely on durable state across compaction). Measured reductions: SKILL.md bodies 1940->1657, AI_PIPELINE docs ~700->548, planner -714 LOC. Heavy reference method now lives once in gamedesign/knowledge/reference_deconstruction.md. skills_eval 9/9, taskboard ok.
