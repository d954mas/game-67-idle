# AI Development Session Profiling

Purpose: make long AI-assisted development sessions measurable. A
retrospective explains what went wrong; a session profile records where time,
tools, context, and validation actually went.

Use this for long sessions, multi-agent work, release loops, art pipelines, or
any task where the lead later asks "where did the time/context go?"

## Profile Artifacts

Use three layers:

1. Live JSONL profile:
   `tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl`
2. Generated summary:
   `tmp/session_profiles/session_profile_YYYY-MM-DD.summary.md`
3. Durable lesson or retrospective:
   `AI_PIPELINE_ITERATION_LOG.md`,
   `AI_PIPELINE_RETROSPECTIVE_YYYY-MM-DD.md`, or a task log.

The JSONL and generated summary can stay in `tmp/` unless the lead asks to keep
them durable. Durable docs should contain conclusions, not every event.

## Versioning Policy

Commit reusable profiling infrastructure:

- `AI_PIPELINE_SESSION_PROFILING.md`
- `tools/ai_profile/*`
- skill/rule updates that make profiling part of the agent workflow
- task files that track profiling pipeline work
- compact lessons in `AI_PIPELINE_ITERATION_LOG.md`

Do not commit raw session telemetry by default:

- `tmp/session_profiles/*.jsonl`
- generated `*.summary.md`
- recovered thread dumps
- command transcripts
- one-off timing extracts
- large retrospectives that were created only for the current conversation

Promote a session-specific artifact to git only when the lead explicitly asks
to preserve it, or when it becomes a reusable playbook/example after cleanup.
If promoted, strip noisy logs and keep only decisions, metrics, and lessons.

This is not a replacement for `.gitignore`. `.gitignore` only protects known
scratch paths such as `tmp/`. The agent still needs this rule so it does not
create raw telemetry, recovered thread dumps, or one-off retrospectives in the
repository root where git can see them.

## When To Start Profiling

Start a profile when any of these are true:

- expected session is longer than 60 minutes;
- user asks to improve pipeline, inspect wasted time, or compare agents/tools;
- task requires repeated build/package/test loops;
- work touches art generation, reference research, release packaging, or
  multi-agent coordination;
- a context compaction occurs during active work;
- the agent has already repeated the same validation or file-reading loop twice.

If a profile was not started at the beginning, start it as soon as the need is
recognized and add `late_start: true` to the first record.

## Event Shape

Each line is one JSON object. Required fields:

```json
{
  "ts": "2026-06-13T10:00:00+05:00",
  "phase": "native_visual_polish",
  "category": "validation",
  "intent": "Run child-test readiness after HUD change",
  "result": "pass",
  "value": "productive"
}
```

Recommended fields:

```json
{
  "duration_ms": 120000,
  "tools": ["shell_command"],
  "commands": ["py -3.12 tools/devapi/scenarios/child_test_readiness.py ..."],
  "files_read": ["tasks/STATUS.md", "src/main.c"],
  "files_written": ["src/main.c"],
  "evidence": ["build/reports/child_test_readiness_v25_top_hud.json"],
  "context_inputs": [
    {"path": "tasks/STATUS.md", "chars": 42000, "reason": "current status"}
  ],
  "context_risk": "high",
  "waste_reason": "",
  "blocked_by": "",
  "notes": "DevAPI label contract stayed stable."
}
```

## Field Rules

`phase` is the current work lane, not the tool name. Examples:

- `reference_research`
- `art_generation`
- `asset_slicing`
- `native_gameplay`
- `native_visual_polish`
- `balance_tuning`
- `release_packaging`
- `release_audit`
- `task_status`
- `reflection`

`category` must be one of:

- `context`
- `planning`
- `research`
- `implementation`
- `art`
- `asset_pipeline`
- `validation`
- `release`
- `task_status`
- `reflection`
- `tooling`
- `handoff`

`result` must be one of:

- `pass`
- `fail`
- `mixed`
- `blocked`
- `skipped`
- `unknown`

`value` must be one of:

