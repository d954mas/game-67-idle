# Visual Runtime Evidence

Load this only when the task asks for screenshots, recordings, runtime visual
health, or player-visible proof from a running build.

## Evidence Loop

1. Build and run the primary target through the normal project path.
2. Prefer native desktop proof first unless the task is specifically web/WASM.
3. Drive state through DevAPI or the project's documented playtest command.
4. Wait for a stable frame.
5. Capture screenshot, recording, state JSON, or acceptance matrix.
6. Run `pixel_health.py` or `ui_readability.py` when screenshot evidence might
   be blank, flat, unreadable, or too small.

## Basic Runtime Health

Check the requested behavior plus:

- nonblank output;
- sensible viewport/camera framing;
- no obvious rendering errors;
- controls or UI affordances respond;
- logs do not contain the failure that explains the visual result.

For acceptance decisions, use the matching rule under `ai_studio/quality/`.
This checklist only helps collect runtime evidence.

## Commands

```powershell
py -3.12 ai_studio/runtime_automation/pixel_health.py tmp/captures/screenshot.png
py -3.12 ai_studio/runtime_automation/ui_readability.py tmp/captures/screenshot.png
py -3.12 ai_studio/runtime_automation/state_capture.py --help
```
