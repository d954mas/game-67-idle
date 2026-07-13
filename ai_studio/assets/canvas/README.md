# Canvas

Canvas owns multi-image projects, journaled operations, the browser workspace,
and Canvas Chat. Studio Shell only hosts the HTTP routes.

Load only the contract needed for the current request:

- [Operations](contracts/operations.md) — `ops.mjs`, CLI/API parity, mutation and export rules.
- [Alpha and cleanup](contracts/alpha-and-cleanup.md) — keying, dual plate, cleanup, and filter bake.
- [Recipe and pack cards](contracts/recipe-pack.md) — recipe/style/animation cards and pack slicing.
- [Generation and export](contracts/generation-and-export.md) — generator provenance and delivery rules.
- [History](contracts/history.md) — journal structure, undo/redo, and live-head guards.
- [Browser UI](contracts/browser-ui.md) — stable inspector/workspace facades and interaction rules.
- [Storage](contracts/storage.md) — project layout, configuration, locking, and history.
- [Canvas Chat](contracts/chat.md) — selection context, permissions, transport, and hosting seam.
- [Validation](contracts/validation.md) — focused tests and architecture checks.

Deep feature details that are not needed for normal routing remain in the
[full reference](contracts/full-reference.md); load it only for those features.

The narrative history and non-negotiable laws remain in [PLAN.md](PLAN.md).
