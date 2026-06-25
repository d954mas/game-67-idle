# Core Harness Validation

This module owns lightweight harness guards that protect agent-facing docs and
routes.

Keep this layer narrow:

- validate references, routing, and generated compatibility surfaces;
- report actionable stale paths or retired commands;
- avoid product/game acceptance gates here.

## Public Tools

- `doc_reference_check.mjs`: checks local Markdown/tool references in
  agent-facing docs and fails retired command routes.
- `pipeline_validate.mjs`: orchestrates quick/review/full validation over the
  reusable pipeline, export route, taskboard, skills, and product gates.

## Validation

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node --test ai_studio/core_harness/validation/tests/doc_reference_check.test.mjs
node ai_studio/core_harness/validation/pipeline_validate.mjs --dry-run
node --test ai_studio/core_harness/validation/tests/pipeline_validate.test.mjs
```
