# Reflection Profiling

Load this reference for AI workflow review, profiler review, analytics review,
or any reflection where time-spend claims need profiling evidence.

Profiling is fully passive: the PostToolUse hook records every tool call
(command, duration, result) to a per-session log under `tmp/session_profiles/`
automatically. There is no manual start/checkpoint/run step. Read the captured
session with:

```powershell
node tools/ai.mjs status
node tools/ai.mjs status --verbose
```

`status` reports the record count, unresolved/recovered failures, active
wall-clock coverage, the slowest recorded work, top time-sinks, and most-run
commands (repeats/retries = friction). `--verbose` adds the largest coverage
gaps and any parse errors. Use it to name the session's slow commands, repeated
or failed commands, and uncaptured gaps.

For long Codex sessions with suspected missing failures, recover them first:

```powershell
node tools/ai.mjs import-codex-session
```

`status` runs this automatically unless `--no-import-codex-session` is passed.

Use low-level `tools/ai_profile/*` only when debugging profiling itself.
