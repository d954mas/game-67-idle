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
- `tools/ai.mjs`
- `tools/ai_profile/*`
- skill/rule updates that make profiling part of the agent workflow
- task files that track profiling pipeline work
- compact lessons in `AI_PIPELINE_ITERATION_LOG.md`

Do not commit raw session telemetry by default:

- `tmp/session_profiles/*.jsonl`
- generated `*.summary.md`, `*.review.md`, `*.review.json`,
  `*.followups.md`, and `*.followups.json`
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

For normal work, use the fast facade:

```powershell
node tools/ai.mjs start <task-id> <iteration-name>
node tools/ai.mjs focus <next-iteration-name>
node tools/ai.mjs context
node tools/ai.mjs context --path <file>
node tools/ai.mjs context -- <command> <args>
node tools/ai.mjs checkpoint "Reviewed current reflection and chose next fix"
node tools/ai.mjs run -- <command> <args>
node tools/ai.mjs validate --change <kind> --risk <risk>
node tools/ai.mjs status
node tools/ai.mjs reflect
```

This keeps analytics attached to real work without forcing the agent to manage
the profiler as a separate project. Use `focus` after a commit, process fix, or
direction change when the work item stays the same but current-scope review
should stop carrying older regressions forward. `reflect` prepares the full
handoff chain when a baseline exists: a thresholded pre-reflection gap
checkpoint, closeout bundle, baseline comparison, reflection packet, draft, and
review. It continues when current-scope regressions exist so those regressions
become reflection evidence; use `node tools/ai.mjs reflect --strict` to stop on
regressions, `node tools/ai.mjs reflect --quick` only for a cheap closeout
summary without the full handoff, or `--no-gap-checkpoint` only when debugging
telemetry behavior.
Use `tools/ai_profile/*` directly only when debugging telemetry, running a
one-off deep analysis, or changing the profiler.

For playable game iterations, `node tools/ai.mjs context` profiles and prints
the compact game context pack. To write the pack JSON explicitly, run:

```powershell
node tools/game_context/iteration_context.mjs --json-output tmp/game_iteration_context_<task>.json
```

This records the pre-code context used for the iteration: active concept,
native/web harness gates, reference-study gate, visual/art gate, current
project gate, next priorities, source files, and validation commands. Use it
to audit later whether the agent had the right context before choosing a
runtime, generating art, or implementing gameplay.

For other medium/high-cost context reads, use the same facade with explicit
inputs: `node tools/ai.mjs context --path <file>` for local files, or
`node tools/ai.mjs context -- <command>` for read-only commands whose output
the agent uses as context.

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
  "work_item": "T0055",
  "iteration": "top-hud-polish",
  "tools": ["shell_command"],
  "commands": ["py -3.12 tools/project_67_world/devapi_scenarios/child_test_readiness.py ..."],
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

`work_item` is the durable task, issue, ticket, phase, or user-request id that
the event belongs to. Use it for any profile that spans more than one task.
For this repo, prefer task IDs such as `T0072`; for other projects, use their
native work item IDs.

`iteration` is a smaller batch label inside the work item, such as
`profile-metadata`, `visual-audit-pass-2`, or `release-smoke-rerun`. Use it
when one work item has multiple distinct loops.

For a focused work session, set default metadata once instead of repeating it
on every command. In Codex-style tool loops where shell environment does not
persist reliably across tool calls, prefer the persistent scope file:

```powershell
node tools/ai_profile/scope.mjs set --work-item T0078 --iteration persistent-scope
```

For an ordinary persistent terminal, environment defaults are also supported:

```powershell
$env:AI_PROFILE_WORK_ITEM = "T0077"
$env:AI_PROFILE_ITERATION = "env-defaults"
```

Explicit `--work-item` and `--iteration` flags override these environment
defaults and the persistent scope file for one command.

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

At the start of a focused task or long iteration, prefer one command:

```powershell
node tools/ai_profile/start.mjs --work-item T0080 --iteration profile-start-helper --phase profiling --intent "Start profiler helper work"
```

`start.mjs` writes the persistent scope file and appends a `phase_start`
checkpoint, so later `run.mjs`, `event.mjs`, and `context.mjs` records inherit
the same work-item/iteration metadata without repeating flags.

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

For long non-command stretches, prefer:

```powershell
node tools/ai.mjs checkpoint "Reviewed generated art and selected fixes" --category art --value productive
```

