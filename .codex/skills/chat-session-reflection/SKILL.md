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

For future long sessions, prefer collecting data during work with
`tools/ai_profile/run.mjs` for substantial commands and
`tools/ai_profile/event.mjs` for sparse checkpoints. Do not wait until the end
to reconstruct avoidable telemetry from chat history.

Before writing a retrospective from a live profile, run
`tools/ai_profile/summarize_session_profile.mjs <profile.jsonl> --output
tmp/session_profiles/<name>.summary.md` and cite the summary path. If the
summary cannot be generated, state that profiling evidence is incomplete.
For normal session closeout, prefer `tools/ai_profile/closeout.mjs`, which
records a closeout event and writes the summary in one step.
Then run `tools/ai_profile/review.mjs <profile.jsonl> --output
tmp/session_profiles/<name>.review.md` to extract waste/rework, failures,
blockers, context hotspots, repeated commands, and suggested pipeline actions
before writing the human retrospective.
When the review findings need to feed another tool or agent, add
`--json-output tmp/session_profiles/<name>.review.json` and treat that JSON as
scratch telemetry unless the lead asks to preserve it.
If review reports missing context input details, name the affected line/intents
and add a next-cycle fix: use `tools/ai_profile/context.mjs --path <file>` for
local medium/high context reads so file sizes are measured automatically.
If profile review finds repeated validation or unclear validation scope, run
`tools/ai_profile/plan_validation.mjs --change <kind> --risk <risk>` before the
next validation loop and report the narrow/scoped/broad ladder. Treat repeated
broad gates without a changed risk or failed previous gate as validation waste.
