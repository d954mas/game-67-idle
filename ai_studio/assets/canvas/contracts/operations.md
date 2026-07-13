# Canvas operations contract

`../ops.mjs` is the stable public facade used by the CLI, HTTP adapter, tests,
and direct agents. Domain entrypoints under `../ops/` keep maintenance reads
scoped; changing their physical layout must not change facade exports.

Every persisted capability is one operation shared by page and CLI. Mutations
validate before writing, run under the project lock, update `project.json`, and
append one journal step per user gesture. Source files are immutable and
content-addressed. Transformations create new files so undo restores pixels and
metadata exactly.

Agents use `node ai_studio/assets/canvas/cli.mjs`. Run it without arguments for
the live command list. History navigation requires a fresh `history-list` read
and `--expect-head <n>` on undo, redo, and jump.
