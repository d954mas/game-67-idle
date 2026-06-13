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
`AI_PIPELINE_SESSION_PROFILING.md` and the fast facade in `tools/ai.mjs`.
A proper profile
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
`node tools/ai.mjs start <id> <iteration>` at the beginning of a focused
iteration, `node tools/ai.mjs focus <next-iteration>` after a commit, process
fix, or direction change inside the same work item, `node tools/ai.mjs context`
before implementation,
`node tools/ai.mjs checkpoint "<intent>"` after long manual/research/review
stretches, `node tools/ai.mjs context --path <file>` for medium/high local
context reads, `node tools/ai.mjs context -- <command>` for read-only command
output used as context, and `node tools/ai.mjs run -- <command>` for
substantial commands.
Do not wait until the end to reconstruct avoidable telemetry from chat history.
During long sessions, run `node tools/ai.mjs status` when telemetry health
is unclear. It is read-only and reports latest event, closeout/bundle presence,
work-item coverage, missing context inputs, wall-clock coverage, failed
records, recovered versus unresolved failed records, and the next profiling
action. It also reports captured baseline manifests when clean review JSON has
already been preserved under `tmp/session_profiles/baselines/`, plus whether
the latest baseline comparison is missing, stale, regressed, or fresh. After a
fresh bundle and comparison, it reports reflection packet/draft/review
freshness and prints the exact commands to regenerate missing or stale handoff
artifacts.
Anchor: reflection packet/draft/review freshness.
Before relying on generated summary/review/follow-up artifacts, check that
status reports `Bundle fresh: yes`; if the bundle is stale, rerun
`tools/ai_profile/closeout.mjs` or the stale review/follow-up commands.
For normal retrospective preparation, run
`tools/ai_profile/prepare_reflection.mjs --json-output
tmp/session_profiles/<name>.status.json` before manually chaining closeout,
compare, packet, draft, or review commands. If it stops on missing baseline or
current-scope regressions, handle that explicit decision before continuing.
If status shows historical missing work-item records but current scope is
already set, keep the scope and fix the next reported current issue instead of
resetting metadata for old records.
If the current issue was already fixed earlier in the same wide work item, use
`node tools/ai.mjs focus <next-iteration>` to start a fresh current-scope slice
instead of letting old records keep the current review dirty.
When status reports `current_scope`, use current-scope missing context and
work-item counts for next actions; use whole-profile totals as retrospective
history unless the current-scope counts are also bad.
Apply the same rule to wall-clock coverage: if whole-profile coverage is low
but current-scope coverage is acceptable or too short to judge, do not add
checkpoint records solely to repair old history.

