# Canvas operations contract

`../ops.mjs` is the stable public facade used by the CLI, HTTP adapter, tests,
and direct agents. Domain entrypoints under `../ops/` keep maintenance reads
scoped; changing their physical layout must not change facade exports.

Every persisted capability is one operation shared by page, API, and CLI; the
browser owns rendering and input only. Mutations validate before writing, run
under the project lock, update `project.json`, and append one journal step per
user gesture. Source files are immutable and content-addressed. Pixel
transformations create new files so undo restores pixels and metadata exactly.
An entire batch validates before its first write, and a true no-op creates no
journal row.

Groups and elements remain a flat persisted graph. Parent links and sibling
order are validated cycle-safely; render/tree order is computed rather than
stored as a second hierarchy. Project deletion is the deliberate journal
exception: it moves the whole folder into `.trash` for recovery.

Agents use `node ai_studio/assets/canvas/cli.mjs`. Run it without arguments for
the live command list. History navigation requires a fresh `history-list` read
and `--expect-head <n>` on undo, redo, and jump.