The facade uses the thresholded gap checkpoint by default and writes only when
the latest profile gap is meaningful. It defaults to `--min-gap-min 2` so
ordinary short pauses do not clutter the profile. Use `--force` when a short
manual stretch must be recorded exactly, and pass `--duration-ms` when the
elapsed time is known more precisely.

When the pause may be short and you only want to close meaningful coverage
holes, use the thresholded helper:

```powershell
node tools/ai_profile/gap_checkpoint.mjs --intent "Reviewed reflection output and chose next tool improvement" --min-gap-min 5
```

`gap_checkpoint.mjs` skips without writing a record when the latest profile gap
is shorter than the threshold. Use it before reflection handoff or after a
manual/research/review stretch when you do not want to add noise for short
pauses.

For context-file reads, prefer the measured helper:

```powershell
node tools/ai.mjs context --path AGENTS.md --path tasks/README.md --reason "session start"
```

The facade measures character counts, records `files_read`, fills
`context_inputs`, and assigns context risk automatically unless `--context-risk`
is provided. Use it instead of manually typing `--context-input` when the
source is a local file.

For read-only commands that produce context, prefer:

```powershell
node tools/ai.mjs context --intent "Load current task digest" --reason "session resume" -- node tools/taskboard/cli.mjs context
```

This prints the wrapped command output, records command text, duration, exit
code, and measures stdout/stderr as one `context_inputs` entry. Use it for
`taskboard context`, generated summaries, profile reviews, search summaries, or
other command output that the agent reads as context.

Use project-relative paths when possible. `--context-input` also accepts
Windows absolute paths because the parser treats the first numeric segment as
the character-count separator.

Both commands default to:

```text
tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
```

Override with `--profile <path>` only for tests or when comparing profiles.

To check telemetry health during a long session without closing it, run:

```powershell
node tools/ai_profile/status.mjs
```

Use `--json-output tmp/session_profiles/session_profile_YYYY-MM-DD.status.json`
when a dashboard, follow-up tool, or later agent should consume the status.

For long or multi-task sessions, add segmentation to every substantial command
or checkpoint:

```powershell
node tools/ai_profile/run.mjs --phase validation --category validation --intent "Check profile metadata" --work-item T0072 --iteration profile-metadata -- node --check tools/ai_profile/profile_lib.mjs
node tools/ai.mjs context --intent "Measure profiling docs" --path AI_PIPELINE_SESSION_PROFILING.md --reason "profile metadata docs"
```

If most commands belong to the same task, prefer `AI_PROFILE_WORK_ITEM` and
`AI_PROFILE_ITERATION` defaults or `tools/ai_profile/scope.mjs set`, and use
explicit flags only for exceptions.

## Tool Profiling

Record tools by role:

- `shell_command`: command, duration, pass/fail, whether it was narrow or broad.
- `start.mjs`: one-command iteration starter. Prefer it at the beginning of a
  focused profiled task because it writes persistent scope and a `phase_start`
  checkpoint together.
- `ai.mjs focus`: fast facade for starting a new iteration inside the current
  work item. Use it after a commit or process fix so current-scope reflection
  measures the next slice instead of the whole umbrella task.
- `run.mjs`: default wrapper for shell commands. Prefer it for expensive,
  repeated, or validation commands so duration and exit code are captured
  automatically.
- `event.mjs`: low-cost checkpoint writer for non-command events.
- `checkpoint.mjs`: wall-clock checkpoint helper for manual, research, design,
  review, and other non-command stretches. It infers `duration_ms` from the
  previous profile record and caps long unknown gaps by default.
- `gap_checkpoint.mjs`: thresholded wall-clock checkpoint helper that writes a
  checkpoint only when the gap since the latest profile record is at least
  `--min-gap-min` (default 5).
- `ai.mjs checkpoint`: fast facade for the thresholded checkpoint helper. Use
  this in normal work; call `checkpoint.mjs` or `gap_checkpoint.mjs` directly
  only when debugging or needing non-default internals.
- `ai.mjs context --path`: fast facade for measured local file context.
- `ai.mjs context -- <command>`: fast facade for measured read-only command
  context such as `node tools/taskboard/cli.mjs context`.
- `context.mjs` and `context_command.mjs`: low-level context tools used by the
  facade; call them directly only when debugging or customizing profiler
  internals.
- `scope.mjs`: persistent session-scope helper that writes
  `tmp/session_profiles/current_scope.json` so work-item/iteration defaults
  survive separate tool command invocations.
