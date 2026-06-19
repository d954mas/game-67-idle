# Agent Workflow Reference

Detailed portable workflow rules for agent behavior. Load this file when
changing agent context policy, work-loop behavior, multi-agent usage, or hot
Markdown structure.

## Roles

- Lead: sets direction, taste, priority, and acceptance. The lead does not need
  to write detailed specs.
- Agent: turns intent into scoped work, asks only necessary questions, researches
  when references matter, implements small slices, and proves results with
  evidence.

## Agent-Facing Docs

Keep hot Markdown short and stable.

- `AGENTS.md`: project map, hard rules, validation defaults.
- `AI_PIPELINE.md`: reusable workflow map and context policy.
- `tasks/README.md`: task store commands and lifecycle.
- `.codex/skills/*/SKILL.md`: one focused procedure per task type.
- Skill `references/` and `gamedesign/knowledge/`: detailed method, examples,
  research, and historical lessons loaded only when needed.

Do not duplicate the same rule in every file. Put each rule where the agent
first needs it, then link to the deeper source. Delete stale anecdotes once the
lesson is encoded as a tool, validator, task rule, or skill.

Write agent Markdown as:

- decision rule first;
- shortest required command;
- source-of-truth path;
- stop condition;
- no long chat history, no broad checklists in hot context.

## Context Policy

Default context for substantial work is:

1. `AGENTS.md`
2. `node tools/taskboard/cli.mjs context`
3. one relevant task or evidence file
4. one matching skill

Prefer scoped search and compact command output over whole-file dumps. Use
archives, old task logs, generated artifacts, and broad design folders only when
the current task links to them or the user asks for review/history.

Keep stable context byte-stable inside a session when possible. Put volatile
session facts in tasks, status, evidence files, or final reports rather than
rewriting hot instruction files repeatedly.

## Work Loop

1. Interpret the user request into one working scope.
2. Select or create a task only when durable tracking is useful.
3. For non-trivial work, start/passively record profiling scope or state why it
   is unavailable.
4. Read only files needed for the selected scope.
5. Make the smallest coherent change.
6. Run the narrowest validation that proves the change.
7. Record evidence in the task/status/final response when the work changes
   project state.

Use multiple agents only for independent side work with clear ownership,
expected artifact, evidence, and out-of-scope boundaries. The main integrator
keeps final responsibility for merge, validation, and status.
