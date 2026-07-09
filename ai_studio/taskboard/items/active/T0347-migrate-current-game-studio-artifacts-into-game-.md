---
id: T0347
title: Migrate current game Studio artifacts into game stores
status: backlog
project: P001
epic: E014
priority: P1
tags: [migration, rb-dark-rpg, taskboard, canvas]
created: 2026-07-09
updated: 2026-07-09
---

## What

Migrate current game-specific Studio artifacts into game-owned `.ai_studio/`
stores after privacy and mount-aware gates are implemented.

`rb-dark-rpg` is the first migration target. This task must not start file moves
until `T0342`, `T0344`, `T0345`, and relevant generator hardening from `T0346`
are done.

## Done when

- [ ] Preflight proves the parent Studio repo will not track, stage, gitlink, or
      leak the target game-owned stores.
- [ ] Inventory enumerates current `rb-dark-rpg` Taskboard project/epic/task
      files, archive files, Canvas projects, evidence paths, registry entries,
      `.vscode` entries, docs/tests references, and provenance/manifest records.
- [ ] Migration plan distinguishes reusable public fixture references from
      game-owned private work state.
- [ ] Game-owned taskboard items move from `ai_studio/taskboard/items/` into
      `games/rb-dark-rpg/.ai_studio/taskboard/items/` only after Taskboard can
      read/write mounted game stores.
- [ ] Game-owned Canvas projects move from
      `ai_studio/assets/canvas/projects/` into
      `games/rb-dark-rpg/.ai_studio/canvas/projects/` only after Canvas v2 refs
      and store-routed writes exist.
- [ ] Canvas project folders are preserved together: `project.json`, `files/`,
      `export/`, `tool_runs.jsonl`, `errors.jsonl`, and local chat/history
      sidecars.
- [ ] Evidence, provenance, manifests, screenshots, and runtime logs move or
      relink into the owning game store where they are game-specific.
- [ ] Studio-level references that remain are sanitized public fixture notes or
      migration notes, not private task/canvas/evidence content.
- [ ] Old public history is not rewritten by this task; any history scrub is
      captured as separate explicit work if the lead asks for it.
- [ ] Final leak scan reports no private IDs, paths, remotes, gitlinks, task
      metadata, Canvas refs, or evidence paths in tracked public Studio files.
- [ ] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Does `rb-dark-rpg` remain a public sample after migration, or should it be
  converted to a local/private mount later?

## Log

- 2026-07-09: Created as final child task from `T0341` review. Migration is
  intentionally behind the privacy, Taskboard, Canvas, and generator gates.
