# Core Harness Profiling

Profiling records and reviews agent harness behavior. It is part of Core
Harness because it observes the agent loop itself: session starts, shell command
starts/results, failures, repeated commands, slow commands, coverage gaps, and
subagent transcript diagnostics.

Lightweight hook profiles require matching start events or an explicit
`duration_ms`; some hosts record results only. A complete Codex review reads the
canonical rollout transcript directly, including tool timing and token totals.

Profiling is passive. Hooks write JSONL records under `tmp/session_profiles/`.
Those files are disposable live diagnostics, not the source of truth. Codex
rollout transcripts remain in the user-local Codex session store, so cleaning
`tmp/` cannot erase the evidence. Generated reports are local and uncommitted.

Reports distinguish observed hook/session records from advisory diagnosis.
They do not prove process conventions or select models.

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

Review a week of work across every session:

```powershell
node ai_studio/core_harness/profiling/iteration_report.mjs --since 7d
node ai_studio/core_harness/profiling/iteration_report.mjs --since 14d --json
```

`iteration_report.mjs` answers what one session cannot: whether iteration is
getting cheaper, and which category is paying. It reads Claude transcripts and
Codex rollouts whose working directory is this repository, and reports per
harness the call count, measured command time, output volume, failure share,
median turn length and tools per turn; then the same by category, the noisiest
commands by output, and test-versus-source lines written in the window.

Output volume sits next to time on purpose: every tool result is re-read on each
later request, so a command that dumps a tree is charged again on every turn
that follows it. Truncated Codex output is reported as produced-and-cut tokens
rather than dropped, because that generation was paid for.

Parallel `functions.exec` batches keep one shared timing and one shared output.
The report classifies such a batch only when every nested command agrees, and
labels it `mixed batch` otherwise; nested commands appear in a counts-only list
because their shared bytes cannot be attributed honestly.

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
The report names the noisiest single command as well as aggregate tool output,
so broad reads can be replaced with scoped queries.
Parallel `functions.exec` batches stay labeled as composite calls because their
shared timing and output cannot be attributed honestly to one nested command.

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

- `hook_record_fast.c` / `hook_record_fast.exe`: the single hook-recorder
  implementation. Windows hooks run the committed binary; other hosts no-op
  unless the binary is built explicitly with `build_hook_record_fast.mjs`.
- `codex_transcript.mjs`: canonical Codex rollout resolver and normalizer.
- `status.mjs`: lightweight or complete session report renderer.
- `agent_rollup.mjs`: optional subagent transcript rollup.
- `profile_lib.mjs`: shared JSONL/profile helpers.
- `tests/profiling.test.mjs`: focused profiling tests.

Session-retrospective instructions live in
`.codex/skills/nt-chat-session-reflection/SKILL.md`; Profiling owns the
telemetry commands those instructions use.
