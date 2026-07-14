# Studio Shell

`studio_shell/` owns the shared browser shell for AI Studio tools.

It is not the owner of the tools it hosts. `architecture_map/` owns the map
surface and map data. `taskboard/` owns the board surface and task state.
`assets/gallery/` owns the asset browsing launcher and galleries.
`assets/canvas/` owns the multi-image canvas editor surface.

## Owned Here

- Home surface: `index.html`.
- Home content styles: `home.css`.
- Shared dark sidebar and page frame: `studio_shell.css`.
- Shared browser behavior for sidebar collapse: `studio_shell.js`.
- Foreground cross-platform server: `server.mjs`.
- Canonical detached Windows agent launcher: `start_site_windows.ps1`.

## Related Utility

Temporary public sharing of self-contained static directories is handled by the
`nt-app-tunnel` skill: `.codex/skills/nt-app-tunnel/`. Studio Shell surfaces can
be shared through that skill, but `studio_shell/` does not own the tunnel
workflow.

## Start

On Windows, agents start the detached site through this single entry point:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Agent start/restart and the explicit smoke require host-process permission
outside the managed sandbox. Invoke this PowerShell command directly; never
route it through WSL.

Useful variants:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Port 8780 -Open
```

The launcher accepts ports `1..65535`, writes PID and logs to
`tmp/ai_studio/`, and reports a clear error if the port is occupied or the
server does not start. Health is accepted only when the recorded live process
is the matching `node .../studio_shell/server.mjs <port>` process. `-Restart`
never stops a stale or reused PID that belongs to another command.

`server.mjs` remains the foreground cross-platform implementation for direct
debugging and server tests:

```powershell
node ai_studio/studio_shell/server.mjs 8765
```

The explicit Windows integration smoke does not open a browser and is not part
of normal unit-test discovery:

```powershell
node --test ai_studio/studio_shell/tests/launcher_windows.integration.mjs
```

## Surface Rule

A `surface` is a user-facing browser entry into a module. A `module` owns
meaning, data, contracts, and APIs. The shell hosts surfaces, but does not own
their domain logic.

Current hosted surfaces:

- `/` -> Studio Home.
- `/architecture_map/` -> Architecture Map surface.
- `/taskboard/` -> Taskboard surface.
- `/asset_viewer/` -> Asset Viewer launcher.
- `/canvas` -> 302 redirect (query string preserved) to the Canvas surface
  (`ai_studio/assets/canvas/site/canvas.html`), owned by `assets/canvas/`.
- `/items` -> 302 redirect (query string preserved) to the Items Viewer surface
  (`ai_studio/assets/items_viewer/site/items.html`), owned by `assets/items_viewer/`.
- `/quality/` -> Quality Checks surface.
