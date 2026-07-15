# Core Harness Profiling

Profiling records and reviews agent harness behavior. It is part of Core
Harness because it observes the agent loop itself: session starts, shell command
starts/results, failures, repeated commands, slow commands, coverage gaps, and
subagent spawn diagnostics.

Lightweight hook profiles require matching start events or an explicit
`duration_ms`; some hosts record results only. A complete Codex review reads the
canonical rollout transcript directly, including tool timing and token totals.

Profiling is passive. Hooks write JSONL records under `tmp/session_profiles/`.
Those files are disposable live diagnostics, not the source of truth. Codex
rollout transcripts remain in the user-local Codex session store, so cleaning
`tmp/` cannot erase the evidence. Generated reports are local and uncommitted.

Reports distinguish observed hook/session records from advisory diagnosis.
They do not prove process conventions, select models, or replace the explicit
post-restart role/model smoke in `../validation/agent_role_smoke.mjs`.

Stay local-first for AI observability. Do not add external tracing, eval, or
dashboard services unless a concrete repeated need exists, such as shared human
review, comparable datasets/evals, production telemetry, OTLP integration, or a
local JSONL workflow that cannot answer an important repeated question.

## Boundary

- `agent_surfaces/` owns generated hook config for Codex and Claude.
- `profiling/` owns the recorder commands that those hooks run.
- `workflow/` may route retrospectives to profiling output.
- Taskboard store/domain code does not know about session profiling.

## Commands

Review the active session:

```powershell
node ai_studio/core_harness/profiling/status.mjs
node ai_studio/core_harness/profiling/status.mjs --verbose
```

Review the complete current Codex session across date boundaries:

```powershell
node ai_studio/core_harness/profiling/status.mjs --complete --verbose
```

`--complete` resolves the rollout by `CODEX_THREAD_ID`; use
`--transcript <path>` only for an older or explicitly selected session.
It also reports a workflow advisory from session age, top-level tool-call count,
and the latest Codex context-window telemetry: checkpoint after four hours or
300 calls, and prefer a new session after six hours or 70% context use.
Command rollups keep `studio.mjs verify --changed`, `--domain`, and `--full`
separate so repeated release proof is visible without adding another runner.
Escalated approval-review time is reported as coordination time rather than as
execution time for the command that was waiting for approval.

Show subagent transcript diagnostics:

```powershell
node ai_studio/core_harness/profiling/status.mjs --agents
```

Verify the single root Studio Python environment before Python-backed gates:

```powershell
node ai_studio/dev_environment/python_check.mjs
```

Create or repair that environment through `../../dev_environment/python_setup.mjs`;
its README owns the required Python 3.12 bootstrap command.

## Files

- `hook_record_fast.c` / `hook_record_fast.exe`: fast hook recorder used by the
  hot path.
- `hook_record.mjs`: JS fallback and subagent spawn telemetry.
- `codex_transcript.mjs`: canonical Codex rollout resolver and normalizer.
- `status.mjs`: lightweight or complete session report renderer.
- `agent_rollup.mjs`: optional subagent transcript rollup.
- `profile_lib.mjs`: shared JSONL/profile helpers.
- `tests/profiling.test.mjs`: focused profiling tests.

Session-retrospective instructions live in
`.codex/skills/nt-chat-session-reflection/SKILL.md`; Profiling owns the
telemetry commands those instructions use.