- `productive` - directly moved the selected gate forward;
- `necessary_overhead` - required coordination, logging, or validation;
- `rework` - fixed a mistake or stale evidence from this session;
- `waste` - should have been avoided or batched;
- `unknown` - not enough evidence yet.

`context_risk` must be one of:

- `low` - small, current files only;
- `medium` - several docs/logs or one long source file;
- `high` - broad history, long status logs, compaction risk, or stale evidence
  risk.

## What To Log

Log a record for:

- starting a major phase;
- reading large context files;
- running a build/test/scenario/package/audit;
- generating or integrating art;
- editing source-of-truth rules, skills, or tasks;
- discovering a wrong assumption;
- rerunning validation because evidence became stale;
- user correction that changes direction;
- context compaction/resume;
- final summary.

Do not log every tiny file read. Batch related reads into one record unless
the context cost itself is the point.

## Low-Overhead Capture

Use wrappers so profiling does not steal time from the actual work.

For shell commands, prefer:

```powershell
node tools/ai_profile/run.mjs --phase validation --category validation --intent "Run taskboard validation" -- node tools/taskboard/cli.mjs validate
```

`run.mjs` inherits stdio, waits for the command, then appends one JSONL record
with duration, exit code, command text, result, and tool type.

For non-command checkpoints, use:

```powershell
node tools/ai_profile/event.mjs --phase context --category context --intent "Read current status" --result pass --value necessary_overhead --file-read tasks/STATUS.md --context-input tasks/STATUS.md:42000:current-gate --context-risk medium
```

Use `event.mjs` only for meaningful checkpoints: phase start/end, user
correction, context compaction, major file-read batch, wrong assumption,
manual visual finding, or handoff decision.

For context-file reads, prefer the measured helper:

```powershell
node tools/ai_profile/context.mjs --phase context --intent "Load current rules" --path AGENTS.md --path tasks/README.md --reason "session start"
```

`context.mjs` measures character counts, records `files_read`, fills
`context_inputs`, and assigns context risk automatically unless
`--context-risk` is provided. Use it instead of manually typing
`--context-input` when the source is a local file.

Use project-relative paths when possible. `--context-input` also accepts
Windows absolute paths because the parser treats the first numeric segment as
the character-count separator.

Both commands default to:

```text
tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
```

Override with `--profile <path>` only for tests or when comparing profiles.

## Tool Profiling

Record tools by role:

- `shell_command`: command, duration, pass/fail, whether it was narrow or broad.
- `run.mjs`: default wrapper for shell commands. Prefer it for expensive,
  repeated, or validation commands so duration and exit code are captured
  automatically.
- `event.mjs`: low-cost checkpoint writer for non-command events.
- `context.mjs`: context-read checkpoint writer that measures local file
  character counts and fills `context_inputs` automatically.
- `closeout.mjs`: end-of-session helper that appends a final closeout event
  and writes a scratch summary artifact.
- `review.mjs`: reflection prep helper that turns a JSONL profile into
  priority findings: waste/rework, failures, blockers, context hotspots,
  repeated commands, repeated command scope (`preflight`, `scoped`,
  `broad/final`, `unknown`), missing context input details, and suggested
  pipeline actions.
- `plan_validation.mjs`: pre-validation helper that prints a narrow-to-broad
  validation ladder for the changed work kind. Use it when `review.mjs` reports
  repeated commands, before broad reusable-base checks, or when the right
  validation scope is unclear.
- `taskboard context`: current-context digest for resumes and long work. Prefer
  it before reading `tasks/STATUS.md` directly; if full status is still needed,
  log it as a medium/high `context_input`.
- `apply_patch`: files changed, why the change was scoped.
- `imagegen`: prompt packet path, candidate count, accepted/rejected result.
- `view_image`: image paths, visual findings.
- `web`: query/source count, source reliability, checked date.
- `read_thread`: turn count, duration sample, compactions found.
- `multi_agent`: packet owner, artifact returned, integration cost.

When a tool was used late, add `waste_reason`.

## Context Profiling

For long files or repeated context loads, add `context_inputs` with rough
character counts. Exact tokens are not required; the goal is to identify
expensive context sources.

