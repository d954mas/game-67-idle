---
name: chat-session-reflection
description: Use when reflecting on a long AI-assisted work session, chat history, agent run, or multi-turn project iteration to identify bottlenecks, mistakes, wasted time, weak tool use, context loss, planning gaps, quality risks, profiler/telemetry gaps, and concrete process improvements. Triggers include requests for retrospective, reflection, postmortem, session analysis, AI workflow review, profiler review, analytics review, "where did you waste time", "where did you get stuck", pipeline improvement, or improving future 24+ hour AI development sessions.
---

# Chat Session Reflection

Produce a blunt retrospective, not a progress report. The goal is to identify
where the AI agent got stuck, what durable evidence supports that claim, and
what should change next time.

## Load Only What Applies

- `references/reflection-evidence-intake.md`: durable evidence order,
  `AGENTS.md`, `AI_PIPELINE.md`, `tasks/STATUS.md`,
  `tools/taskboard/cli.mjs summary`, `tools/taskboard/cli.mjs context`,
  `git status --short`, high-cost context rules, and evidence quality fields.
- `references/reflection-profiling.md`: passive profiling telemetry in
  `tmp/session_profiles`, `tools/ai.mjs`, `node tools/ai.mjs status`,
  `status --verbose`, `status --require-current-scope-usable`,
  `import-codex-session`, `reflect`, and why `reflect --deep` is retired.
- `references/reflection-output-rules.md`: deep retrospective shape,
  `symptom` / `cause` / `faster path`, context management, planning, product
  quality, agent behavior, Top 10 improvements, durable capture in
  `AI_PIPELINE_HISTORY.md`, and `tasks/`.

## Default Workflow

1. State scope: session, objective, and evidence inspected.
2. Report evidence quality: profile file, record count, review confidence,
   unresolved failures, wall-clock coverage, and missing context inputs.
3. Summarize factual progress briefly.
4. Identify largest bottlenecks with `symptom`, `cause`, and `faster path`.
5. Audit tool use, context management, planning, product quality, and agent
   behavior separately.
6. End with highest-leverage process changes; include Top 10 improvements for a
   requested deep review.

## Non-Negotiables

- Use durable evidence before memory or chat recollection.
- Do not infer time spent from memory when profiler coverage is broken; mark it
  partial, likely, or unknown.
- Do not invent concrete examples; if evidence is missing, say so.
- Distinguish product problems, pipeline problems, and agent behavior problems.
- Do not mark the project goal complete from a retrospective.
- Keep raw telemetry and generated reflection artifacts in `tmp/session_profiles`
  unless the lead asks to preserve them.
