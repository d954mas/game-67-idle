---
id: E004
title: Clean-project pipeline trial
status: done
priority: P1
tags: [ai-pipeline, trial]
created: 2026-06-12
updated: 2026-06-12
---

## Goal

Prepare a clean exported project foundation for the user's next game idea,
without old fantasy RPG history, archived tasks, game-specific files, or an
agent-invented concept.

## In scope

- Export portable base into a fresh external temp project.
- Ensure starter `AGENTS.md` and `tasks/STATUS.md` tell agents to wait for the
  user's game concept.
- Keep task history empty in the exported project.
- Keep only reusable pipeline files, skills, taskboard tooling, and reusable
  design knowledge.
- Record any cleanup/pipeline friction found during preparation.

## Out of scope

- Cleaning the old dirty game/runtime worktree.
- Inventing the game concept.
- Creating GDD/content/assets before the user provides the concept.
- Shipping a complete game.
- Editing `external/neotolis-engine`.
- Committing generated trial project contents into this repo unless explicitly
  requested.

## Log

- 2026-06-12: Started clean-project trial after E003 hardening completed.
  Updated direction after user clarified the project idea will come from them:
  prepare a clean base only, with no old task history and no invented concept.
- 2026-06-12: Completed clean base preparation at
  `C:\tmp\clean-game-base-20260612`. The base has no tasks, no old game
  history, no `gamedesing` path, canonical `gamedesign/knowledge`, and starter
  instructions that require waiting for the user's concept before GDD/content.
