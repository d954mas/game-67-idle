# Core Harness Validation

This module owns fast mechanical checks for Core Harness entrypoints and docs.

It is not a global pipeline validator. Each AI Studio module owns its own
validation command and tests.

## Scope

Checked by this module:

- `AGENTS.md`
- `CLAUDE.md`
- `GAME_PROJECT.md`
- `ai_studio/README.md`
- `ai_studio/core_harness/`

Not checked here:

- Taskboard store, API, or UI behavior.
- Architecture Map ownership coverage.
- Asset Viewer/gallery behavior.
- Legacy docs under `docs/ai-pipeline/`.
- Skills outside the generated Core Harness surfaces.

## Public Tool

- `doc_reference_check.mjs`: checks agent-facing Markdown for stale local file
  references and retired Core-era command routes.

## When To Use

Run this after moving docs, renaming files, deleting old commands, or changing
agent-facing entrypoints.

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node --test ai_studio/core_harness/validation/tests/doc_reference_check.test.mjs
```
