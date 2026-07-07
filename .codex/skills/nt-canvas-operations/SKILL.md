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

Background cleanup on already-landed art (a flat-light-bg element that needs a
transparent background) does NOT need a separate white/black plate pair built
by hand: `alpha-dual-generate <projectId> --element <eid> [--prompt "..."]`
generates the missing dark plate, gates the pair, and mints one new cut
element beside the source in a single call — see the README's **Automatic
dual-plate generation** section for the full contract (retry, refusal
messages, `meta.alpha` shape).

Tiered/axis packs of icons on the canvas (a grade ladder, a family of variants)
do NOT need a separate pack card or the disk conveyor: `recipe.pack` is an
optional mode on the SAME recipe card (`recipe-pack-preview`/`-generate`/
`-slice`) — see the README's **Recipe card** section, **Pack mode** subsection.

## History navigation (undo/redo/jump) — live-project guard

The project may be live (a human editing it in the page) while an agent is also
acting on it, so `undo`, `redo`, and `history-jump` REQUIRE `--expect-head <n>`
proving the agent read the CURRENT head first — a stale value refuses loudly and
writes nothing (T0234, after an incident where an agent's jump forked history and
orphaned the lead's newest edits). Workflow:

```
node ai_studio/assets/canvas/cli.mjs history-list <projectId>   # prints "head: N", then JSON
node ai_studio/assets/canvas/cli.mjs undo <projectId> --expect-head N
node ai_studio/assets/canvas/cli.mjs redo <projectId> --expect-head N
node ai_studio/assets/canvas/cli.mjs history-jump <projectId> --seq <n> --expect-head N
```

Always re-run `history-list` immediately before navigating if any time has passed —
`N` must be the head at the moment of the call, not a value read earlier in the
session.

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
