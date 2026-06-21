# Agent Workflow

Load when changing context policy, work-loop, multi-agent use, or hot docs.

## Roles

- Lead: direction, priority, acceptance.
- Agent: scopes, researches refs when needed, changes small slices, proves work.

## Agent-Facing Docs

- `AGENTS.md`: project map, hard rules, validation defaults.
- `AI_PIPELINE.md`: reusable workflow map and context policy.
- `tasks/README.md`: task store commands and lifecycle.
- `.codex/skills/*/SKILL.md`: one focused procedure per task type.
- Skill `references/` and `gamedesign/knowledge/`: deeper method on need.

Do not duplicate rules. Prefer decision rule, shortest command, source path,
stop condition, and no anecdotes.

## Context Policy

Default substantial-work context: `AGENTS.md`,
`node tools/taskboard/cli.mjs context`, one task/evidence file, one skill.

Prefer scoped search and compact output over whole-file dumps. Use archives,
logs, generated artifacts, and broad design only when task-linked or requested.

Put volatile facts in tasks, status, evidence, or final reports.

## Work Loop

1. Interpret the user request into one working scope.
2. Select or create a task only when durable tracking is useful.
3. Read only files needed for the selected scope.
4. Make the smallest coherent change.
5. Run the narrowest validation that proves the change.
6. Record evidence in the task/status/final response when the work changes
   project state.

Profiling is passive. Review with `node tools/ai.mjs status`.

## Subagents

Use subagents for independent research, disjoint edits, generation, review, or
verification. The lead owns task/status, hot files, validation, and commits.

Operator path:

1. Load context, create/select one task, and preflight it.
2. Build each prompt from `subagent-packet-template`; check file or here-string
   packets before launch.
3. Spawn bounded independent packets; keep the immediate blocker local and work
   on non-overlapping lead tasks while agents run.
4. Wait for needed results, verify current files, integrate findings, close
   completed agents, and record changes.
5. Check workflow manifest, strict evidence, reviewer PASS, validation, commit.

Load `docs/ai-pipeline/subagent-protocol.md` for packet schema, ownership,
handoff fields, review rules, and context-budget decisions.
