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
tool-use guard:
expected output:
evidence command or artifact:
stop condition:
```

Use `tool-use guard` to prevent known subagent command mistakes: give exact
existing paths or a discovery command before reads, use `Select-Object -Skip`
and `-First` for line windows, and include a trace evidence source plus
`--json-output` for orchestration checks.

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
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace source plus --json-output
  expected output: <non-empty>
  evidence command: <non-empty>
  stop condition: <non-empty>
  independent reviewer: <non-empty>
```

`tool-use guard` is template-default but validator-optional for older packets.

For genuinely small work:

```text
- orchestration: not needed - small scope: one-file/docs-only/no code ...
```

The taskboard guard is label-based. It proves that orchestration was considered
and recorded; it does not parse app internals by itself.

For transcript/session evidence, add an evidence command with the focused trace
tool:

```text
node tools/ai.mjs orchestration-trace --session <codex-session.jsonl> --json-output <trace.json>
node tools/ai.mjs orchestration-trace --parent-thread-id <id> --session-root <dir> --min-agents <n> --json-output <trace.json>
```

For newer substantial pipeline/orchestration tasks, taskboard validation expects
the packet's `evidence command` to include either `node tools/ai.mjs
orchestration-trace ...` with `--session`/`--parent-thread-id` and
`--json-output`, or
strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok ...`.
Before moving the task to review/done, record a later `- evidence: PASS ...`
log entry for that approved machine evidence command. The validator checks the
command shape and recorded PASS line; it does not execute commands copied from
task logs.

Strict status rollup defaults to `--min-agents 1`; raise it when the packet
expects several independent agents.

Use transcript mode when the parent session exposes spawn/wait/close tool calls;
it verifies call order and completed wait/close outputs. Use
`parent-thread-id` mode when subagent `session_meta` files are the available
evidence. Status rollup can also show best-effort subagent command telemetry
from profile logs or subagent transcripts, but the lead still verifies the
traced agents, final messages, and artifacts match the task scope before
closeout.

Run `node tools/taskboard/cli.mjs validate` before closeout. If the packet is
missing or malformed, the CLI names the missing/invalid field labels and prints
a compact copyable packet template. Use
`node tools/taskboard/cli.mjs orchestration-template` for the accepted block,
`node tools/taskboard/cli.mjs orchestration-check <task-id>` before launching
subagents, and `node tools/taskboard/cli.mjs validate --json` when
another tool or agent needs structured `missingFields` output.

## Context Budgets

Context budgets are pressure signals, not taste. Do not skip required evidence
or files to satisfy a cap. When correctness and budget conflict, record the
reason and choose deliberately:

1. compress if meaning stays clear;
2. move detail from hot docs to a cold reference;
3. raise the cap only with a documented reason and validation.
