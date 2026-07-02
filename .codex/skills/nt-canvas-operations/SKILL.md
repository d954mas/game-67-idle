---
name: nt-canvas-operations
description: "Use when a request mentions a canvas:// ref, a canvas project, or canvas screens/groups/slicing/export/text - resolve the ref and act via the canvas CLI. Any agent (Claude or Codex harness) handed a canvas:// ref pasted from the page's Copy ID needs this to act without hunting for docs."
---

# NT Canvas Operations

Use this as a thin router to `ai_studio/assets/canvas/`. Do not duplicate the
canvas module's operation contract here.

## Sources

- Module contract (single source of truth): `ai_studio/assets/canvas/README.md`.
- Working plan + non-negotiable laws: `ai_studio/assets/canvas/PLAN.md`.

## Resolving a `canvas://` ref

The page's right-click **Copy ID** pastes one of these (verified against
`site/context_menu.js`):

```
canvas://<projectId>                            — the project
canvas://<projectId>/group/<groupId>            — a group (screen)
canvas://<projectId>/element/<elementId>        — an element
canvas://<projectId>/element/<eId>/region/<rId> — a region on an element
```

A human-readable tail follows after ` — `; it is display sugar only. To
resolve: take the bare ids out of the URI, then run
`node ai_studio/assets/canvas/cli.mjs show <projectId>` and find the
element/group/region by id in the returned JSON (`show` includes `groups`).
Drive the normal CLI from there with those ids.

## CLI surface

`node ai_studio/assets/canvas/cli.mjs` with no args (or an unknown command)
prints a self-documenting usage banner listing every command
(list/create/show/rename/delete/add-image/add-text/element-set/group-create/
export/undo/redo/history/...). Run it bare first to see the current command
set — do not hardcode a command list here. An HTTP API (`api.mjs`) exists for
page parity, but an agent should use the CLI.

## Laws (non-negotiable — see PLAN.md)

- **Non-destructive**: `files/` are immutable content-addressed originals;
  every transformation writes a NEW file, never overwrites the lead's art.
- **Every mutation is journaled**: undo must restore it exactly (pixels and
  metadata). If an op has no undo, it must not silently mutate the project.
- **Tool parity**: never bypass `ops.mjs` and never hand-edit `project.json`
  — go through the CLI (or `ops.mjs` directly), same as the page does.
- **Projects root**: resolved from `ai_studio/studio.config.json`
  (`canvasProjectsRoot`), which lives on YandexDisk — never assume a local
  path.

## When to use

Any request mentioning `canvas://`, a canvas project, or canvas
screens/groups/slicing/export/text work.
