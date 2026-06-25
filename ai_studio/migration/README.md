# Migration

Owns the gradual move into `ai_studio/`.

## Policy

Do not bulk-move the current repo. A file moves here only after it is inspected,
refactored, and assigned to a domain.

The map has one explicit backlog node: `Not Refactored`. Anything not yet
reviewed stays there or at its old path. Promotion into the main `ai_studio/`
tree happens one module at a time.

## States

- `current`: existing file remains in the old location.
- `mapped`: domain owner is known, but the file has not moved.
- `migrating`: new source exists under `ai_studio/`, old path remains as a shim.
- `migrated`: callers use the new location or an intentional facade.
- `delete`: file has no owner or real use and should be removed after proof.

## First Refactor Line

Start from Core:

1. Keep root `AGENTS.md`, `AI_PIPELINE.md`, and `CLAUDE.md` as harness facades.
2. Move durable Core explanation into `ai_studio/core/`.
3. Move domain procedure out of Core into `assets/`, `tech/`, `design/`,
   `tasks/`, `validation/`, or `export/`.
4. Rebuild `docs/ai-pipeline/architecture-map.html` after each domain move.
