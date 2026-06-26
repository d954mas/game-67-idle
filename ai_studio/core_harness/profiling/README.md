# Core Harness Profiling

Profiling records and reviews agent harness behavior. It is part of Core
Harness because it observes the agent loop itself: session starts, shell command
starts/results, failures, repeated commands, slow commands, coverage gaps, and
subagent spawn diagnostics.

Profiling is passive. Hooks write JSONL records under `tmp/session_profiles/`.
The raw telemetry is local evidence and should not be committed.

## Boundary

- `agent_surfaces/` owns generated hook config for Codex and Claude.
- `profiling/` owns the recorder commands that those hooks run.
- `workflow/` may route retrospectives to profiling output.
- Taskboard does not know about profiling.

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

## Files

- `hook_record_fast.c` / `hook_record_fast.exe`: fast hook recorder used by the
  hot path.
- `hook_record.mjs`: JS fallback, Codex failure recovery, and subagent spawn
  telemetry.
- `status.mjs`: session report renderer.
- `agent_rollup.mjs`: optional subagent transcript rollup.
- `profile_lib.mjs`: shared JSONL/profile helpers.
- `tests/profiling.test.mjs`: focused profiling tests.
- `skills/nt-chat-session-reflection/`: canonical reflection skill instructions.
