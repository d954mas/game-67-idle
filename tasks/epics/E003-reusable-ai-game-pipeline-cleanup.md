---
id: E003
title: Reusable AI game pipeline cleanup
status: active
priority: P0
tags: [pipeline, ai-workflow, assets, profiling]
created: 2026-06-15
updated: 2026-06-15
---

## Goal

Improve the reusable AI-first game development pipeline after the fishing
prototype test: faster orientation, cleaner project boundaries, stronger visual
asset gates, and profiling evidence that helps the next game iteration without
blocking normal work.

## In scope

- Task/status hygiene after test iterations.
- Reusable asset pipeline structure and validation commands.
- Skills/AGENTS/taskboard rules that prevent repeated workflow mistakes.
- Profiling summaries that identify stalls, failed assumptions, and slow gates.
- Small tooling changes that make the next prototype easier to start cleanly.

## Out of scope

- More Splash Rods gameplay, content, or visual rescue work.
- Editing `external/neotolis-engine` unless explicitly requested.
- Web/browser prototype work unless the lead explicitly asks for it.
- Rewriting old game code unless it blocks reusable pipeline cleanup.

## Log

- 2026-06-15: Started after fishing iteration review. The next objective is
  pipeline optimization, not continuing the fishing prototype.
- 2026-06-15: Captured review-derived backlog: visual-first product gate,
  profiling coverage, commit/review hygiene, and clean-seed runtime isolation.
- 2026-06-15: Full pipeline review (docs + skills + tooling/profiling + the two
  failed prototypes). Root causes: gates validate artifacts/provenance, not fun
  or reference-match (Splash Rods + Rune Marches passed automation while the
  rendered screen was rejected); the visual/fake-shot gate existed
  (T0020/T0027/T0030/T0032) but stayed advisory and post-hoc; speed lost to
  all-`--full` validate (126 runs/34h), profiling-as-second-project (172
  sidecars, 70% of a day's records were validation), and additive machinery
  (T0035-T0042 validation planner). Logged prioritized backlog `T0043`-`T0052`
  spanning speed, subtraction, and quality. Net principle: next pipeline edits
  must subtract (flip defaults, make gates bind, delete machinery), not add.