Prefer `node tools/taskboard/cli.mjs context` for current state. Reading the
full `tasks/STATUS.md` is justified only when updating it, auditing a specific
claim, or following an evidence path from the digest.
When reading any medium/high-cost local context file directly, record it with
`tools/ai_profile/context.mjs` so the later review can name the file instead of
only reporting missing metadata.

Examples:

```json
{"path": "tasks/STATUS.md", "chars": 58000, "reason": "current gate lookup"}
{"path": "AI_PIPELINE_RETROSPECTIVE_2026-06-13.md", "chars": 19000, "reason": "reflection follow-up"}
```

If a context compaction happens, log:

```json
{
  "ts": "2026-06-13T10:30:00+05:00",
  "phase": "release_audit",
  "category": "context",
  "intent": "Resume after context compaction",
  "result": "mixed",
  "value": "necessary_overhead",
  "context_risk": "high",
  "notes": "Recovered from summary; checked STATUS before continuing."
}
```

## Summarize

Run:

```powershell
node tools/ai_profile/summarize_session_profile.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
```

For session closeout, write a scratch summary artifact:

```powershell
node tools/ai_profile/summarize_session_profile.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl --output tmp/session_profiles/session_profile_YYYY-MM-DD.summary.md
```

The summary is still printed to stdout. The `.summary.md` file is ignored by
git by default because it lives under `tmp/`.

For normal long-session closeout, prefer:

```powershell
node tools/ai_profile/closeout.mjs
```

`closeout.mjs` appends a final closeout event, writes the `.summary.md`, prints
both paths, and keeps all raw/profile artifacts under `tmp/session_profiles/`.

The summary reports:

- total records and profiled duration;
- duration by phase/category/value/result;
- tool usage counts;
- command count and failures;
- most-read and most-written files;
- context input character totals;
- waste/rework records;
- blockers and evidence.

For reflection prep, run:

```powershell
node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl --output tmp/session_profiles/session_profile_YYYY-MM-DD.review.md
```

The review is not a replacement for judgment. It is a fast checklist of what
the agent must explain or convert into process improvements.

For automation or cross-agent handoff, also write the machine-readable review:

```powershell
node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl --output tmp/session_profiles/session_profile_YYYY-MM-DD.review.md --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.review.json
```

The JSON artifact uses `schema_version: 1` and contains findings, repeated
command scopes, missing context-input details, and suggested pipeline actions.
Keep it in `tmp/session_profiles/` unless the lead explicitly asks to preserve
it.

To turn structured findings into reviewable next actions, generate follow-up
drafts:

```powershell
node tools/ai_profile/followups.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.review.json --output tmp/session_profiles/session_profile_YYYY-MM-DD.followups.md --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.followups.json
```

Follow-up drafts are not tasks yet. Promote only still-relevant items after
checking current tasks and recent commits.

When the review shows repeated broad commands, plan the next validation loop
before running it:

```powershell
node tools/ai_profile/plan_validation.mjs --change profiling --change skills --risk medium
```

The planner prints commands but does not run them. Profile substantial commands
with `run.mjs`, batch broad/final gates, and record any intentionally skipped
expected gate in the task log.

## Definition Of Done

A "profiled session" is done when:

- JSONL exists and parses;
- command work used `run.mjs` for substantial validations/builds/audits;
- non-command context decisions used sparse `event.mjs` checkpoints, and local
  medium/high context reads used `context.mjs` where possible;
- the summary script passes and writes a scratch `.summary.md` closeout when
  the session is long enough to reflect on;
- `closeout.mjs` is used for normal session closeout unless a custom summary
  path or manual investigation is needed;
- `review.mjs` is used before deeper reflection when a profile exists;
- `review.mjs --json-output` is used when another tool/agent will consume the
  findings instead of a human reading the markdown;
- `followups.mjs` is used when review JSON should become draft next actions
  for task/rule/tool promotion;
- `plan_validation.mjs` is used before rerunning broad validation after the
  profile review has identified repeated commands or validation waste;
- the final response names profile path and summary path;
- any repeated waste is converted into a task, skill rule, or pipeline rule;
- the retrospective uses the profile instead of relying only on memory.