- `status.mjs`: mid-session helper that reports current profile health without
  appending records: latest event, closeout/bundle presence, work-item
  coverage, missing context inputs, wall-clock coverage, failed records, and
  one suggested next profiling action. It separates recovered failed commands
  from unresolved failed commands when a later matching command passes, and
  distinguishes current scope setup problems from historical missing metadata.
  When current scope has `updated_at`, status reports current-scope record and
  missing-context counts separately, including current-scope wall-clock
  coverage; use current-scope health for next actions and whole-profile totals
  for retrospective history.
  Treat `Bundle complete: yes` as insufficient for reflection unless
  `Bundle fresh: yes` also holds; stale bundles were generated before the
  latest profile records.
  Status also reports captured baseline manifests from
  `tmp/session_profiles/baselines/`; if a clean profile already has a captured
  baseline, use the reported baseline for the next comparison instead of
  recapturing. When a baseline exists, status reports whether the latest
  comparison JSON is missing, stale, regressed, or fresh, and prints the exact
  `compare_reviews.mjs` command when comparison evidence must be refreshed.
  After bundle and comparison are fresh, status reports whether reflection
  packet, draft, and review artifacts are missing, stale, waiting, or fresh,
  and prints the exact generation commands. Use that status instead of
  manually guessing the next reflection handoff step.
- `test.mjs`: regression test suite for profile CLI behavior. Run
  `node --test tools/ai_profile/test.mjs` after profiler tool changes.
- `closeout.mjs`: end-of-session helper that appends a final closeout event
  and writes a scratch reflection bundle: summary, review markdown/JSON, and
  follow-up markdown/JSON.
- `capture_baseline.mjs`: baseline helper that copies a clean review JSON to
  `tmp/session_profiles/baselines/<label>.review.json` and writes a manifest
  with source, target, current-scope counts, and a ready compare command. Use
  it before later closeout/review commands overwrite the daily review JSON.
- `reflection_packet.mjs`: reflection handoff helper that gathers review JSON,
  follow-up drafts, captured baseline, and baseline comparison into one compact
  scratch markdown/JSON packet before writing a full retrospective.
- `reflection_draft.mjs`: retrospective starter helper that reads a reflection
  packet JSON and its referenced review JSON, then writes scratch markdown/JSON
  with current state, pending and satisfied follow-ups, historical lessons,
  next-cycle actions, and caveats. Treat it as a draft; edit it with judgment.
- `prepare_reflection.mjs`: one-command handoff helper that runs only the
  stale/missing reflection preparation steps: closeout bundle, baseline
  comparison, reflection packet, reflection draft, and reflection review. It
  does not capture baselines automatically and stops on current-scope
  regressions unless `--allow-regression` is explicit.
- `reflection_review.mjs`: compact decision-review helper that consumes
  `reflection_draft.mjs --json-output` and separates current actions from
  historical-only lessons, repeated-command interpretation, and top next-cycle
  improvements.
- `review.mjs`: reflection prep helper that turns a JSONL profile into
  priority findings: waste/rework, failures, blockers, context hotspots,
  tool-use summary by recorded tool id,
  repeated commands, repeated command scope (`preflight`, `scoped`,
  `broad/final`, `unknown`), repeated broad/final commands by work item,
  repeated command classification (`planned_validation`,
  `validation_waste_risk`, `failure_recovery_or_rework`,
  `guardrail_rerun_review`, `needs_manual_classification`),
  wall-clock coverage and largest unprofiled gaps, missing work-item metadata,
  missing context input details, recovered versus unresolved failed records,
  and suggested pipeline actions.
- `observability_gate.mjs`: decision helper for proposed external AI
  observability/eval platforms. Use it before adopting LangSmith, Phoenix,
  Langfuse, Braintrust, OpenTelemetry/OTLP export, or similar tooling. The
  local JSONL profile stays the baseline; external systems start as bounded
  pilots unless the pilot already proved a repeated time saving or a team/eval
  workflow the local profile cannot cover.
- `plan_validation.mjs`: pre-validation helper that prints a narrow-to-broad
  validation ladder for the changed work kind. Use it when `review.mjs` reports
  repeated commands, before broad reusable-base checks, or when the right
  validation scope is unclear.
- `validation_run.mjs`: profiled validation runner that consumes a validation
  plan or builds one from `--change/--file/--risk`, executes non-placeholder
  checks by tier, records each command in the JSONL profile, and skips later
  checks after a failure unless `--continue-on-fail` is explicit. Each command
  record gets a shared `validation_batch_id`, plan risk, plan changes, tier,
  and check id so reflection can separate a planned validation batch from ad
  hoc repeated commands.
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
`node tools/ai.mjs context --path <file>` so the later review can name the file
instead of only reporting missing metadata. For command output used as context,
wrap it with `node tools/ai.mjs context -- <command>`.

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

