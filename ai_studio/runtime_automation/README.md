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
py -3.12 -m pytest ai_studio/runtime_automation
py -3.12 ai_studio/runtime_automation/devapi_cli.py 17890 endpoints
py -3.12 ai_studio/runtime_automation/iterate.py 17890 --reuse
py -3.12 ai_studio/runtime_automation/ui_readability.py tmp/captures/screenshot.png
py -3.12 ai_studio/runtime_automation/pixel_health.py tmp/captures/screenshot.png
```

Set `AI_STUDIO_GAME_EXE` when driving a specific game build. Without it, the
DevAPI launcher looks for the template executable at
`templates/template/build/bin/game.exe`. The default port follows the engine
default `17890`; override with `AI_STUDIO_DEVAPI_PORT` or `NT_DEVAPI_PORT`.

## Skill

Use `nt-runtime-automation` when an agent needs to collect runtime evidence,
drive DevAPI, capture screenshots/recordings, or change this module's helpers.

## Boundaries

Use this module when the agent needs local runtime evidence: state snapshots,
screenshots, visual health checks, readability zooms, or repeatable native
iteration probes.

Game-specific context and startup gates belong inside `games/<game-id>/`.
State-code generation lives inside `features/game-state/`.
