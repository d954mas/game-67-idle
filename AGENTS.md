# AGENTS

## Repository Role

This repository is an AI Studio for game development: a shared agent/tooling
pipeline for creating game concepts, GDDs, assets, prototypes, release builds,
and liveops support.

The repository root is not a game. Templates live under `templates/`; games are
created under `games/`. Closed prototypes are git tags.

## AI Studio

`ai_studio/` is the target home for reviewed AI pipeline architecture: core
harness, agents, skills, tools, architecture maps, asset workflows, technical
workflows, and future production modules.

Reviewed AI pipeline modules live in `ai_studio/`. Compatibility entrypoints may
still exist in root, `.codex/skills/`, `.claude/`, and `tasks/`, but new
AI-pipeline docs/tools belong in the owning `ai_studio/` module.

## Current Game

Current game context lives in `GAME_PROJECT.md`.

Read `GAME_PROJECT.md` only when the task is about a specific game. Do not store
game lore, balance, roadmap, GDD detail, or per-game task state in `AGENTS.md`.

If there is no active game, do not start game implementation unless the lead
explicitly asks for it.

## Hard Invariants

- Engine boundary: use `external/neotolis-engine` public APIs before custom code.
- Game/world/UI logic is Y-up; convert Y-down input/platform data only at boundaries.
- All user-visible text uses the engine text renderer with real fonts; no handmade
  `draw_text`.
- Source assets before generating: shared library, then free CC0/OFL sources, then
  generation.
- Every committed asset must have license, provenance, integrity, and `origin`.
- Paid or non-redistributable binaries never enter git.

## Context Routing

- Workflow and commands: `ai_studio/README.md`.
- Task/status state: `ai_studio/taskboard/README.md` and the taskboard.
- AI Studio architecture: `ai_studio/README.md`, `ai_studio/tree.json`, and
  `ai_studio/architecture_map/README.md`.
- Detailed engine, workflow, validation, subagent, asset, and release procedures:
  load the matching `ai_studio/` module, doc, or skill only when the task needs it.
