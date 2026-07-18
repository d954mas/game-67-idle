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
`accept|revise|reject` report with source/exemplar refs and the complete lock
snapshot in an undoable commit, but deliberately preserves `checked`/`accepted`
status so the model
cannot become a hidden hard gate. `decideAssetStyle` is the explicit lead
backstop: a required caller-authored `decision` plus bounded non-empty `reason`
records `meta.style_decision`; `accept` moves current checked art to `accepted`,
while `revise`/`reject` return it to `quarantine`. A lead may override any model
verdict, but only while the source, technical PASS, complete style lock, and all
exemplar sources still match the advisory evidence. The decision and status
change share one undoable commit.

CLI parity is `asset-status-show`, `asset-status-set`, `asset-status-check`,
`asset-style-check`, `asset-style-decide`, and `asset-promote`; HTTP parity adds
`POST .../asset-style-check`, `POST .../asset-style-decision`, and
`POST .../asset-promote`. Request bodies cannot supply trusted technical/style
reports. Decision and reason are intentionally caller-authored because that
route represents the explicit lead action, not model evidence.

`promoteAssetToGame` is the only Canvas-to-game asset write boundary. It refuses
anything except a current `accepted` image with a matching technical PASS,
style-lock snapshot, exemplar sources, advisory report, and explicit lead accept
decision. It also requires complete publishable license/provenance metadata and
refuses overwrite. A per-game local/cross-process lock covers manifest read,
duplicate checks, staging, and commit; physical-directory checks reject symlink
or junction escapes. Source bytes must still match their Canvas content-addressed
filename after staging. Success copies immutable bytes into the owning public/private
game's `assets/packs/canvas-promotions/files/<asset-id>/` and appends a Pack
Manifest row with SHA-256, byte count, Canvas source ref, lock id, and frozen lead
decision. Private provenance uses the canonical scoped
`canvas://game/<game-id>/<project-id>/element/<element-id>` ref. Prepared/committed
transaction markers recover an interrupted rename sequence before the next
promotion; recovery revalidates physical ancestors before cleanup, and a
committed marker remains authoritative until finalization completes. Only the
Canvas image extension allowlist (`png|jpg|jpeg|gif|webp`) can be promoted. The
CLI reads metadata from `--metadata <json>`; the API body uses
`{metadata}`.

Hand-authored `nodes-paste` specs are never an acceptance path. Every pasted
image re-enters `quarantine`, and any supplied `technical_gate`, `style_verdict`,
or `style_decision` metadata is discarded while unrelated metadata is retained.
The separate live-project `nodes-duplicate` path may preserve an already-valid
review state because its spec is built internally from persisted nodes rather
than caller-authored JSON.

Pixel-materializing mutations also restart review. `cleanupApply` and both
single/batch `bakeFilters` paths set the changed image to `quarantine` and remove
technical/style/decision evidence bound to the previous source, while retaining
unrelated provenance such as alpha, cleanup, origin, and filters-bake records.

The vision subprocess is ephemeral, ignores project/user execution rules, and
pins the read-only Codex sandbox to an isolated temporary root containing only
copies of the target and selected exemplar images. Before committing after the
slow call, the op re-resolves the external style-lock file and rechecks every
exemplar's current `source_ref`; evidence freezes those source refs beside the
logical Canvas refs.
Changed, missing, or malformed lock/exemplar inputs and a moved Canvas head
return a conflict instead of storing stale judgment. Model reports are exact-key
and size-bounded before any project write.
