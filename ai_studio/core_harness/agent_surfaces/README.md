# Agent Surfaces

This Core Harness module owns generated agent-facing compatibility surfaces.

It does not define skill behavior or profiling semantics. It only renders the
same canonical sources into the file shapes expected by different agent CLIs.

## Source Of Truth

- `.codex/skills/*/SKILL.md`: canonical skill instructions.
- `hooks_sync.mjs`: canonical hook event/matcher/recorder source.

## Generated Outputs

- `.claude/skills/*/SKILL.md`: generated thin pointers to canonical Codex
  skills.
- `.codex/hooks.json`: generated Codex hook config.
- `.claude/settings.json`: generated Claude hook config; Claude-only keys are
  preserved.
- `.claude/settings.local.json`: never generated and never committed.

## Commands

Public facade:

```powershell
node ai_studio/core_harness/agent_surfaces/sync.mjs
node ai_studio/core_harness/agent_surfaces/sync.mjs --check
```

Internal generator checks:

```powershell
node ai_studio/core_harness/agent_surfaces/skills_sync.mjs --check
node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs --check
```

## Boundary

This module does not own:

- skill content;
- hook recorder implementation;
- task state;
- profiling analysis;
- agent runtime permissions.

Those belong to their owning modules. Agent Surfaces only prevents generated
Codex/Claude compatibility files from drifting.
