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
4. Conversation context only after checking durable state.

If evidence is missing, label the claim as likely or unknown instead of making
it sound proven.

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
5. Audit tool use. Cover terminal/files/search/tests/generation/agents. Say
   where a tool was useful, late, missing, or wasted.
6. Audit context management. Name forgotten constraints, rediscovered
   decisions, stale source-of-truth files, and state that should have been
   pinned.
7. Audit planning. Name order mistakes, missing vertical slices, broad scopes,
   missing Definition of Done, and weak checkpoints.
8. Audit product quality. Review as a director: unclear gameplay, weak visuals,
   unreadable UI, brittle packaging, unproven manual acceptance, or technical
   success that still fails the product goal.
9. Propose a next-session workflow with concrete gates, not generic advice.
10. Propose prompt/system changes: added instructions, bans, checklists,
    readiness criteria, and role boundaries.
11. End with the 10 highest-leverage improvements.

## Output Rules

- Be specific and self-critical. Do not defend the agent.
- Use concrete examples from files, reports, screenshots, tasks, commands, or
  user corrections.
- Separate `symptom`, `cause`, and `fix` for each major issue.
- Distinguish product problems from pipeline problems.
- Prefer hard process changes over vague habits.
- Do not mark the project goal complete from a retrospective.

## Durable Capture

When the reflection yields reusable process lessons, add a short entry to
`AI_PIPELINE_ITERATION_LOG.md`. Keep the detailed retrospective in the chat or
a dedicated requested document; the iteration log is only for compact lessons.

If the reflection identifies actionable project work, create or update tasks in
`tasks/` instead of burying work inside the retrospective.
