---
name: chat-session-reflection
description: Use when reflecting on a long AI-assisted work session, chat history, agent run, or multi-turn project iteration to identify bottlenecks, mistakes, wasted time, weak tool use, context loss, planning gaps, quality risks, and concrete process improvements. Triggers include requests for retrospective, reflection, postmortem, "where did you waste time", "where did you get stuck", pipeline improvement, or improving future 24+ hour AI development sessions.
---

# Chat Session Reflection

Produce a blunt retrospective, not a progress report. Optimize for finding
failure modes that can change the next session.

## Inputs

Use current durable evidence before memory:

1. Project rules and process docs: `AGENTS.md`, `AI_PIPELINE.md`,
   task-store rules, active skills.
2. Live status and task logs: `tasks/STATUS.md`, relevant `tasks/active/*`.
3. Evidence artifacts: reports, screenshots, package logs, validation outputs,
   generated assets, release/audit files.
4. Session profiling artifacts when present:
   `tmp/session_profiles/*.jsonl`, generated summaries, or recovered thread
   extracts. Treat these as scratch evidence, not git-bound deliverables.
5. Conversation context only after checking durable state.

If evidence is missing, label the claim as likely or unknown instead of making
it sound proven.

For context hygiene, start with `node tools/taskboard/cli.mjs context` and
follow only the task/status/evidence links needed for the retrospective. Read
the full `tasks/STATUS.md` only when auditing a specific status claim or
updating it; otherwise treat it as high-cost context.

## Workflow

1. State scope: session period, project, objective, and what evidence was
   inspected.
2. Reconstruct factual progress: completed tasks, decisions, artifacts, and
   validation. Keep it short.
3. Identify 5-10 largest time sinks. For each, separate:
   - symptom: what happened;
   - cause: bad task framing, missing context, weak plan, tool friction,
     implementation error, or agent behavior;
   - faster path: what should have happened.
4. Identify where the agent was wrong or inefficient. Include circular work,
   late discoveries, wrong assumptions, over-validation, premature coding,
   poor decomposition, and unnecessary artifacts.
5. Extract or use profiling telemetry. If a live profile exists, summarize
   time by phase/category/value, tool counts, repeated context loads, waste,
   rework, and compactions. If no live profile exists, use thread history when
   available and clearly mark the result as partial.
6. Audit tool use. Cover terminal/files/search/tests/generation/agents. Say
   where a tool was useful, late, missing, or wasted.
7. Audit context management. Name forgotten constraints, rediscovered
   decisions, stale source-of-truth files, and state that should have been
   pinned.
8. Audit planning. Name order mistakes, missing vertical slices, broad scopes,
   missing Definition of Done, and weak checkpoints.
9. Audit product quality. Review as a director: unclear gameplay, weak visuals,
   unreadable UI, brittle packaging, unproven manual acceptance, or technical
   success that still fails the product goal.
10. Propose a next-session workflow with concrete gates, not generic advice.
11. Propose prompt/system changes: added instructions, bans, checklists,
    readiness criteria, and role boundaries.
12. End with the 10 highest-leverage improvements.

## Output Rules

- Be specific and self-critical. Do not defend the agent.
- Use concrete examples from files, reports, screenshots, tasks, commands, or
  user corrections.
- Separate `symptom`, `cause`, and `fix` for each major issue.
- Distinguish product problems from pipeline problems.
- Prefer hard process changes over vague habits.
- Do not mark the project goal complete from a retrospective.
- Separate committed process improvements from scratch telemetry. Raw
  profiling logs, thread dumps, and one-off timing extracts belong in
  `tmp/session_profiles/` unless the lead explicitly asks to preserve them.

## Durable Capture

When the reflection yields reusable process lessons, add a short entry to
`AI_PIPELINE_ITERATION_LOG.md`. Keep the detailed retrospective in the chat or
a dedicated requested document; the iteration log is only for compact lessons.

If the reflection identifies actionable project work, create or update tasks in
`tasks/` instead of burying work inside the retrospective.

When the user asks for full AI-development profiling, use
`AI_PIPELINE_SESSION_PROFILING.md` and `tools/ai_profile/`. A proper profile
must separate useful work, necessary overhead, rework, and waste; report context
inputs and compactions; and state what was not measurable.
If the reflection suggests adopting external AI observability/eval tooling,
consult `AI_PIPELINE_OBSERVABILITY_TOOLS.md` and run
`tools/ai_profile/observability_gate.mjs` before recommending LangSmith,
Phoenix, Langfuse, Braintrust, OpenTelemetry/OTLP export, or similar systems.
Local JSONL remains the baseline unless a bounded pilot proves lower
reflection/debug time or enables a shared eval/review workflow the local profile
cannot cover.

