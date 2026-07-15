# AGENTS

## Repository Role

This repository is an AI Studio for game development: a shared agent/tooling
pipeline for creating game concepts, GDDs, assets, prototypes, release builds,
and liveops support.

The repository root is not a game. Templates live under `templates/`; reusable
feature packs live under `features/`; games are created under `games/`. Closed
prototypes are git tags.

## Communication

Minimize optional commentary. During execution, send only required notices,
blocking questions, meaningful progress updates, risk/verification notes, and
the final result.

## AI Studio

`ai_studio/` is the target home for reviewed AI pipeline architecture: core
harness, agents, skills, tools, architecture maps, asset workflows, technical
workflows, and future production modules.

Reviewed AI pipeline modules live in `ai_studio/`. New AI-pipeline docs/tools
belong in the owning `ai_studio/` module; do not add new root-level compatibility
paths.

Mechanical gates and the advisory boundary are summarized in
`ai_studio/core_harness/workflow/README.md#enforcement-boundary`.

## Agent Roles

Reusable agent roles live in the active harness catalog. Codex custom agents
live in `.codex/agents/*.toml`; Claude agents live in `.claude/agents/*.md`.
Delegate only when an independent bounded packet materially reduces latency,
context load, or review risk. Keep coherent local work, including related
multi-file changes, with the lead when context transfer and reintegration would
cost more than direct work.

When delegating, use the closest existing role, a short
Task / Scope / Return / Stop packet, and integrate and verify the result. Reuse
an existing suitable agent instead of creating disposable roles. If host policy
requires explicit approval, ask once per chat/session and reuse that approval
while scope remains in this repository.

Review budget follows risk: mechanical documentation, moves, and obvious edits
need no independent reviewer; normal logic gets one; security, concurrency, and
release work gets two independent reviewers. Repeat review only after a
high-risk finding or contract change.

## Current Game

Current game context lives under `games/<game-id>/`.

When the task is about a specific game, use the explicit game id from the user or
infer it only when there is exactly one game folder. Do not store game lore,
balance, roadmap, GDD detail, or per-game task state in `AGENTS.md`.

Game-specific knowledge, GDDs, reference lessons, playtest findings, and accepted
design facts live under `games/<game-id>/design/`. Use
`games/<game-id>/design/knowledge/` as that game's private knowledge base.

If there is no active game, do not start game implementation unless the lead
explicitly asks for it.

## Hard Invariants

- Engine boundary: use `external/neotolis-engine` public APIs before custom code.
- The engine working tree is read-only for agents. Suspected engine problem:
  first establish where the ROOT CAUSE lives — the existence of a game/template
  workaround does not settle it (a workaround can be a hack that leaves the
  real defect in the engine). If the root cause is in the engine, convince the
  lead the fix is needed; every engine change ships only through an issue and
  PR in the engine repo — never a direct edit.
- Game/world/UI logic is Y-up; convert Y-down input/platform data only at boundaries.
- All user-visible text uses the engine text renderer with real fonts; no handmade
  `draw_text`.
- Source assets before generating: shared library, then free CC0/OFL sources, then
  generation.
- SVG/vector/procedurally drawn art is a direction mockup only. It never becomes a
  game asset unless the lead explicitly asked for or approved it; real game art
  goes through the asset pipeline (source-first, then raster generation).
- Every committed asset must have license, provenance, integrity, and `origin`.
- Paid or non-redistributable binaries never enter git.

## Context Routing

- Workflow and commands: `ai_studio/README.md`.
- Reusable agent roles: the active harness catalog, such as `.codex/agents/`
  or `.claude/agents/`.
- Task/status state: `ai_studio/taskboard/README.md` and the taskboard.
- AI Studio architecture: `ai_studio/README.md`, `ai_studio/tree.json`, and
  `ai_studio/architecture_map/README.md`.
- Templates, games, and features: `templates/README.md`, `games/README.md`,
  and `features/README.md`.
- Shared reusable game-development knowledge: `ai_studio/game_design/knowledge_base/README.md`.
- Game-specific design knowledge and GDDs:
  `games/<game-id>/design/README.md`.
- Detailed engine, workflow, validation, subagent, asset, and release procedures:
  load the matching `ai_studio/` module, doc, or skill only when the task needs it.
