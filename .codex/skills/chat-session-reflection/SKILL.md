---
name: chat-session-reflection
description: Use when reflecting on a long AI-assisted work session, chat history, agent run, or multi-turn project iteration to identify bottlenecks, mistakes, wasted time, weak tool use, context loss, planning gaps, quality risks, profiler/telemetry gaps, and concrete process improvements. Triggers include requests for retrospective, reflection, postmortem, session analysis, AI workflow review, profiler review, analytics review, "where did you waste time", "where did you get stuck", pipeline improvement, or improving future 24+ hour AI development sessions.
---

# Chat Session Reflection

Produce a blunt retrospective, not a progress report. The goal is to identify
where the AI agent got stuck, what evidence supports that claim, and what should
change next time.

## Inputs

Use durable evidence before memory:

1. `AGENTS.md`, `AI_PIPELINE.md`, `tasks/README.md`.
2. `node tools/taskboard/cli.mjs summary`; use
   `node tools/taskboard/cli.mjs context` only if the summary is not enough.
3. `git status --short` to identify changed durable artifacts without implying
   they were all part of the reflected session.
4. Relevant task logs, reports, screenshots, package logs, or validation
   outputs.
5. Passive profiling telemetry in `tmp/session_profiles/` when present.
6. Conversation context only after checking durable state.

Avoid high-cost context reads. Treat full `tasks/STATUS.md`, old task logs,
generated profile reviews, and archived material as optional evidence.
If the reflection suggests external AI observability/eval tooling, consult the
External AI Observability Decision Criteria in `AI_PIPELINE_HISTORY.md` first.
Do not infer time spent from memory when profiler coverage is broken; mark those
intervals `unknown` or `likely`.

## Workflow

1. State scope: session, objective, and evidence inspected.
2. Report evidence quality: profile file, record count, review confidence,
   unresolved failures, wall-clock coverage, and missing context inputs.
3. Summarize factual progress briefly.
4. Identify the largest bottlenecks. For each, separate `symptom`, `cause`,
   and `faster path`.
5. Audit tool use: terminal, search, tests, generation, browser/runtime tools,
   and automation.
6. Audit context management: large reads, stale source-of-truth, rediscovered
   decisions, compaction risk.
7. Audit planning: broad scope, missing vertical slice, weak done criteria,
   late validation, over-validation.
8. Audit product quality separately from pipeline quality and agent behavior.
9. End with the highest-leverage process changes and the top 10 fixes for the
   next cycle when the user asks for a deep review.

## Profiling

Profiling is passive by default. For normal retrospectives, start with:

```powershell
node tools/ai.mjs status
```

Use it to name unresolved failures, slowest recorded work, largest context input,
long manual/research/review gaps, and whether normal work needs any profile
action.

For AI workflow, profiler, analytics, or requested retrospective reviews, run:

```powershell
node tools/ai.mjs status --verbose
node tools/ai.mjs status --require-current-scope-usable
```

If the guard fails, do not repair stale artifacts, stale bundles, drafts,
reviews, follow-ups, or baselines by default. Say what is missing, use available
evidence, and mark time-spend claims as partial or unknown. For long Codex
sessions with suspected missing failures, run
`node tools/ai.mjs import-codex-session` before trusting the status. Use
`node tools/ai.mjs reflect` for a short closeout artifact; add
`--gap-checkpoint` only when the user explicitly wants the pre-reflection gap
recorded. The old `reflect --deep` chain is retired.

Use low-level `tools/ai_profile/*` only when debugging profiling itself or when
`tools/ai.mjs status --verbose` identifies a specific artifact that must be
inspected.

## Output Shape

For deep retrospectives, cover these sections in order:

1. What was done.
2. Where the most time went.
3. Where the agent got stuck or behaved poorly.
4. Tool and automation audit.
5. Context problems.
6. Planning problems.
7. Product/result quality problems.
8. Improved workflow for the next cycle.
9. Prompt/system-rule changes.
10. Top 10 improvements.

For each major problem, separate `symptom`, `cause`, and `fix/faster path`.
Do not invent concrete examples; if evidence is missing, say so.

## Output Rules

- Be specific and self-critical.
- Use concrete examples from files, reports, screenshots, tasks, commands, or
  user corrections.
- Clearly label weak evidence, broken profiler coverage, and unknown time
  intervals.
- Mark missing evidence as likely or unknown.
- Distinguish product problems, pipeline problems, and agent behavior problems.
- Prefer small rule/tool changes over vague advice.
- Do not mark the project goal complete from a retrospective.
- Keep raw telemetry and generated reflection artifacts in
  `tmp/session_profiles/` unless the lead asks to preserve them.

## Durable Capture

If the reflection yields reusable process lessons, add a short dated entry to
`AI_PIPELINE_HISTORY.md`. If it yields actionable project work, create or
update tasks in `tasks/`. Do not bury work only inside the retrospective.
