---
name: nt-canvas-operations
description: "Use for canvas:// refs and Canvas project, screen, group, element, region, export, history, or Chat operations."
---

# NT Canvas Operations

Canvas is owned by `ai_studio/assets/canvas/`. Start at its short
`README.md`, then load only the contract matching the request:

- mutations, CLI, exports, slicing: `contracts/operations.md`
- alpha, dual plate, cleanup, filter bake: `contracts/alpha-and-cleanup.md`
- recipe/style/animation cards and packs: `contracts/recipe-pack.md`
- generation, render, export delivery: `contracts/generation-and-export.md`
- undo, redo, history jump: `contracts/history.md`
- page inspector/workspace behavior: `contracts/browser-ui.md`
- roots, project files, ownership, history: `contracts/storage.md`
- Canvas Chat and permissions: `contracts/chat.md`
- test commands: `contracts/validation.md`

For an operation, run `node ai_studio/assets/canvas/cli.mjs` without arguments
to discover the current command surface. Never hand-edit `project.json` or
files; all mutations go through the CLI/ops facade and journal path.

Resolve public refs as `canvas://<projectId>[/group/<id>|/element/<id>[/region/<id>]]`.
Resolve store-qualified private refs as `canvas://game/<gameId>/<projectId>/...`
and pass `--store game:<gameId>` on every CLI call. Ignore the human-readable
tail after ` — `.

Before undo, redo, or history jump, run `history-list` immediately and pass its
current head as `--expect-head <n>`. Source files are immutable; operations
must remain non-destructive and page/CLI behavior must stay in parity.
