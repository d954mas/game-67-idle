---
name: chat-session-reflection
description: Use when reflecting on a long AI-assisted work session, chat history, agent run, or multi-turn project iteration to identify bottlenecks, mistakes, wasted time, weak tool use, context loss, planning gaps, quality risks, and concrete process improvements. Triggers include requests for retrospective, reflection, postmortem, "where did you waste time", "where did you get stuck", pipeline improvement, or improving future 24+ hour AI development sessions.
---

# Chat Session Reflection

Produce a blunt retrospective, not a progress report. The goal is to identify
where the AI agent got stuck and what should change next time.

## Inputs

Use durable evidence before memory:

1. `AGENTS.md`, `AI_PIPELINE.md`, `tasks/README.md`.
2. `node tools/taskboard/cli.mjs summary`; use
   `node tools/taskboard/cli.mjs context` only if the summary is not enough.
3. Relevant task logs, reports, screenshots, package logs, or validation
   outputs.
4. Passive profiling telemetry in `tmp/session_profiles/` when present.
5. Conversation context only after checking durable state.

Avoid high-cost context reads. Treat full `tasks/STATUS.md`, old task logs,
generated profile reviews, and archived material as optional evidence.
If the reflection suggests external AI observability/eval tooling, consult
`AI_PIPELINE_OBSERVABILITY_TOOLS.md` and `observability_gate.mjs` first.

## Workflow

1. State scope: session, objective, and evidence inspected.
2. Summarize factual progress briefly.
3. Identify the largest bottlenecks. For each, separate `symptom`, `cause`,
   and `faster path`.
4. Audit tool use: terminal, search, tests, generation, browser/runtime tools,
   and automation.
5. Audit context management: large reads, stale source-of-truth, rediscovered
   decisions, compaction risk.
6. Audit planning: broad scope, missing vertical slice, weak done criteria,
   late validation, over-validation.
7. Audit product quality separately from pipeline quality.
8. End with the highest-leverage process changes.

## Profiling

Profiling is passive by default. For normal retrospectives, start with:

```powershell
node tools/ai.mjs status
```

Use it to name unresolved failures, slowest recorded work, largest context input,
and long manual/research/review gaps. Do not repair stale bundles,
packets, drafts, reviews, follow-ups, or baselines unless the user explicitly
asks for a deep AI-workflow retrospective.

Deep mode is opt-in:

```powershell
node tools/ai.mjs reflect --deep
node tools/ai.mjs status --verbose
```

Use low-level `tools/ai_profile/*` only when debugging profiling itself or when
deep mode says exactly which artifact is needed.

## Output Rules

- Be specific and self-critical.
- Use concrete examples from files, reports, screenshots, tasks, commands, or
  user corrections.
- Mark missing evidence as likely or unknown.
- Distinguish product problems from pipeline problems.
- Prefer small rule/tool changes over vague advice.
- Do not mark the project goal complete from a retrospective.
- Keep raw telemetry and generated reflection artifacts in
  `tmp/session_profiles/` unless the lead asks to preserve them.

## Durable Capture

If the reflection yields reusable process lessons, add a short entry to
`AI_PIPELINE_ITERATION_LOG.md`. If it yields actionable project work, create or
update tasks in `tasks/`. Do not bury work only inside the retrospective.