For future long sessions, prefer collecting data during work with
`tools/ai_profile/start.mjs --work-item <id> --iteration <name>` at the
beginning of a focused profiled iteration, `tools/ai_profile/run.mjs` for
substantial commands, and `tools/ai_profile/event.mjs` for sparse checkpoints.
Do not wait until the end to reconstruct avoidable telemetry from chat history.
During long sessions, run `tools/ai_profile/status.mjs` when telemetry health
is unclear. It is read-only and reports latest event, closeout/bundle presence,
work-item coverage, missing context inputs, wall-clock coverage, failed
records, recovered versus unresolved failed records, and the next profiling
action.
Before relying on generated summary/review/follow-up artifacts, check that
status reports `Bundle fresh: yes`; if the bundle is stale, rerun
`tools/ai_profile/closeout.mjs` or the stale review/follow-up commands.
If status shows historical missing work-item records but current scope is
already set, keep the scope and fix the next reported current issue instead of
resetting metadata for old records.
When status reports `current_scope`, use current-scope missing context and
work-item counts for next actions; use whole-profile totals as retrospective
history unless the current-scope counts are also bad.
Apply the same rule to wall-clock coverage: if whole-profile coverage is low
but current-scope coverage is acceptable or too short to judge, do not add
checkpoint records solely to repair old history.

Before writing a retrospective from a live profile, run
`tools/ai_profile/summarize_session_profile.mjs <profile.jsonl> --output
tmp/session_profiles/<name>.summary.md` and cite the summary path. If the
summary cannot be generated, state that profiling evidence is incomplete.
For normal session closeout, prefer `tools/ai_profile/closeout.mjs`, which
records a closeout event and writes the summary, review markdown/JSON, and
follow-up markdown/JSON in one closeout bundle.
Run `tools/ai_profile/review.mjs <profile.jsonl> --output
tmp/session_profiles/<name>.review.md --json-output
tmp/session_profiles/<name>.review.json` only when the closeout bundle was
skipped, stale, or intentionally created with `--no-review`. The review
extracts waste/rework, failures, blockers, context hotspots, repeated commands,
and suggested pipeline actions before the human retrospective.
When reading review markdown, start with `Current Scope Findings` and
`Current Scope Actions`. Treat `Historical Whole-Profile Findings` as
retrospective history unless the same issue appears in the current scope.
Run `tools/ai_profile/followups.mjs` only when review JSON was rerun manually
or the closeout bundle was created with `--no-followups`. Promote only
still-relevant drafts into tasks, rules, or tools after checking current
taskboard state.
For multi-task profiles, inspect `work_items`, `iterations`, and
`repeated_broad_final_by_work_item` before calling repeated validation waste.
When review JSON includes `current_scope`, separate current-scope findings from
whole-profile history. Inspect `current_scope.findings` and
`current_scope.suggested_actions` before promoting whole-profile findings into
tasks. If follow-up drafts report
`suppressed_historical_findings`, mention those as historical lessons but do
not promote them as current tasks unless the same issue appears in the current
scope. Treat historical recovered failures as retrospective learning notes;
only current-scope or recurring recovered failures should become process tasks.
If many records lack work-item metadata, add a next-cycle fix: begin the task
with `tools/ai_profile/start.mjs --work-item <id> --iteration <name>`, pass
`--work-item <id>` and optional `--iteration <name>` to `run.mjs`,
`event.mjs`, `context.mjs`, and `closeout.mjs`, or set
`AI_PROFILE_WORK_ITEM` and `AI_PROFILE_ITERATION` defaults for the shell
session. In Codex-style separate tool calls, prefer
`tools/ai_profile/scope.mjs set --work-item <id> --iteration <name>` so the
scope persists outside a single shell command.
Inspect `wall_clock_coverage`, `low_profile_coverage`, and largest gaps before
claiming the profile explains the whole session. Low coverage means the
retrospective must name what likely happened in the unprofiled gaps, or add a
next-cycle rule to place `tools/ai_profile/checkpoint.mjs --intent <text>`
checkpoints during long manual, research, design, or review stretches so the
elapsed time is recorded with `duration_ms`.
If review reports missing context input details, name the affected line/intents
and add a next-cycle fix: use `tools/ai_profile/context.mjs --path <file>` for
local medium/high context reads, or
`tools/ai_profile/context_command.mjs -- <command>` for read-only commands
that print context such as `node tools/taskboard/cli.mjs context`, so context
size is measured automatically.
If review reports recovered failed records, classify them as useful negative
feedback, avoidable rework, or tool noise. Treat only unresolved failed records
as current blockers.
If profile review finds repeated validation or unclear validation scope, run
`tools/ai_profile/plan_validation.mjs --change <kind> --risk <risk>` before the
next validation loop and report the narrow/scoped/broad ladder. Treat repeated
broad gates without a changed risk or failed previous gate as validation waste.
When a validation plan will be consumed by another tool/agent or cited in a
later reflection, write it with `--json-output` and inspect `broad_final_count`,
`deferred_broad_count`, and `next_action` instead of parsing markdown.
After changing profiler tools or reflection telemetry behavior, run
`node --test tools/ai_profile/test.mjs` so scope, status, closeout, recovered
failure, and follow-up behavior remain covered.
