# Subagent Protocol

Detailed orchestration rules for substantial AI-pipeline, game, visual, asset,
or review work. `docs/ai-pipeline/agent-workflow.md` is the hot route; this file
holds the longer method.

## Lead Role

The main agent is the orchestrator/integrator. It owns scope, task/status
changes, hot policy files, integration, final validation, commits, and the final
report.

Use subagents for independent, bounded packets that can run without blocking the
lead's immediate next step:

- reference or source research;
- codebase exploration;
- disjoint code/docs work;
- asset/art generation or intake checks;
- independent review or verification.

Do not delegate simple linear edits, the immediate blocker, ambiguous scope, or
overlapping writes.

## Packet

Every subagent gets a packet:

```text
objective:
allowed files or inputs:
forbidden files:
expected output:
evidence command or artifact:
stop condition:
```

If the packet touches repo state, include the current project boundary from
`AGENTS.md`, such as active concept, closed prototype status, engine policy, and
whether game work is in scope.

## Ownership

- Subagents do not edit hot files (`AGENTS.md`, `AI_PIPELINE.md`,
  `tasks/STATUS.md`, `tasks/README.md`, `.codex/skills/*/SKILL.md`). They may
  inspect or propose a patch; the lead applies, integrates, and validates.
- Parallel writes need disjoint files/modules. Never assign the same task file,
  runtime module, generated pack, or hot doc to multiple agents.
- Workers report draft changes; the lead integrates and validates.

## Handoff

Handoffs use scan-friendly fields:

```text
findings:
files:
commands/evidence:
risks:
owner action:
not-done:
```

The lead must verify current files before copying a subagent claim into
task/status/docs. If subagents conflict, the lead decides the source of truth
and records uncertainty instead of merging prose.

## Review

Use an independent reviewer/verifier for substantial code, pipeline,
product/visual, or asset changes. A reviewer should check evidence, scope
creep, missing tests, context-budget risk, and contradictions with current
policy.

## Task Closeout Guard

Substantial pipeline/orchestration tasks cannot move to `review` or `done`
without one of these `## Log` markers:

```text
- orchestration: used
  objective: <non-empty>
  allowed files: <non-empty>
  expected output: <non-empty>
  evidence command: <non-empty>
  stop condition: <non-empty>
  independent reviewer: <non-empty>
```

For genuinely small work:

```text
- orchestration: not needed - small scope: one-file/docs-only/no code ...
```

The guard is label-based for now. It proves that orchestration was considered
and recorded; it does not inspect transcripts yet.

## Context Budgets

Context budgets are pressure signals, not taste. Do not skip required evidence
or files to satisfy a cap. When correctness and budget conflict, record the
reason and choose deliberately:

1. compress if meaning stays clear;
2. move detail from hot docs to a cold reference;
3. raise the cap only with a documented reason and validation.