Before writing a retrospective from a live profile, run
`node tools/ai.mjs reflect`. It first runs a thresholded gap checkpoint to
capture any long manual/research/review stretch since the latest profile
record, then prepares the closeout bundle, baseline comparison, reflection
packet, draft, and review when the required baseline is available. It continues
through current-scope regressions so they are visible in the reflection
handoff; use `node tools/ai.mjs reflect --strict` to stop on regressions, `node
tools/ai.mjs reflect --quick` only when a cheap closeout summary is enough, or
`--no-gap-checkpoint` only when debugging telemetry behavior.
If the full handoff cannot be generated, state what is missing instead of
manually chaining commands unless you are debugging the profiler.
For low-level session closeout, `tools/ai_profile/closeout.mjs` records a
closeout event and writes the summary, review markdown/JSON, and follow-up
markdown/JSON in one closeout bundle.
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
When a review JSON is clean enough to become a baseline, check `status.mjs`
first. If status reports no captured baseline, capture it with
`tools/ai_profile/capture_baseline.mjs <review.json> --label <name>` so later
closeout/review commands do not overwrite the daily artifact. When a previous
clean baseline review JSON is available, run
`tools/ai_profile/compare_reviews.mjs <baseline.review.json>
<current.review.json> --output tmp/session_profiles/<name>.compare.md
--json-output tmp/session_profiles/<name>.compare.json` before writing trend
claims. Treat current-scope regressions from the compare output as urgent and
whole-profile deltas as historical trend evidence. If status reports a missing
or stale baseline comparison, run the exact compare command from status before
writing trend claims.
When status reports a fresh bundle and fresh baseline comparison, generate a
compact packet with `tools/ai_profile/reflection_packet.mjs <profile.jsonl>
--output tmp/session_profiles/<name>.reflection_packet.md --json-output
tmp/session_profiles/<name>.reflection_packet.json` and read that packet first.
Use it as the first evidence map before opening larger summary, review,
follow-up, or comparison artifacts. Do not promote packet follow-ups marked
`satisfied` into tasks unless new evidence reopens the issue.
If status reports packet/draft/review missing or stale, run the exact command
from its `Reflection Artifacts` section before writing the retrospective.
After the packet is ready, generate a scratch starter with
`tools/ai_profile/reflection_draft.mjs <packet.json> --output
tmp/session_profiles/<name>.reflection_draft.md --json-output
tmp/session_profiles/<name>.reflection_draft.json`. Read the draft and edit it with judgment; do not treat it as the final retrospective.
After the draft is ready, generate a compact decision review with
`tools/ai_profile/reflection_review.mjs <draft.json> --output
tmp/session_profiles/<name>.reflection_review.md --json-output
tmp/session_profiles/<name>.reflection_review.json`. `prepare_reflection.mjs`
does this automatically when the review is missing or stale. Use the review to
separate current actions from historical-only lessons and to extract the top
next-cycle improvements before writing final prose.
In reflection review JSON, `current.actions` counts only real pending work; a
clean-scope no-action explanation belongs in `current.status_message` and must
not be treated as an action item.
Anchor: top next-cycle improvements.
When the draft includes `tool_use_summary`, use it to explain which tool
classes consumed time, failed, produced context, or created rework.
If `tool_use_summary` includes `(unrecorded)` or review reports
`missing_tool_metadata`, treat that as incomplete telemetry for future
sessions and use `node tools/ai.mjs run/context/checkpoint/validate` or
profiler wrappers that populate `tools`.
When the draft or review includes `context_use_summary`, read it before
opening larger review JSON or long source files. Use its hotspots to name the
largest context inputs and its missing inputs to explain where context cost was
not measured.
When the handoff includes current-scope tool/context summaries, use those first
for the just-finished iteration. Treat whole-profile tool/context summaries as
historical trend evidence unless the same issue appears in current scope.
When the handoff includes a current-scope snapshot, use it as the first
iteration-size check: records, profiled versus wall-clock time, telemetry gaps,
and unresolved/recovered failures. Do not reopen `status` just to recover those
numbers.
When the reflection review includes `Current Scope Readout`, start the final
retrospective from that synthesized current-iteration summary before discussing
historical lessons or whole-profile trends.
When the handoff includes current-scope validation batches, use them to explain
validation-runner tool cost as planned validation evidence before calling it
waste or rework.
When the draft includes repeated-command evidence, classify repeats before
turning them into process tasks. Prefer `repeated_command_classification` over
raw repeat counts: planned validation, validation-waste risk,
failure/rework signal, scoped/preflight guardrail rerun, or manual-review
case.
When `review.mjs` or the draft reports validation batches, use that batch
evidence to separate planned validation runs from ad hoc repeated commands.
For broad/final validation, prefer `repeated_unbatched_broad_final_commands`
over the total broad/final count when deciding whether repeated validation was
waste; `batched_broad_final_commands` is planned validation evidence. Use
`repeated_unbatched_broad_final_occurrences` to state the scale of the waste
risk, because the command array count only reports distinct repeated commands.
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
When review JSON includes `recovered_failure_classification`, use those labels
before writing failure lessons: useful validation feedback, avoidable rework,
or tool/environment noise.
For broad/final validation follow-ups, current action status must use
`current_scope.repeated_unbatched_broad_final_commands`; batched current-scope
final gates are planned validation evidence, not a pending action.
If many records lack work-item metadata, add a next-cycle fix: begin the task
with `node tools/ai.mjs start <work-item> <iteration>`, then use
`node tools/ai.mjs focus <next-iteration>` for later slices inside the same
work item. Use low-level scope/start scripts only when debugging profiler
internals or writing custom profile records.
Inspect `wall_clock_coverage`, `low_profile_coverage`, and largest gaps before
claiming the profile explains the whole session. Low coverage means the
retrospective must name what likely happened in the unprofiled gaps, or add a
next-cycle rule to place `node tools/ai.mjs checkpoint "<intent>"` checkpoints
during long manual, research, design, or review stretches so the elapsed time
is recorded with `duration_ms`. The facade records only meaningful gaps by
default; use `--force` only when a short stretch must be captured exactly.
If review reports missing context input details, name the affected line/intents
and add a next-cycle fix: use `node tools/ai.mjs context --path <file>` for
local medium/high context reads, or `node tools/ai.mjs context -- <command>`
for read-only commands that print context such as `node tools/taskboard/cli.mjs
context`, so context size is measured automatically.
If review reports recovered failed records, classify them as useful negative
feedback, avoidable rework, or tool noise. Treat only unresolved failed records
as current blockers.
If profile review finds repeated validation or unclear validation scope, use
`node tools/ai.mjs validate --change <kind> --risk <risk>` for the next
validation loop and report the selected tier batch. Treat repeated broad gates
without a changed risk or failed previous gate as validation waste.
When a validation plan will be consumed by another tool/agent or cited in a
later reflection, write it with `--json-output` and inspect `broad_final_count`,
`deferred_broad_count`, and `next_action` instead of parsing markdown.
When the plan contains concrete commands for an AI pipeline/tooling change, use
`node tools/ai.mjs validate --change <kind> --risk <risk>` instead of manually
chaining checks or wrapping `node tools/pipeline_validate.mjs` with
`ai.mjs run`. It records each executed command in the profile, skips
placeholders, and runs broad/final checks once at the end of the selected tier
batch. Its command records share a `validation_batch_id`; later reflection
should inspect `Validation Batches` before treating repeated commands as waste.
Use `tools/ai_profile/validation_run.mjs` directly only when debugging the
validation runner or producing a custom machine-readable run summary.
After changing profiler tools or reflection telemetry behavior, run
`node --test tools/ai_profile/test.mjs` so scope, status, closeout, recovered
failure, and follow-up behavior remain covered.
