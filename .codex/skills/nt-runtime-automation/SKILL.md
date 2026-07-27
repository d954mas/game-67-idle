---
name: nt-runtime-automation
description: "Use when working with AI Studio runtime automation for a running game: DevAPI client/CLI, endpoints and command.describe discovery, frame.wait loops, ui.tree/ui.click, native iteration helpers, screenshots, recordings, pixel-health checks, UI readability evidence, or live-state capture matrices. Use for runtime proof collection and automation tooling, not for game design, task tracking, or general visual quality rules."
---

# NT Runtime Automation

Use this skill to collect runtime evidence or change the local runtime proof
helpers owned by `ai_studio/runtime_automation/`. The skill is the workflow
entrypoint; reusable Python clients, capture helpers, and proof tooling stay in
the reviewed AI Studio module.

## Load Only What Applies

- `references/runtime-workflow-rules.md`: launch/reuse rhythm, evidence paths,
  native-first proof, and boundaries for game-specific smoke scripts.
- `references/devapi-pattern.md`: DevAPI discovery, command metadata, UI/input,
  frame sync, and capture flow.
- `references/visual-qa-checklist.md`: only when the task asks for visible
  runtime evidence; quality acceptance rules live in `ai_studio/quality/`.

## Game Video Capture

For routine game video, start in the game root and use only:

```text
capture live
capture shot <id>
```

Read the game-owned `capture/catalog.json` before choosing an id. `live` runs
the catalog's approved continuous presentation without a deterministic event
timeline. `shot` resets and host-paces the referenced deterministic scenario
while the existing OBS recorder observes real game pixels and process audio.

Treat every completed take as a draft first. A shot becomes a true master only
when recorder validation, scenario completion, and the applicable universal
safe-area gate all pass. An incomplete safe-area policy is guidance only and
must keep the take in `draft/`.

Run the game's documented local audiovisual E2E after capture workflow changes.
Use `record_game.py` directly only when maintaining or diagnosing the recorder;
do not add a backend selector, job, queue, or alternate routine capture path.

## Default Workflow

1. Check `ai_studio/runtime_automation/README.md` for module ownership and
   commands.
2. Find the project launch path and DevAPI port from the active game docs or
   runbook.
3. Discover the live runtime contract before acting: call `endpoints`, then
   `command.describe` for unfamiliar commands.
4. Use the shared helpers:
   `ai_studio/runtime_automation/devapi_client.py`,
   `devapi_cli.py`, `iterate.py`, `state_capture.py`,
   `pixel_health.py`, and `ui_readability.py`.
5. Use observe -> act -> `frame.wait` -> observe. Wrap interactive injection
   (`ui.click`, `input.*`) in `DevApiClient.player_gated()` — an ungated click
   with a live mouse lands in a non-primary slot, not slot 0. Capture
   logs/screenshots only after the visual state is stable.
6. Report concise evidence paths and failures. Do not claim runtime behavior
   from code inspection alone when a running build is available.

## Boundaries

- Runtime Automation owns local proof helpers, not engine DevAPI internals.
- Games own semantic commands and game-specific smoke scenarios.
- `ai_studio/quality/` owns acceptance rules such as player clarity or technical
  behavior evidence.
- `ai_studio/taskboard/` owns durable task state.
- Keep release builds automation-free unless the project explicitly changes that
  policy.

## Validation

```powershell
py -3.12 -m pytest ai_studio/runtime_automation
node ai_studio/architecture_map/validate_map.mjs
```
