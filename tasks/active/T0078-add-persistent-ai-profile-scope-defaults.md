---
id: T0078
title: Add persistent AI profile scope defaults
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, context, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a persistent AI profile scope file under `tmp/session_profiles/` so
work-item and iteration defaults survive separate tool command invocations.

## Done when

- [x] `tools/ai_profile/scope.mjs` can set, show, and clear current profile
      scope defaults.
- [x] Profile records use precedence: explicit CLI metadata, then env
      defaults, then persistent scope file.
- [x] `status.mjs` reports active scope defaults and recommends `scope.mjs`
      when work-item coverage is low.
- [x] Profiling docs, reflection skill rules, and skill eval mention
      `scope.mjs` and the scope-file fallback.
- [x] Validation passes for syntax, live scope/default/override behavior,
      status JSON, taskboard, skill eval, diff check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started persistent profile scope so work-item/iteration defaults survive separate tool commands without repeating CLI flags.
- 2026-06-13: Added persistent scope fallback in `profile_lib.mjs`: profile metadata precedence is explicit CLI flags, then `AI_PROFILE_*` environment variables, then `tmp/session_profiles/current_scope.json`.
- 2026-06-13: Added `tools/ai_profile/scope.mjs` with `show`, `set`, and `clear`; scope path can be overridden with `--scope` or `AI_PROFILE_SCOPE_FILE`.
- 2026-06-13: Scope evidence: `node tools/ai_profile/scope.mjs set --scope tmp/session_profiles/scope_precedence_test_scope.json --work-item T0078_SCOPE --iteration scope-default` created a valid scope; `show` reported it; `clear` removed it.
- 2026-06-13: Precedence evidence: isolated events proved scope-only metadata wrote `T0078_SCOPE/scope-default`, env over scope wrote `T0078_ENV/env-over-scope`, and CLI over env/scope wrote `T0078_CLI/cli-over-env`.
- 2026-06-13: Status evidence: with `AI_PROFILE_SCOPE_FILE=tmp/session_profiles/scope_precedence_test_scope.json`, `status.mjs --profile tmp/session_profiles/scope_precedence_scope_only.jsonl --json-output tmp/session_profiles/scope_precedence_status.json` reported `Scope: set (T0078_SCOPE/scope-default)`.
- 2026-06-13: Final validation passed: `node --check tools/ai_profile/profile_lib.mjs`; `node --check tools/ai_profile/scope.mjs`; `node --check tools/ai_profile/status.mjs`; `node --check tools/ai_profile/followups.mjs`; `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`; `git diff --check`; `node tools/pipeline_validate.mjs`.
- 2026-06-13: Started persistent profile scope so work-item/iteration defaults survive separate tool commands without repeating CLI flags.
- 2026-06-13: Moved to review after persistent scope implementation, precedence proof, status proof, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
