# Codex Agent Catalog

This folder is Codex's project-scoped custom agent catalog. Codex loads
standalone TOML files in `.codex/agents/` as custom subagent definitions.

Each custom agent file must define:

- `name`
- `description`
- `developer_instructions`

Optional settings such as `model`, `model_reasoning_effort`, and `sandbox_mode`
use the same configuration keys as a normal Codex session. Omitted optional
settings inherit from the parent session.

## Use

Before delegation:

1. Read this catalog.
2. Pick the closest existing role.
3. Write a bounded packet for that role.
4. Create a new role only when no existing role fits.

Packet shape:

```text
Agent: <agent name>
Task: <what to find or do>
Scope: <where to look or write>
Return: <short result shape>
Stop: <when to stop>
```

## Roles

- `deep-reasoner.toml`: reasoning-heavy architecture, debugging, algorithms,
  research synthesis, and trade-off analysis.
- `fast-worker.toml`: mechanical implementation, tests, formatting, simple
  edits, repetitive refactors, and decided plans.
