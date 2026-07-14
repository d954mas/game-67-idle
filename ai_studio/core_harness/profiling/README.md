# Core Harness Profiling

Profiling records and reviews agent harness behavior. It is part of Core
Harness because it observes the agent loop itself: session starts, shell command
starts/results, failures, repeated commands, slow commands, coverage gaps, and
subagent spawn diagnostics.

Command durations require matching start events or an explicit `duration_ms`;
some hosts record results only. Token usage is not part of this profile format.

Profiling is passive. Hooks write JSONL records under `tmp/session_profiles/`.
The raw telemetry is local evidence and should not be committed.

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

Recover missed failed Codex shell calls from a Codex session JSONL:

```powershell
node ai_studio/core_harness/profiling/hook_record.mjs codex --recover-only
```

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
- `hook_record.mjs`: JS fallback, Codex failure recovery, and subagent spawn
  telemetry.
- `status.mjs`: session report renderer.
- `agent_rollup.mjs`: optional subagent transcript rollup.
- `profile_lib.mjs`: shared JSONL/profile helpers.
- `tests/profiling.test.mjs`: focused profiling tests.

Session-retrospective instructions live in
`.codex/skills/nt-chat-session-reflection/SKILL.md`; Profiling owns the
telemetry commands those instructions use.
