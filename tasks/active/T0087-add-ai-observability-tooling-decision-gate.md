---
id: T0087
title: Add AI observability tooling decision gate
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, observability, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a reusable decision gate for AI observability/eval tooling so future
projects know when to keep using the local JSONL profiler, when to run a
bounded external pilot, and when external tooling is a credible adoption
candidate.

## Done when

- [x] Durable pipeline docs describe local-first profiling versus external
      observability/eval pilots.
- [x] A low-overhead CLI prints a recommendation and machine-readable JSON for
      local-only, external-pilot, or adoption-candidate decisions.
- [x] Portable export includes the decision-gate doc/tool.
- [x] Reflection skill rules mention the gate before recommending external
      AI observability systems.
- [x] Validation covers the new CLI and portable pipeline.

## Open questions

## Log

- 2026-06-13: Promoted profile follow-up into a task after refreshed closeout
  showed the next universal improvement should prevent external observability
  setup from becoming unbounded process overhead.
- 2026-06-13: Added `AI_PIPELINE_OBSERVABILITY_TOOLS.md` with a local-first
  decision rule based on OpenTelemetry GenAI conventions, LangSmith, Phoenix,
  Langfuse, and Braintrust documentation checked on 2026-06-13.
- 2026-06-13: Added `tools/ai_profile/observability_gate.mjs`; smoke command
  `node tools/ai_profile/observability_gate.mjs --need human-review --need
  datasets --team small --setup-cost medium --sensitivity medium --self-host-ok
  --json-output tmp/session_profiles/observability_gate_smoke.json` returned
  `external_pilot` while keeping local JSONL as the baseline.
- 2026-06-13: Updated `AI_PIPELINE.md`,
  `AI_PIPELINE_SESSION_PROFILING.md`, `chat-session-reflection`,
  `tools/bootstrap/export_base.mjs`, and `tools/skills_eval.mjs` so the gate
  travels with the portable base and reflection agents use it before
  recommending external observability tooling.
- 2026-06-13: Validation passed: `node --check
  tools/ai_profile/observability_gate.mjs`; `node --check
  tools/bootstrap/export_base.mjs`; `node --check tools/skills_eval.mjs`;
  `node --test tools/ai_profile/test.mjs` passed 19 tests; `node
  tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `git diff --check`; `node
  tools/pipeline_validate.mjs`.
- 2026-06-13: Completed observability tooling decision gate; validation: node --test tools/ai_profile/test.mjs, skills_sync/eval, taskboard validate, git diff --check, node tools/pipeline_validate.mjs.
