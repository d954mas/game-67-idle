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

## Surface Rule

A `surface` is a user-facing browser entry into a module. A `module` owns
meaning, data, contracts, and APIs. The shell hosts surfaces, but does not own
their domain logic.

Current hosted surfaces:

- `/` -> Studio Home.
- `/architecture_map/` -> Architecture Map surface.
- `/taskboard/` -> Taskboard surface.
