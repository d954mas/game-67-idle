# Runtime Workflow Rules

Load this when launching a build through DevAPI, collecting runtime evidence,
or changing the local automation helpers.

## Workflow

1. Read `ai_studio/runtime_automation/README.md`.
2. Find the active game's normal launch/runbook before creating ad hoc scripts.
3. Start or reuse the native debug build with DevAPI enabled; the engine default
   native port is `17890` unless the game runbook overrides it.
4. Capture launch stdout/stderr to an ignored log path such as `build/logs/`.
5. Discover `endpoints`; use `command.describe` before unfamiliar commands.
6. Use observe -> act -> `frame.wait` -> observe.
7. Capture evidence after the state is stable.
8. Report paths to logs, screenshots, recordings, JSON state, or acceptance
   matrix files.

## Shared Helpers

- `ai_studio/runtime_automation/devapi_client.py`: Python client, launch/reuse,
  frame waits, engine-native screenshot capture, recordings, and convenience
  helpers.
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
