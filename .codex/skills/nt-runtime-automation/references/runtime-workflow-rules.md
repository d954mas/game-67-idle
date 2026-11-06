# Runtime Workflow Rules

Load this when launching a build through DevAPI, collecting runtime evidence,
or changing the local automation helpers.

## Workflow

Use the game's runbook to launch or reuse its native DevAPI build. The engine
default port is `17890`; the runbook may override it. Consult relevant module
README sections for unfamiliar helpers or helper maintenance. Capture launch
stdout/stderr under an ignored path such as `build/logs/`.

Discover `endpoints`, describe unfamiliar commands, and use
observe -> act -> `frame.wait` -> observe. Capture stable state, inspect the
result, and report evidence paths and gaps. For video, use the
[game recording contract](game-video-capture.md); OBS is only for explicitly
requested desktop/OBS work, not a fallback for missing game integration.

## Shared Helpers

- `ai_studio/runtime_automation/devapi_client.py`: Python client, launch/reuse,
  frame waits, engine-native screenshot capture, and convenience helpers.
- `ai_studio/runtime_automation/capture_game.py`: catalog-selected game capture
  through the shared engine/audio-core recorder and decoded-media validation.
- `ai_studio/runtime_automation/devapi_cli.py`: one-shot JSON command client.
- `ai_studio/runtime_automation/iterate.py`: native visual iteration loop.
- `ai_studio/runtime_automation/state_capture.py`: multi-state screenshot
  coverage and acceptance-matrix helper.
- `ai_studio/runtime_automation/pixel_health.py`: blank/flat screenshot checks.
- `ai_studio/runtime_automation/ui_readability.py`: screenshot readability crop
  audit.

## Boundaries

- Keep reusable helpers low level: device/input/frame/capture/state.
- Put semantic actions and scenario scripts in the game project.
- Do not create a separate command database; live command registration and
  `command.describe` are the runtime contract.
- Do not enable automation in release builds unless the project explicitly
  changes that policy.

## Evidence Paths

Use ignored scratch paths such as:

- `tmp/captures/`
- `build/logs/`
- game-local `tmp/` output

Use `ai_studio/quality/` rules when deciding what evidence is enough for a
player-facing or technical acceptance claim.
