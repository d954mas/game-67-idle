# Canvas

Canvas owns multi-image projects, journaled operations, the browser workspace,
and Canvas Chat. Studio Shell only hosts the HTTP routes.

`config.mjs` owns Canvas project, history, and cache root interpretation while
the neutral Studio loader owns only reading the committed and local JSON files.

Load only the contract needed for the current request:

- [Operations](contracts/operations.md) — `ops.mjs`, CLI/API parity, mutation and export rules.
- [Alpha and cleanup](contracts/alpha-and-cleanup.md) — keying, dual plate, cleanup, and filter bake.
- [Recipe and pack cards](contracts/recipe-pack.md) — recipe/style cards and pack slicing.
- [Animation](contracts/animation.md) — animation cards, flipbook provenance, playback, and frozen browser generation controls.
- [Generation and export](contracts/generation-and-export.md) — generator provenance and delivery rules.
- [History](contracts/history.md) — journal structure, undo/redo, and live-head guards.
- [Browser UI](contracts/browser-ui.md) — stable inspector/workspace facades and interaction rules.
- [Storage](contracts/storage.md) — project layout, configuration, locking, and history.
- [Canvas Chat](contracts/chat.md) — selection context, permissions, transport, and hosting seam.
- [Validation](contracts/validation.md) — focused tests and architecture checks.