`closeout.mjs` appends a final closeout event, writes the `.summary.md`,
`.review.md`, `.review.json`, `.followups.md`, and `.followups.json`, prints
their paths, and keeps all raw/profile artifacts under `tmp/session_profiles/`.
Use `--no-review` for summary-only closeout, or `--no-followups` when review
artifacts are needed but follow-up drafts are intentionally skipped.

For normal retrospective preparation, prefer the one-command wrapper:

```powershell
node tools/ai_profile/prepare_reflection.mjs --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.status.json
```

It uses `status.mjs` and existing tools to run only stale or missing steps in
the closeout, baseline comparison, reflection packet, reflection draft, and
reflection review chain. It refuses to auto-capture baselines; review and
capture a clean baseline deliberately. It also stops on current-scope
comparison regressions unless `--allow-regression` is explicit.

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
When `closeout.mjs` was used without `--no-review`, these review artifacts are
already generated and this manual review command is only needed after further
profile edits. Read `Current Scope Findings` and `Current Scope Actions`
first. Treat `Historical Whole-Profile Findings` as retrospective history
unless the same issue appears in the current scope.

For automation or cross-agent handoff, also write the machine-readable review:

```powershell
node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl --output tmp/session_profiles/session_profile_YYYY-MM-DD.review.md --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.review.json
```

The JSON artifact uses `schema_version: 1` and contains findings, repeated
command scopes, work-item/iteration summaries, repeated broad/final commands
by work item, `wall_clock_coverage`, `current_scope`, missing context-input
details, and suggested pipeline actions. When scope is active, inspect
`current_scope.findings` and `current_scope.suggested_actions` before acting
on whole-profile `findings`. Keep it in `tmp/session_profiles/` unless the
lead explicitly asks to preserve it.

To turn structured findings into reviewable next actions, generate follow-up
drafts:

```powershell
node tools/ai_profile/followups.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.review.json --output tmp/session_profiles/session_profile_YYYY-MM-DD.followups.md --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.followups.json
```

Follow-up drafts are not tasks yet. Promote only still-relevant items after
checking current tasks and recent commits. Drafts include repeated validation,
missing context inputs, missing work-item metadata, low wall-clock coverage,
recovered/unresolved failed records, and waste/rework when those findings
appear in review JSON.
When review JSON includes `current_scope`, follow-up drafts use current-scope
health for urgent P1 suggestions and list suppressed historical-only findings
separately. Historical findings still belong in retrospectives, but they should
not keep creating current action items after the active scope is clean.
Recovered failed records follow the same rule: historical recovered failures
are retrospective learning notes, while current-scope recovered failures can
still become follow-up drafts if they reveal recurring rework.
When `closeout.mjs` was used without `--no-followups`, these drafts are already
generated and this manual command is only needed after rerunning review.

When a profile is clean enough to become a baseline, capture the review JSON
before later closeout/review commands overwrite the daily artifact:

```powershell
node tools/ai_profile/capture_baseline.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.review.json --label clean-profile-baseline
```

Then compare the next review JSON against the captured baseline instead of
manually reading two artifacts:

```powershell
node tools/ai_profile/compare_reviews.mjs tmp/session_profiles/baselines/clean-profile-baseline.review.json tmp/session_profiles/current.review.json --output tmp/session_profiles/profile_compare.md --json-output tmp/session_profiles/profile_compare.json
```

Use `--fail-on-regression` only for automation that should fail when
current-scope metrics regress. The comparison treats current-scope regressions
as urgent and reports whole-profile deltas as historical trend, so old
telemetry debt does not keep creating current work.

When status reports a fresh bundle and fresh baseline comparison, generate a
compact reflection packet before writing the retrospective:

```powershell
node tools/ai_profile/reflection_packet.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl --output tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_packet.md --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_packet.json
```

The packet is scratch evidence. It cites source artifacts, current-scope
findings/actions, follow-up drafts, suppressed historical findings, and
baseline comparison regressions/trend. Keep it in `tmp/session_profiles/`
unless the lead explicitly asks to preserve it. Packet follow-ups are split
into pending and satisfied suggestions; do not promote satisfied suggestions
into tasks unless new evidence reopens the issue.

After a ready packet exists, generate a draft starter:

