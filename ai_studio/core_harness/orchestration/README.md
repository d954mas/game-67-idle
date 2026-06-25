# Core Harness Orchestration

Orchestration exists because broad read-heavy work was staying in the lead
agent too long, making the session slow and noisy.

It is not a proof layer and not a compliance form. It is an early split rule.

## Trigger

Before broad reading, consider delegation when the task needs:

- mapping more than one small module;
- reading roughly 8-10+ files;
- source/license/research work;
- independent review;
- asset search or generation;
- several disjoint branches that can be summarized separately.

Stay single-agent for the immediate blocker, ambiguous scope, quick edits, or
coupled writes where state, UI, runtime, docs, and tests must agree together.
Also stay single-agent when step B depends on step A's full output.

## Default Packet

Use this simple packet for normal subagent work:

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

Do not add strict allowed/forbidden-file contracts by default. If a task is
risky enough to need that, handle it as a separate explicit design decision.

Subagents start with fresh context. Restate the important local fact in the
packet instead of assuming they saw the lead conversation.

## Lead Responsibility

Subagents return compressed findings, not transcripts. The lead verifies current files,
integrates the result, runs the right validation, updates task/status when
needed, and owns the final report.

Acceptance gates the delivered work, not the fact that delegation happened.
Do not add proof-of-delegation checks, mandatory reviewer blocks, or task-log
ceremony.

Fan out reads freely; keep writes serial unless the files are clearly disjoint.
Use 2-4 workers for normal fan-out. Do not create recursive subagent trees.

## Current Tooling

The old prototype commands still live in `ai_studio/taskboard/cli.mjs`:

- `subagent-packet-template`
- `subagent-packet-check`
- `orchestration-check`
- `orchestration-bootstrap`

They are compatibility tooling for now. Later, useful orchestration commands
should move here, and Taskboard should remain task state/storage only.

## Removed

The old long playbook was deleted. Its useful rules are now in this README.
