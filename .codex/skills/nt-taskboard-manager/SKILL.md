---
name: nt-taskboard-manager
description: "Inspect, plan, update, or close Studio Taskboard work. Create items only when the human lead requests them or invokes the item-creation workflow."
---

# NT Taskboard Manager

Inspect or update the requested work using the Taskboard's owning contract.

## Sources

- Product/API entry: `ai_studio/taskboard/README.md`.
- Detailed task-store contract: `ai_studio/taskboard/task-store-reference.md`.
- Current-game routing: `games/<game-id>/`.

## Commands

- Context: `node ai_studio/taskboard/cli.mjs context --json`.
- List rows: `node ai_studio/taskboard/cli.mjs list --json`.
- Read one current item: `node ai_studio/taskboard/cli.mjs show <P###|E###|T####> --json`.
- Read one historical task only when explicitly needed:
  `node ai_studio/taskboard/cli.mjs show T#### --archive --json`.
- For requested archive maintenance, seal pending closed history:
  `node ai_studio/taskboard/cli.mjs archive seal --name <immutable-batch-name> --json`.
- Help: `node ai_studio/taskboard/cli.mjs help`.
- Update item: `node ai_studio/taskboard/cli.mjs set <id> --status <status> --log "..." --json`.
- Validate the affected store after updates: `node ai_studio/taskboard/cli.mjs validate --json`.
- For board UI work: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Open`, then open `/taskboard/`.
  Use the Windows host with its required permissions, never WSL.

## Workflow

Read a named item directly; use `context` or `list` when routing is needed.
Load `task-store-reference.md` for unfamiliar fields, lifecycle transitions, or
Markdown changes. Use `--game <game-id>` for private stores; keep their work off
the shared board. Read sealed history only when the task needs it.

Create items only at the human lead's request or through the designated creation
workflow. Otherwise propose new work in the reply. For authorized creation,
use `help` for commands, preserve project/epic ownership, and use `idea` until
scope and checkable done criteria justify `backlog`.

Mark `done` only after satisfying the contract's closure and applicable quality
requirements. Record evidence, validate the affected store, and report the changed item
and remaining blockers. Do not add confirmation steps for already authorized
updates.

## Boundary

Keep this skill as a router. Taskboard code, schemas, markdown templates,
browser UI, API payloads, and validation belong in `ai_studio/taskboard/`.
