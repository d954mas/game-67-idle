# Studio Shell

`studio_shell/` owns the shared browser shell for AI Studio tools.

It is not the owner of the tools it hosts. `architecture_map/` owns the map
surface and map data. `taskboard/` owns the board surface and task state.

## Owned Here

- Home surface: `index.html`.
- Home content styles: `home.css`.
- Shared dark sidebar and page frame: `studio_shell.css`.
- Shared browser behavior for sidebar collapse: `studio_shell.js`.
- Unified local server: `server.mjs`.
- Stable local launcher: `start_site.mjs`.

## Start

Use the launcher instead of starting `server.mjs` by hand:

```powershell
node ai_studio/studio_shell/start_site.mjs --open
```

Useful variants:

```powershell
node ai_studio/studio_shell/start_site.mjs
node ai_studio/studio_shell/start_site.mjs --restart
node ai_studio/studio_shell/start_site.mjs --port 8780 --open
```

The launcher writes PID and logs to `.tmp/ai_studio/`, checks that the page
really answers, and reports a clear error if the port is busy or the server
does not start.

On Windows, a minimal detached start command is also available:

```powershell
ai_studio\studio_shell\start_site.cmd
```

Codex managed shell note: background child processes started inside the sandbox
can be killed when the command finishes. When starting the site for browser use
from Codex, run the Windows detached command outside the sandbox and then verify
`http://127.0.0.1:8765/`.

## Surface Rule

A `surface` is a user-facing browser entry into a module. A `module` owns
meaning, data, contracts, and APIs. The shell hosts surfaces, but does not own
their domain logic.

Current hosted surfaces:

- `/` -> Studio Home.
- `/architecture_map/` -> Architecture Map surface.
- `/taskboard/` -> Taskboard surface.
