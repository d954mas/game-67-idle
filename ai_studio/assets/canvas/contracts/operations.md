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
repeat a no-op, or downgrade an existing state. `runAssetTechnicalGate` is the
trusted upward path to `checked`: it resolves the game-owned accepted style lock,
runs the deterministic evaluator outside the project lock, then rechecks the
Canvas head and commits the verdict, metrics, thresholds, lock id, source ref,
and optional problem thumbnail in one undoable step. PASS promotes untracked or
quarantined art to `checked` (and preserves `accepted`); FAIL moves any state to
`quarantine`. Public callers cannot submit verdict evidence. `accepted` remains
reserved for an explicit lead decision. `runAssetStyleVerdict` is the trusted
advisory step before that decision: it requires current passing technical-gate
evidence for the same source and lock, resolves the lock's owned exemplar
images, and vision-compares them with the target against the lock's
`prompt_preamble` (Do) and `negative_prompt` (Don't). It stores one strict
`accept|revise|reject` report with source/lock/exemplar refs in an undoable
commit, but deliberately preserves `checked`/`accepted` status so the model
cannot become a hidden hard gate. CLI parity is `asset-status-show`,
`asset-status-set`, `asset-status-check`, and `asset-style-check`; HTTP parity
adds `POST .../asset-style-check`. Request bodies cannot supply either trusted
technical or style verdicts.

The vision subprocess is ephemeral, ignores project/user execution rules, and
pins the read-only Codex sandbox to an isolated temporary root containing only
copies of the target and selected exemplar images. Before committing after the slow call, the op
re-resolves the external style-lock file and rechecks every exemplar's current
`source_ref`; evidence freezes those source refs beside the logical Canvas refs.
Changed, missing, or malformed lock/exemplar inputs and a moved Canvas head
return a conflict instead of storing stale judgment. Model reports are exact-key
and size-bounded before any project write.
