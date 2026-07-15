# Workflow Orchestration

Orchestration is optional. Keep coherent work with the lead and delegate only
when an independent bounded packet materially reduces latency, context load, or
review risk after packet writing, context transfer, and reintegration cost.

Before creating a delegation packet, load the active harness's agent catalog
and select the closest existing role. Codex uses `.codex/agents/*.toml`; Claude
uses `.claude/agents/*.md`. Do not invent ad hoc agent types while a catalog
role fits.

## Delegate When

Delegate when all of these are true:

- the packet is independent and bounded;
- its result can be compressed and integrated;
- the work is parallel-safe or deliberately independent review;
- the expected latency, context, or risk gain exceeds coordination cost.

Good cases are large independent research or codebase mapping, disjoint parallel
implementation, and adversarial review required by the risk tier.

Stay direct for coherent related multi-file implementation, tightly coupled
writes, work that already fits the lead context, unclear scope requiring lead
decisions, or any packet whose explanation and reintegration cost as much as
doing the work.

## Packet

Use a short packet:

```text
Agent: <role from the active harness catalog>
Task: <what to find or do>
Scope: <where to look or the area to inspect>
Return: <short result shape>
Stop: <when to stop>
```

When delegation is for review or verification, make the packet adversarial by
default: assume the proposed change is wrong, find concrete bugs, regressions,
missing validation, and contract drift; do not implement fixes unless explicitly
asked.

Example:

```text
Agent: deep-reasoner
Task: Map how Taskboard validation currently works.
Scope: ai_studio/taskboard/cli.mjs, ai_studio/taskboard/lib.mjs, ai_studio/taskboard/tests/
Return: commands, data flow, risks, and suggested split. Max 20 bullets.
Stop: after mapping validation flow; do not edit files.
```

Subagents start with fresh context. Restate the important local fact in the
packet instead of assuming they saw the lead conversation.

## Review Budget

- Mechanical documentation, moves, formatting, and obvious edits need no
  independent reviewer.
- Normal behavior or logic gets one independent reviewer.
- Security, concurrency, and release work gets two independent reviewers.
- Repeat review only after a high-risk finding or contract change.

Independent reviewers receive the current files and evidence, not each other's
conclusions. Deterministic validation remains required regardless of reviewer
count.

## Lead Agent Rules

- Keep at most three subagents active at once; use a later wave when more
  independent packets exist.
- Subagents return compressed findings, not transcripts.
- The lead verifies current files before trusting results.
- The lead integrates changes, runs validation, and owns the final answer.
- When a durable task exists, the lead records delegated packet results,
  verification, and integration using the task-log format from
  `../README.md`.
- Writes stay serial unless files are clearly disjoint.
- Reuse a fitting existing agent before creating another one.
- Prefer event-driven completion or one long wait; do not tight-loop poll.
- An external wait over ten minutes becomes a checkpoint: record the external
  identifier and resume by bounded polling instead of holding an approval or
  tool call open indefinitely.
- Do not create recursive subagent trees.

## Approval

If host policy requires explicit permission for delegation, ask once on the
first useful delegation and reuse that approval within the same
chat/session/repository scope. Do not add per-task approval ceremony.

## Boundary

This file defines delegation policy only. It does not own Taskboard state,
validation gates, task lifecycle, or browser surfaces.

Choosing the requested role is a process convention. The host loads and applies
the provider-owned role/model settings declared by the repository. After a Codex
harness restart, request a named stock role, locate its native rollout JSONL,
and verify its recorded role and actual model:

```powershell
node ai_studio/core_harness/validation/agent_role_smoke.mjs --evidence <rollout.jsonl> --requested-role fast-worker
```

The verifier reads the expected model from the stock Codex catalog and fails on
missing evidence, mismatch, or generic fallback. Profiling is advisory and is
not a substitute for this smoke.
