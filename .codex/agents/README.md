# Codex Agent Catalog

This folder is Codex's project-scoped custom agent catalog. Codex automatically
loads standalone TOML files in `.codex/agents/` as custom subagent definitions.
Global subagent limits stay under `[agents]` in `.codex/config.toml`.

Host, repository-validator, and process-convention boundaries are classified
in `ai_studio/core_harness/workflow/enforcement_contract.json`; this catalog
declares roles but does not prove which role/model the host selected.

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

- `deep-reasoner.toml` (`gpt-5.6-terra`, high, read-only): architecture,
  complex debugging, algorithms, synthesis, trade-offs, and independent
  evidence.
- `coding-worker.toml` (`gpt-5.6-terra`, high): bounded feature implementation,
  non-trivial bug fixes, refactors, and test-backed changes after scope is
  settled.
- `researcher.toml` (`gpt-5.6-luna`, high, read-only): codebase mapping,
  primary-source research, reference discovery, and concise evidence packets.
- `fast-worker.toml` (`gpt-5.6-luna`, low): obvious mechanical edits,
  repetitive changes, formatting, and already-defined tests.

The lead stays on `gpt-5.6-sol` and owns task decomposition, decisions,
integration, and final review. Route analysis to `deep-reasoner`, non-trivial
implementation to `coding-worker`, parallel evidence gathering to `researcher`,
and obvious mechanics to `fast-worker`.
