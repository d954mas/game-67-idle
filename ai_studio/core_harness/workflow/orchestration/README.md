# Workflow Orchestration

Use orchestration by default for non-trivial work. The goal is to keep the lead
agent focused on scope, integration, and validation while workers do bounded
reading, research, review, generation, or isolated implementation.

## Delegate When

Delegate before broad reading when the task needs:

- mapping more than one module;
- reading more than a few files;
- source, license, or research lookup;
- independent review or verification;
- asset search or generation;
- parallel branches that can be summarized separately.

Stay single-agent only for trivial edits, immediate blockers, unclear scope that
must be clarified first, or coupled writes that must be kept in one mental
model.

## Packet

Use a short packet:

```text
Task: <what to find or do>
Scope: <where to look or the area to inspect>
Return: <short result shape>
Stop: <when to stop>
```

Example:

```text
Task: Map how Taskboard validation currently works.
Scope: ai_studio/taskboard/cli.mjs, ai_studio/taskboard/lib.mjs, ai_studio/taskboard/tests/
Return: commands, data flow, risks, and suggested split. Max 20 bullets.
Stop: after mapping validation flow; do not edit files.
```

Subagents start with fresh context. Restate the important local fact in the
packet instead of assuming they saw the lead conversation.

## Lead Agent Rules

- Subagents return compressed findings, not transcripts.
- The lead verifies current files before trusting results.
- The lead integrates changes, runs validation, and owns the final answer.
- Writes stay serial unless files are clearly disjoint.
- Use 2-4 workers for normal fan-out.
- Do not create recursive subagent trees.

## Boundary

This file defines delegation policy only. It does not own Taskboard state,
validation gates, task lifecycle, or browser surfaces.