```powershell
node tools/ai_profile/reflection_draft.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_packet.json --output tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_draft.md --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_draft.json
```

The draft is not the final retrospective. Use it to avoid repeatedly opening
summary, review, follow-up, and comparison artifacts, then rewrite with
judgment, user-visible context, and project-specific examples.
When the draft includes `tool_use_summary`, use it to explain which tool
classes consumed time, failed, produced context, or created rework.
If `tool_use_summary` includes `(unrecorded)` or review reports
`missing_tool_metadata`, treat that as incomplete telemetry for future
sessions and use `node tools/ai.mjs run/context/checkpoint/validate` or
profiler wrappers that populate `tools`.
When the draft includes repeated-command evidence, classify the repeats before
turning them into process work. Prefer the generated
`repeated_command_classification` table over raw repeat counts: planned
validation, validation-waste risk, failure/rework signal, scoped/preflight
guardrail rerun, or manual-review case.

Before writing final retrospective prose, generate a compact decision review:

```powershell
node tools/ai_profile/reflection_review.mjs tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_draft.json --output tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_review.md --json-output tmp/session_profiles/session_profile_YYYY-MM-DD.reflection_review.json
```

Use this review to separate current action items from historical-only lessons
and to extract the top next-cycle improvements. It is still a generated aid,
not final prose. `Current actions` counts only real pending action items; when
the current scope is clean, the explanatory no-action text lives in
`current.status_message` and must not be promoted as a task.

When the review shows repeated broad commands, plan the next validation loop
before running it:

```powershell
node tools/ai_profile/plan_validation.mjs --change profiling --change skills --risk medium
```

For automation or handoff, also write a machine-readable plan:

```powershell
node tools/ai_profile/plan_validation.mjs --change profiling --change skills --risk medium --json-output tmp/session_profiles/validation_plan.json
```

The planner prints commands but does not run them. Its JSON plan includes
`checks_by_tier`, `broad_final_checks`, `broad_final_count`,
`deferred_broad_count`, and `next_action` so agents and tools can avoid parsing
markdown. Profile substantial commands with `run.mjs`, batch broad/final gates,
and record any intentionally skipped expected gate in the task log.

When the plan contains concrete commands and the goal is to validate an AI
pipeline/tooling change, prefer the facade over manually chaining checks:

```powershell
node tools/ai.mjs validate --change profiling --change skills --risk medium
```

