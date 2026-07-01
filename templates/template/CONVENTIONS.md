# Conventions (copied into every game)

How code is structured here. These exist to stop the failure mode where an agent
dumps a whole game into one file. Keep them.

## Engine first (don't hand-roll)

- Before writing custom code, SEARCH THE ENGINE and use its public API. UI uses the
  engine widgets (`nt_ui_panel`/`nt_ui_slider_float`/`nt_ui_button`/`nt_ui_label`,
  see `external/neotolis-engine/examples/ui_showcase`), not hand-drawn text/quads.
  Same for renderers, math, input, resources. Hand-roll ONLY when the engine has no
  fit — and say why. (A text-drawn "UI" is a hack; the nt_ui widgets are the way.)
- Styles/theme (colours, button/slider/panel styles) live in their OWN file
  (e.g. `ui/theme.{c,h}`), separate from the logic that uses them.

## Decomposition

- **`main.c` is the conductor, not the game.** It only: init subsystems →
  `nt_app_run(frame)` → teardown. `frame()` only CALLS subsystems in order
  (input → game-system updates → render systems). No gameplay rules in `main.c`.
- **One system per file.** A system (`systems/sys_*.c`, `render/*.c`, `ui/*.c`,
  `scene/*.c`, `devapi/*.c`) has a single responsibility. Add a feature by adding a
  file + registering it in `frame()`, NEVER by growing an existing file.
- **The World is the source of truth.** Systems read/write the `World`
  (`world/world.{c,h}`): entity handles + per-system SoA state. Systems do not own
  entities and do not call into each other's internals — they go through the World.
- **DevAPI commands live in `devapi/`**, the HUD/UI tree in `ui/`, render + material
  setup in `render/` — never inline in `main.c`.

## Anti-patterns (do NOT do)

- **God-file:** a monolithic `main.c` with window + assets + all systems + render +
  rules. (If `main.c` exceeds ~200 lines or contains gameplay logic, decompose.)
- **God-struct:** one fat `Entity`/struct every system reads (AoS). Use per-system
  SoA; an entity is a handle indexing into system data.
- **Clone-and-own as a strategy:** don't copy a previous game and mutate it. Start
  from `templates/template/`, pull reusable feature packs from `features/`, and promote good
  features back so the shared versions stay best. Engine fixes come from the one
  shared `external/neotolis-engine`.

## Assets

- Reuse first: the shared asset library has thousands of reusable game assets.
  Search with `node ai_studio/assets/storage/search.mjs --query "<need>" --kind <kind> --json`,
  pull with `node ai_studio/assets/viewer/pull.mjs --ids <asset-id> --to <game>/assets --apply`,
  then pack from the game-local asset copy. Generate only what you can't source.
- All on-screen text uses the engine text renderer + a real font — never hand-drawn
  pixel/shape text.
