# Studio Shell

`studio_shell/` owns the shared browser shell for AI Studio tools.

It is not the owner of the tools it hosts. `architecture_map/` owns the map
surface and map data. `taskboard/` owns the board surface and task state.
`assets/viewer/` owns the asset browsing launcher, galleries, and the Asset
Tools browser surface.

## Owned Here

- Home surface: `index.html`.
- Home content styles: `home.css`.
- Shared dark sidebar and page frame: `studio_shell.css`.
- Shared browser behavior for sidebar collapse: `studio_shell.js`.
- Unified local server: `server.mjs`.
- Stable local launcher: `start_site.mjs`.
- Windows-safe launcher: `start_site_windows.ps1`.

## Related Utility

Temporary public sharing of self-contained static directories is handled by the
`nt-app-tunnel` skill: `.codex/skills/nt-app-tunnel/`. Studio Shell surfaces can
be shared through that skill, but `studio_shell/` does not own the tunnel
workflow.

## Start

Use the launcher instead of starting `server.mjs` by hand:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

PowerShell 7 also works if `pwsh` is installed:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Useful variants:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Port 8780 -Open
```

The launcher writes PID and logs to `tmp/ai_studio/`, checks that the page
really answers, and reports a clear error if the port is busy or the server
does not start. The `.cmd` wrapper uses the same Windows-safe launcher:

```powershell
ai_studio\studio_shell\start_site.cmd
```

Codex managed shell note: background child processes started inside the sandbox
can be killed when the command finishes. When Codex starts this site for browser
use, run `start_site_windows.ps1` outside the sandbox, then verify
`http://127.0.0.1:8765/asset_viewer/` or the needed surface.

## Surface Rule

A `surface` is a user-facing browser entry into a module. A `module` owns
meaning, data, contracts, and APIs. The shell hosts surfaces, but does not own
their domain logic.

Current hosted surfaces:

- `/` -> Studio Home.
- `/architecture_map/` -> Architecture Map surface.
- `/taskboard/` -> Taskboard surface.
- `/asset_viewer/` -> Asset Viewer launcher.
- `/asset_prep/` -> Asset Tools surface.
- `/quality/` -> Quality Checks surface.
