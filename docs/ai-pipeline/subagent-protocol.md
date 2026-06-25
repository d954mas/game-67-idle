# Subagent Protocol

How the lead delegates bounded work to subagents in substantial AI-pipeline,
game, visual, asset, or review work. `docs/ai-pipeline/agent-workflow.md` is the
hot route; this file holds the method; day-to-day operator recipes are in
`docs/ai-pipeline/orchestration-playbook.md`.

This repo follows the model mature agent systems use (Anthropic multi-agent
research, Claude Code subagents, OpenAI Agents SDK, LangGraph): **delegation is
enabled and detected, never machine-proven. Acceptance gates the work product,
not the process.** The lead is the backstop.

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

Default to a single agent. Delegate only when a side task is genuinely
independent and at least one holds: it floods the lead's context, needs
tool/permission isolation, parallelizes cleanly into disjoint files, or routes to
a cheaper model. Do not delegate the immediate blocker, simple linear edits,
ambiguous scope, or overlapping writes. Multi-agent costs several times more
tokens; the common failure is over-delegation, not under-delegation.

## Packet

The parent->subagent prompt is the only channel that moves quality. Bound it:

```text
objective: <bounded subagent objective>
allowed files: <repo-local files or bounded patterns>
forbidden files: <files or areas the subagent must not touch>
project boundary: <active concept, engine policy, whether game work is in scope> (if it touches repo state)
tool-use guard: verify exact repo paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First for line windows; keep evidence commands read-only
expected output: <compact final report or changed files>
evidence command or artifact: <read-only command, focused test, or artifact path>
stop condition: <when the subagent must stop>
```

Optionally lint the packet shape before launch (advisory, not a gate):

```text
node ai_studio/taskboard/cli.mjs subagent-packet-template
node ai_studio/taskboard/cli.mjs subagent-packet-check --file packet.txt --json
```

For quick inline prompts, copy the template shape directly. `allowed files`
should be bounded repo-local paths or patterns (exact files, final-segment globs
such as `tasks/active/T*.md`, scoped recursive globs such as `ai_studio/taskboard/**`)
-- not absolute paths, `..`, broad root globs, or prose such as `all files`.

## Return / Handoff

Subagents return a compact, structured result the lead reads -- not a transcript.
Large output goes to a file with a pointer passed back.

```text
findings: <facts or verdict>
files: <files inspected or changed>
commands/evidence: <commands run and results, or artifact pointer>
risks: <remaining risk>
owner action: <what the lead must do next>
not-done: <explicit gaps>
```

The lead verifies current files before copying a subagent claim into
task/status/docs. If subagents conflict, the lead decides the source of truth and
records uncertainty instead of merging prose.

## Ownership

- Subagents do not edit hot files (`AGENTS.md`, `ai_studio/README.md`,
  `tasks/STATUS.md`, `ai_studio/taskboard/README.md`, `.codex/skills/*/SKILL.md`). They may
  inspect or propose a patch; the lead applies, integrates, and validates.
- Parallel writes need disjoint files/modules. Never assign the same task file,
  runtime module, generated pack, or hot doc to multiple agents.
- Workers report draft changes; the lead integrates and validates.

## Acceptance: gate the outcome, not the delegation

Acceptance is the work product, never a proof that a subagent ran:

- Routine change: the lead reads the result and `node tools/pipeline_validate.mjs` or
  focused tests pass.
- Product / visual / native-playable: product gate with screenshot evidence plus
  fake-shot judgment; lead accept/reject. Lead rejection freezes feature/content
  expansion until fixed.
- Risky or irreversible action: the lead approves the proposed change before it
  lands.

There is no machine evidence that delegation happened, no per-task evidence
artifact, no workflow manifest, and no required reviewer-PASS block.
Observability (the passive profiler, `node tools/ai_profile/status.mjs`) is advisory
diagnostics the lead inspects and mines for friction -- never an acceptance
condition.

## Start / closeout label guard

A substantial pipeline/orchestration task records one `- orchestration: used`
block in `## Log` with objective, allowed files (bounded), tool-use guard,
expected output, evidence command (a read-only command or artifact path), stop
condition, and independent reviewer. For genuinely small work, record instead:

```text
- orchestration: not needed - small scope: one-file/docs-only/no code ...
```

This is a lightweight label check so the lead has thought about scope and handoff
before substantial delegation -- not a proof gate.
`node ai_studio/taskboard/cli.mjs validate` checks the label is present and the
allowed-files are bounded; `node ai_studio/taskboard/cli.mjs orchestration-check <task-id>`
previews the packet. Both are advisory authoring help.

## Large workflows

For a genuinely multi-thread orchestration (many independent packets exceeding
one context window), prefer a scripted orchestration step over in-conversation
ceremony, and keep one human-readable plan (objective, scope in/out, planned
packets, agent/context budget, verification commands, integration owner) for
planning and resume. It is cold planning state -- never required for closeout,
never validated as proof. Most slices need none of this.
