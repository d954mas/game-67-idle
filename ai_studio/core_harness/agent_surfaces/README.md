# Agent Surfaces

This Core Harness module owns generated agent-facing compatibility surfaces.

It does not define skill behavior or profiling semantics. It only renders the
same discoverable entrypoints and hook sources into the file shapes expected by
different agent CLIs.

## Source Of Truth

- `.codex/skills/*/SKILL.md`: Codex-discoverable skill entrypoints. Small
  skills may keep canonical content there; reviewed module-owned skills may
  point to canonical instructions inside their owning `ai_studio/` module.
- `hooks_sync.mjs`: canonical hook event/matcher source for passive profiling
  and workspace-owned shell guards such as the private game Git guard.

## Generated Outputs

- `.claude/skills/*/SKILL.md`: generated thin pointers to Codex-discoverable
  skill entrypoints.
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
- workspace guard implementation;
- task state;
- profiling analysis;
- agent runtime permissions.

Those belong to their owning modules. Agent Surfaces only prevents generated
Codex/Claude compatibility files from drifting.
