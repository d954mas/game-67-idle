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

Image elements may carry the explicit top-level workflow field `assetStatus`:
`quarantine`, `checked`, or `accepted`. Legacy/unstamped images have no field and
read as `null`. Internal generation and image-pipeline operations mint results in
`quarantine`; ordinary `addImage` / multi-file imports do not accept a caller-set
status and remain untracked. Status changes use the dedicated `setAssetStatus`
operation (not a generic metadata patch), are journaled and undoable, and may move
backward when review is revoked. The public setter may initialize `quarantine`,
repeat a no-op, or downgrade an existing state; promotion to `checked` / `accepted`
is reserved for later technical/style verdict operations carrying their evidence.
CLI parity is `asset-status-show` / `asset-status-set`; HTTP parity is `GET` / `PUT
.../elements/<id>/asset-status`.
