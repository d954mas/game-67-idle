# Agent Surfaces

This Core Harness module owns generated agent-facing compatibility surfaces.

Canonical sources stay small and explicit:

- `.codex/skills/*` is the source for generated `.claude/skills/*` pointers.
- `hooks_sync.mjs` is the source for `.codex/hooks.json` and the `hooks` block
  in `.claude/settings.json`.

## Commands

```powershell
node ai_studio/core_harness/agent_surfaces/sync.mjs
node ai_studio/core_harness/agent_surfaces/sync.mjs --check
```

Use direct commands only when working on one generator:

```powershell
node ai_studio/core_harness/agent_surfaces/skills_sync.mjs --check
node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs --check
```

This module does not own skill content, hook recorder implementation, task
state, or profiling analysis. It only keeps generated agent surfaces in sync.
