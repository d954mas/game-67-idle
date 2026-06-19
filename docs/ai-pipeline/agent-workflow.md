# Agent Workflow Reference

Portable agent rules. Load when changing context policy, work-loop,
multi-agent use, or hot docs.

## Roles

- Lead: sets direction, taste, priority, and acceptance.
- Agent: scopes intent, asks only necessary questions, researches when refs
  matter, implements small slices, and proves results.

## Agent-Facing Docs

- `AGENTS.md`: project map, hard rules, validation defaults.
- `AI_PIPELINE.md`: reusable workflow map and context policy.
- `tasks/README.md`: task store commands and lifecycle.
- `.codex/skills/*/SKILL.md`: one focused procedure per task type.
- Skill `references/` and `gamedesign/knowledge/`: deeper method loaded on need.

Do not duplicate rules. Put each rule where first needed, link deeper source,
then delete anecdotes once encoded as tool, validator, task rule, or skill.

Write agent Markdown as:

- decision rule first;
- shortest required command;
- source-of-truth path;
- stop condition;
- no chat history or broad checklists in hot context.

## Context Policy

Default substantial-work context:

1. `AGENTS.md`
2. `node tools/taskboard/cli.mjs context`
3. one relevant task or evidence file
4. one matching skill

Prefer scoped search and compact output over whole-file dumps. Use archives,
logs, generated artifacts, and broad design only when task-linked or requested.

Keep stable context byte-stable in-session. Put volatile facts in tasks, status,
evidence files, or final reports, not hot instruction files.

## Work Loop

1. Interpret the user request into one working scope.
2. Select or create a task only when durable tracking is useful.
3. Read only files needed for the selected scope.
4. Make the smallest coherent change.
5. Run the narrowest validation that proves the change.
6. Record evidence in the task/status/final response when the work changes
   project state.

Profiling needs no work-loop step: the PostToolUse hook records every tool call
automatically. Review a session with `node tools/ai.mjs status`.

Use multiple agents only for independent side work with clear owner, artifact,
evidence, and boundary. The main integrator owns merge, validation, and status.
