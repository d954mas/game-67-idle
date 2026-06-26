# Core Harness Validation

This module owns fast mechanical checks for Core Harness docs and routes.

It is not a global pipeline validator. Each AI Studio module owns its own
validation command and tests.

## Public Tool

- `doc_reference_check.mjs`: checks agent-facing Markdown for stale local file
  references and retired command routes.

## When To Use

Run this after moving docs, renaming files, deleting old commands, or changing
agent-facing entrypoints.

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node --test ai_studio/core_harness/validation/tests/doc_reference_check.test.mjs
```
