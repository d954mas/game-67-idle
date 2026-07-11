# Runtime Automation

AI Studio module for local runtime proof automation.

## Role

Runtime Automation owns game/runtime interaction helpers used by agents during
native playable work:

- DevAPI TCP client and minimal CLI.
- Native debug launch/reuse helpers.
- Framebuffer, window screenshot, and screen recording helpers.
- PNG encode/decode utilities used by capture and health checks.
- Pixel-health checks for blank or flat screenshots.
- UI readability zoom audit for screenshot evidence.
- Live-state screenshot coverage and acceptance-matrix helper.

This module does not own engine DevAPI implementation, game-specific commands,
game state schemas, quality rules, or task state. The engine owns DevAPI
runtime behavior; games own the commands they expose; `ai_studio/quality/`
owns acceptance rules; `ai_studio/taskboard/` owns durable work state.

## Commands

```powershell
node ai_studio/dev_environment/python_run.mjs -m unittest discover -s ai_studio/runtime_automation -p "*_test.py"
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/devapi_cli.py 17890 endpoints
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/iterate.py 17890 --reuse
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/ui_readability.py tmp/captures/screenshot.png
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/pixel_health.py tmp/captures/screenshot.png
```

Set `AI_STUDIO_GAME_EXE` when driving a specific game build. Without it, the
DevAPI launcher looks for the template executable at
`templates/template/build/bin/game.exe`. `AI_STUDIO_DEVAPI_PORT` or
`NT_DEVAPI_PORT` still pin an explicit port when set.

When `running_game()` launches a new game with no explicit port and no env
override, it now picks a free ephemeral port automatically for that one
launch instead of the fixed `17890`, so concurrent sessions never collide on
the same port (VibeJam had 8-9 concurrent sessions bind-fail and exit
instantly on a shared fixed port, which looked like a hung 5s connect
timeout). CLI tools that attach to an already-running game (`devapi_cli.py`,
`iterate.py`) still default their port argument to `17890`, the conventional
single-session port, when you don't pass one explicitly.

## Player gate for injected input

Wrap any `ui.click`/`input.*` interaction sequence in `DevApiClient.player_gated()`.
The player-input gate's ON->OFF edge clears all real pointer slots; without it, an
injected click can land in a non-primary slot beside a live mouse instead of the
always-free slot 0. The context manager disables the gate on enter and always
re-enables it on exit, even if the body raises:

```python
with game.player_gated():
    game.click_ui("settings/gear")
    game.wait_frames(2)
```

## Skill

Use `nt-runtime-automation` when an agent needs to collect runtime evidence,
drive DevAPI, capture screenshots/recordings, or change this module's helpers.

## Boundaries

Use this module when the agent needs local runtime evidence: state snapshots,
screenshots, visual health checks, readability zooms, or repeatable native
iteration probes.

Game-specific context and startup gates belong inside `games/<game-id>/`.
State-code generation lives inside `features/game-state/`.
