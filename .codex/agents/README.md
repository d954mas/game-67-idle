# Codex Agent Catalog

This folder is Codex's local catalog of reusable subagent roles. The lead agent
loads this catalog before delegating non-trivial work and chooses an existing
role instead of inventing a new one.

The Markdown frontmatter is repo-local metadata for the lead agent. Operational
keys map to `spawn_agent` defaults:

- `agent_type`: the Codex subagent type to request, such as `default`,
  `explorer`, or `worker`.
- `reasoning_effort`: optional effort override, such as `low`, `medium`,
  `high`, or `xhigh`.

Do not add `model` by default. Codex subagents should inherit the parent model
unless the lead explicitly accepts a model override for a specific packet.

## Use

Before delegation:

1. Read this catalog.
2. Pick the closest existing role.
3. Write a bounded packet for that role.
4. Create a new role only when no existing role fits.

Packet shape:

```text
Agent: <role file>
Task: <what to find or do>
Scope: <where to look or write>
Return: <short result shape>
Stop: <when to stop>
```

## Roles

- `deep-reasoner.md`: reasoning-heavy architecture, debugging, algorithms,
  research synthesis, and trade-off analysis.
- `fast-worker.md`: mechanical implementation, tests, formatting, simple
  edits, repetitive refactors, and decided plans.
