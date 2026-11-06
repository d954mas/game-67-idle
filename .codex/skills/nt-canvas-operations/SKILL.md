---
name: nt-canvas-operations
description: "Read or edit Canvas projects, screens, elements, exports, history, and canvas:// references."
---

# NT Canvas Operations

Canvas is owned by `ai_studio/assets/canvas/`. Use its README for orientation
when needed and load the contract relevant to the request:

- CLI, mutations, slicing and acceptance: `contracts/operations.md`
- alpha and cleanup: `contracts/alpha-and-cleanup.md`
- recipe/style/animation cards and packs: `contracts/recipe-pack.md`
- generation and export: `contracts/generation-and-export.md`
- history: `contracts/history.md`; browser UI: `contracts/browser-ui.md`
- storage and ownership: `contracts/storage.md`; Chat: `contracts/chat.md`
- tests for changed Canvas tools/contracts: `contracts/validation.md`

Discover unfamiliar commands with `node ai_studio/assets/canvas/cli.mjs`.
Mutations use the CLI/ops facade and journal, never direct project-file edits.
For generation, check adapter permissions against
`.codex/skills/nt-asset-image-generation/references/generation-paths.md`;
Canvas's legacy generator defaults need the same checks as direct generation.
Resolve public `canvas://<projectId>[/group/<id>|/element/<id>[/region/<id>]]`
and private `canvas://game/<gameId>/<projectId>/...` references; private calls
include `--store game:<gameId>`. Ignore the readable tail after ` — `.

Before history navigation, read `history-list` immediately and pass the current
head as `--expect-head <n>`. Source files stay immutable; preserve undo and
page/CLI parity. Verify the requested resulting state or export; inspect pixels
for visible changes. Follow the existing acceptance/promotion gates for game
assets, and report the resulting Canvas reference and any unresolved gap.