Use `--tier preflight --tier scoped` for an early pass that intentionally
defers broad/final checks, or let `validate` execute final checks once at the
end of the selected batch. Placeholder commands are skipped and listed in the
summary; fill project-specific native, asset, web, or release commands manually
when those surfaces are in scope. Use `tools/ai_profile/validation_run.mjs`
directly only when debugging the validation runner or writing a machine-readable
summary JSON for another tool.
When reviewing repeated validation, inspect `review.mjs`'s `Validation
Batches` and `Broad/Final Validation Classification` sections before treating
repeated commands as waste. A planned batch with one final gate is different
from ad hoc reruns of the same broad command; use
`repeated_unbatched_broad_final_commands` for waste decisions and
`batched_broad_final_commands` as planned validation evidence.

## Definition Of Done

A "profiled session" is done when:

- JSONL exists and parses;
- a focused long task started with `node tools/ai.mjs start`, and later slices
  inside the same work item used `node tools/ai.mjs focus` when old current
  issues should stop carrying forward; otherwise the retrospective explains why
  profiling began late or why one wide scope was intentional;
- command work used `run.mjs` for substantial validations/builds/audits;
- long manual/research/design/review stretches used `node tools/ai.mjs
  checkpoint "<intent>"` with an intent that explains the elapsed time, or the
  retrospective marks the gap as unknown;
- non-command manual/research/review stretches used `node tools/ai.mjs
  checkpoint "<intent>"`, and local medium/high context reads used `node
  tools/ai.mjs context --path <file>` where possible;
- read-only commands that produced medium/high context used `node tools/ai.mjs
  context -- <command>` where possible;
- `status.mjs` is used during long sessions when the agent needs to know
  whether current telemetry is missing work-item metadata, context inputs,
  coverage, closeout, or bundle artifacts; if status reports historical
  missing work-item records while current scope is set, do not reset scope
  only to repair old records; if whole-profile missing context inputs are old
  but current-scope missing context inputs are zero, do not rerun context
  capture solely to repair history; if whole-profile low coverage is old but
  current-scope coverage is acceptable or too short to judge, do not add
  checkpoint events solely to repair history;
- `node --test tools/ai_profile/test.mjs` passes after changes to profiler
  tools, review/status/followup behavior, or scope/default handling;
- long or multi-task profiles include `--work-item <id>` on substantial
  commands and checkpoints, with `--iteration <name>` when the work item has
  separate loops, or equivalent `AI_PROFILE_WORK_ITEM` /
  `AI_PROFILE_ITERATION` defaults are set for the shell session, or
  `tools/ai_profile/scope.mjs set` is used for persistent tool-call scope;
- the summary script passes and writes a scratch `.summary.md` closeout when
  the session is long enough to reflect on;
- `closeout.mjs` is used for normal session closeout unless manual
  investigation is needed;
- the closeout bundle includes summary, review markdown/JSON, and follow-up
  markdown/JSON unless explicitly skipped with `--no-review` or
  `--no-followups`;
- `status.mjs` reports `Bundle fresh: yes` before a retrospective relies on
  generated summary/review/follow-up artifacts; if it reports stale artifacts,
  rerun `closeout.mjs` or the stale review/follow-up commands first;
- `status.mjs` reports fresh reflection packet, draft, and review artifacts
  before a full retrospective starts from generated handoff evidence; if
  packet, draft, or review is missing/stale, run the exact command printed in
  `Reflection Artifacts`;
- `review.mjs` is used before deeper reflection when a profile exists and the
  closeout bundle was skipped or stale;
- `review.mjs --json-output` is used when another tool/agent will consume the
  findings instead of a human reading the markdown; when a persistent scope is
  active, inspect `current_scope.findings` and
  `current_scope.suggested_actions` before treating whole-profile history as a
  current problem;
- low wall-clock coverage or large profile gaps are explained in the
  retrospective, or the next cycle adds `node tools/ai.mjs checkpoint
  "<intent>"` records for long manual/research/design stretches;
- `followups.mjs` is used when review JSON should become draft next actions
  for task/rule/tool promotion and the closeout bundle was skipped or stale;
  if it reports `suppressed_historical_findings`, mention those in the
  retrospective but do not promote them as current tasks unless they recur in
  the current scope; this includes historical recovered failures that already
  passed later. For broad/final validation follow-ups, current action status is
  based on `current_scope.repeated_unbatched_broad_final_commands`, not total
  batched broad/final repeats;
- `compare_reviews.mjs` is used when a previous clean review JSON should act
  as a baseline for a later profile; inspect current-scope regressions first
  and treat whole-profile deltas as trend evidence; check `status.mjs` first
  so missing or stale comparison artifacts are regenerated before trend claims;
- `capture_baseline.mjs` is used before relying on a clean review JSON as a
  future baseline, because normal closeout/review filenames are overwritten by
  later runs; check `status.mjs` first and recapture only when status reports
  no captured baseline for the profile;
- `reflection_packet.mjs` is used before writing a full retrospective when
  generated profile artifacts exist, so the reflection starts from one compact
  scratch evidence packet instead of repeatedly opening summary, review,
  follow-up, and compare files; satisfied packet follow-ups are not promoted
  into tasks;
- `reflection_draft.mjs` is used after a ready packet when the agent needs to
  write a full retrospective; the draft is read and edited with judgment, not
  pasted as final output;
- `prepare_reflection.mjs` is used for normal retrospective handoff so agents
  do not manually repeat closeout/compare/packet/draft/review command
  sequences; use manual commands only when the wrapper reports missing
  baseline, regression, or another explicit stop condition;
- `reflection_review.mjs` is used after a fresh reflection draft before writing
  final retrospective prose, so current actions, historical-only lessons, and
  top improvements are reviewed explicitly;
- `plan_validation.mjs` is used before rerunning broad validation after the
  profile review has identified repeated commands or validation waste; use
  `--json-output` when another tool, agent, or later reflection should consume
  the validation decision;
- `validation_run.mjs` is used when the validation plan has concrete commands
  and command-level telemetry matters; broad/final checks run once at the end
  of the selected batch, skipped placeholders are visible in the summary, and
  `review.mjs` later reports validation batches plus batched/unbatched
  broad/final classification as context for repeated-command interpretation;
- `observability_gate.mjs` is used before adding external tracing/eval
  platforms; the decision and pilot result are captured in a task log or
  durable pipeline note, while raw exported telemetry stays in `tmp/`;
- the final response names profile path and summary path;
- any repeated waste is converted into a task, skill rule, or pipeline rule;
- the retrospective uses the profile instead of relying only on memory.
