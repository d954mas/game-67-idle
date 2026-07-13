# Core Harness Validation

This module owns fast mechanical checks for Core Harness entrypoints and docs.

It is not a global pipeline validator. Each AI Studio module owns its own
validation command and tests.

## Scope

Checked by this module:

- `AGENTS.md`
- `CLAUDE.md`
- `ai_studio/README.md`
- `ai_studio/core_harness/`

Not checked here:

- Taskboard store, API, or UI behavior.
- Architecture Map ownership coverage.
- Asset Viewer/gallery behavior.
- Module-specific docs outside Core Harness.
- Skills outside the generated Core Harness surfaces.

## Public Tools

- `doc_reference_check.mjs`: checks agent-facing Markdown for stale local file
  references.
- `enforcement_check.mjs`: validates enforcement classifications and proof
  links from `../workflow/enforcement_contract.json`.
- `agent_role_smoke.mjs`: checks a supplied native Codex subagent transcript;
  procedure details live in `../workflow/orchestration/README.md`.

## When To Use

Run this after moving docs, renaming files, or changing agent-facing
entrypoints.

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/core_harness/validation/enforcement_check.mjs
node ai_studio/core_harness/validation/agent_role_smoke.mjs --evidence <rollout.jsonl> --requested-role fast-worker
node --test ai_studio/core_harness/validation/tests/doc_reference_check.test.mjs
node --test ai_studio/core_harness/validation/tests/enforcement_check.test.mjs
node --test ai_studio/core_harness/validation/tests/agent_role_smoke.test.mjs
```
