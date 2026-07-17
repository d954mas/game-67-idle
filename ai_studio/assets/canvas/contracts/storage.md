# Canvas storage contract

Portable project folders contain `project.json` and immutable,
content-addressed `files/`. Per-gesture `journal.jsonl`, snapshots, compaction
archive, and the cross-process lock live in the machine-local Canvas cache,
keyed by projects root and project id; undo history is deliberately not synced.
The store path-confines ids and atomically replaces metadata. Do not hand edit
project files; use `ops.mjs` or the CLI.

`ai_studio/studio.config.json` contains portable defaults. Machine-specific
roots belong in ignored `ai_studio/studio.config.local.json`, whose fields
override the tracked file. `CANVAS_PROJECTS_ROOT` remains the explicit test and
one-off override. The default repo-local projects directory is ignored.

Shared Canvas storage remains external in normal workstation configuration.
Project ownership is metadata (`ownership.kind=game`, `gameId`), not a game-side
copy of project data.

Private game stores are explicit mounts. They are excluded from aggregate reads
unless the caller opts in, and private exports may not target the public parent
repository. Public object references use `canvas://<project>/<kind>/<id>`;
private references include `canvas://game/<gameId>/...` and never fall back to a
bare-id search across private stores.

`element.assetStatus`, when present on an image, is mutable workflow state rather
than provenance. Its only valid persisted values are `quarantine`, `checked`, and
`accepted`. Newly minted generated and pixel-derived images start in `quarantine`;
absence means a legacy or user-imported image has not entered the review workflow.
Writers must use the Canvas operation/CLI instead of editing `project.json`.
The latest deterministic evidence lives in `element.meta.technical_gate`; it is
a frozen report snapshot, not caller-authored state, and may reference a
content-addressed failed-region thumbnail under the project's `files/` folder.
The latest advisory vision report lives in `element.meta.style_verdict` and
binds the target source, exact style-lock snapshot, and logical plus physical
exemplar refs. The explicit lead result lives in `element.meta.style_decision`:
it freezes `accept|revise|reject`, the bounded caller-authored reason, timestamp,
source/lock ids, and the advisory verdict it considered. Only a current explicit
`accept` decision can mint `assetStatus: "accepted"`; direct status writers cannot
promote art.
External clipboard specs cannot mint it either: public image paste always stores
`quarantine` and strips all three review-evidence fields. Live-node duplication
is the distinct trusted path that may preserve existing persisted review state.
Any cleanup or filter bake that materializes pixels into a source file likewise
starts a fresh `quarantine` lifecycle and removes review evidence for the old
source; undo restores the exact prior accepted state with its original bytes.
Promotion does not mutate the Canvas project. It copies accepted immutable bytes
to the owning game's `assets/packs/canvas-promotions` pack and records resource,
SHA-256, byte count, Canvas source ref, style lock, and lead-decision provenance
in `assets.jsonl`. Duplicate asset ids or destination files fail loudly.
Promotion stages bytes and manifest under a per-game cross-process lock, verifies
the staged hash against the content-addressed Canvas source name, rolls back a
failed commit, recovers a process crash through prepared/committed markers, and
rejects any symlink/junction in the physical Canvas source or game-pack path.
