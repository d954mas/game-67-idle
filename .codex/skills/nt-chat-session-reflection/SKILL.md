---
name: nt-chat-session-reflection
description: "Use when the user asks for a full retrospective or analysis of an AI-assisted work session, chat history, agent run, long task, profiling data, slow progress, repeated failures, weak tool/subagent use, context loss, planning gaps, quality risks, or process improvements. Produces one comprehensive evidence-based session review with bottlenecks, causes, better workflow, and concrete changes for future runs."
---

# NT Chat Session Reflection

Produce a full retrospective of an AI work session. The goal is to understand
what happened, where time or context was wasted, why quality suffered, and what
should change in the harness, workflow, tools, or agent behavior.

This is not a normal status report. Be direct, evidence-based, and practical.

## Evidence Order

Use durable evidence before memory or chat recollection:

1. Profiling:
   `node ai_studio/core_harness/profiling/status.mjs --verbose`
2. If Codex failures look missing:
   `node ai_studio/core_harness/profiling/hook_record.mjs codex --recover-only`,
   then rerun status.
3. Task state when relevant:
   `node ai_studio/taskboard/cli.mjs summary --json`
4. Changed durable artifacts:
   `git status --short`
5. Relevant task logs, validation output, reports, screenshots, or generated
   artifacts.
6. Conversation context last.

Avoid broad reads. Use `ai_studio/taskboard/cli.mjs context --json` only when
the task summary is not enough.

## Review Shape

Write the review in this order:

1. Scope and evidence inspected.
2. Evidence quality: profile file, record count, unresolved/recovered failures,
   wall-clock coverage, largest gaps, slowest work, repeated commands.
3. What was actually accomplished.
4. Where time/context was wasted.
5. Major bottlenecks and problems. For each major problem, use:
   `symptom`, `cause`, `faster path`.
6. Tool/subagent use audit.
7. Context management audit.
8. Planning and validation audit.
9. Product/result quality audit.
10. Better workflow for the next similar session.
11. Top 10 concrete improvements, ordered by leverage.

When useful, split problems into product, pipeline, and agent behavior.

## Durable Changes

Do not edit files by default. If the user asks to preserve lessons, update the
owning `ai_studio/` module README or `ai_studio/game_design/knowledge_base/knowledge/log.md`,
depending on whether the lesson is workflow/tooling or reusable game-development
knowledge. If the reflection creates project work, create or update taskboard
tasks instead of burying work only in the retrospective.

## Non-Negotiables

- Do not invent concrete examples.
- Mark weak evidence, broken profiling coverage, stale artifacts, and unknown
  intervals clearly.
- Do not infer time spent from memory when profiler coverage is broken; mark it
  `partial`, `likely`, or `unknown`.
- Distinguish product, pipeline, and agent behavior problems.
- Do not mark the project goal complete from a retrospective.
- Keep raw telemetry and generated reflection artifacts in `tmp/session_profiles`
  unless the lead asks to preserve them.
