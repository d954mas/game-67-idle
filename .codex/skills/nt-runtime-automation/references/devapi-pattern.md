# DevAPI Pattern

Load this for command-bus details when driving a live game through DevAPI.

## Source Of Truth

- Engine DevAPI implementation lives under `external/neotolis-engine`.
- Runtime discovery wins over static notes: call `endpoints`, then
  `command.describe`.
- Game-specific commands belong to the game. Runtime Automation only supplies
  clients, capture helpers, and proof tooling.
- State code generation belongs to `.codex/skills/nt-game-state-management/`;
  Runtime Automation only drives the live DevAPI surface.

## Wire Shape

JSON-lines over the loopback TCP transport:

```json
{"request_id":1,"method":"ui.click","params":{"id":"seed.cycle"}}
```

Response:

```json
{"ok":true,"result":{}}
```

or:

```json
{"ok":false,"error":{"code":"bad_params","message":"..."}}
```

`request_id` is correlation only and is echoed unchanged.

## Runtime Rhythm

Do not batch deferred commands such as `frame.wait` with immediate commands.
Use sequential steps:

1. send action or input;
2. call `frame.wait`;
3. read state or capture evidence.

For UI actions, prefer stable node ids from `ui.tree`. Labels/text are fallback
selectors. Never rely on tree array indices as stable selectors.

## Shared Client

Use:

```powershell
py -3.12 ai_studio/runtime_automation/devapi_cli.py 9123 endpoints
py -3.12 ai_studio/runtime_automation/iterate.py 9123 --reuse
```

Python helpers live in `ai_studio/runtime_automation/devapi_client.py`.

## Capture

Capture stays external unless the active game/engine exposes native framebuffer
capture. Use the same observe/act/wait rhythm before:

- screenshots;
- screen recordings;
- pixel health checks;
- readability checks;
- state capture matrices.

## Gate

Automation is development-only unless the project explicitly changes policy.
Release builds should not expose DevAPI commands by accident.
