---
id: E003
title: AI pipeline hardening
status: done
priority: P1
tags: [ai-pipeline, tooling]
created: 2026-06-12
updated: 2026-06-12
---

## Goal

Harden the reusable AI game-development pipeline so future agents start with a
small current context, keep one source of truth per work type, and validate
workflow changes before they become reusable project base.

## In scope

- Task/status source-of-truth rules in `tasks/`.
- Portable pipeline process in `AI_PIPELINE.md`.
- Reusable project skills in `.codex/skills/`.
- Taskboard, skill sync, skill eval, export-base, and other reusable tooling.
- Small validators that prevent context bloat, duplicate task state, or
  unrefined actionable work.

## Out of scope

- Product/gameplay implementation for the fantasy RPG testbed.
- Editing `external/neotolis-engine`.
- Large speculative workflow rewrites without a failing example or explicit
  user request.
- Moving historical archived evidence back into default context.

## Log

- 2026-06-12: Completed hardening pass. Closed active/archive task split,
  retired duplicate implementation task data, added intent-to-scope and
  checkpoint rules, added skill regression evals, added anti-entropy taskboard
  validation and remediation hints, froze legacy `gamedesing/` only for this
  testbed while exporting corrected `gamedesign/`, added taskboard Markdown
  preview, and added `node tools/pipeline_validate.mjs` as the reusable-base
  gate.
