---
name: nt-runtime-automation
description: "Collect game runtime proof through DevAPI discovery, input, frame waits, screenshots, and recordings; maintain the shared automation helpers."
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

For routine game video, start in the game root and use:

```text
capture live
capture shot <id>
```

Read the game-owned `capture/catalog.json` before choosing an id. The shared
`capture_game.py` route uses `game.record.start` and `game.record.stop` from
`features/game-recording`, combining engine framebuffer capture with audio-core's
actual post-mix stereo samples. OBS is not required for this route. Keep the
legacy `record_game.py` tool only for explicitly requested desktop/OBS work;
do not silently fall back to it when a game lacks the native recording module.

Use Windows native DevAPI builds and the approved host execution path. In Codex,
launch the game-owned capture command with `sandbox_permissions: "require_escalated"`
when the managed command sandbox cannot access the interactive game's window,
audio device or build metadata. Production and web builds exclude this native
integration. Discover the live command contract first, require a nonzero
framebuffer, and unlock audio through ordinary player input before recording.

`live` records continuous play. `shot` runs the catalog's authored scenario on
the live wall clock; setup frame waits are host-paced. Do not claim deterministic
or frame-exact simulation from a realtime recording. Do not substitute raw
engine `record.*` commands: those do not attach the game's audio mixer.

Treat every completed take as a draft first. Check the recorder receipt and
fully decode the saved media: codec, dimensions, cadence, audio stream and
sample count. Inspect distinct frames and audio level; static or muted takes
need an appropriate scenario expectation. FPS metadata alone does not prove
smooth motion. Check an observable audiovisual event when proving sync, and
exercise mute, pause, restart and shutdown after capture workflow changes.

A shot becomes a true master only when recording validation, scenario completion,
and the applicable universal safe-area gate all pass. An incomplete safe-area
policy remains guidance and keeps the take in `draft/`.

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
node ai_studio/studio.mjs verify --domain runtime
node ai_studio/architecture_map/validate_map.mjs
```
