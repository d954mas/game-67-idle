# Systems showcase

A storage of **optional, reusable game systems** — the code analog of the shared
asset library. Browse what exists, see each one work, copy the ones you need into a
game, customize your copy, and promote good systems back. See the model in
`ai_studio/bootstrap/TEMPLATE.md`.

Not in the minimal template: the always-needed starter shell (settings, audio, save,
UI, text, mesh) lives in `template/`. The showcase is for the rest — terrain,
inventory, dialogue, vehicle, day/night, pathfinding, … — that only some games need.

## Each solution is its own folder with a runnable example

    systems_showcase/<name>/
      <name>.c  <name>.h     the system: depends only on the engine + the World API
                             (and explicitly-listed sibling systems) — never a
                             specific game's globals.
      example/               a tiny main/scene that shows it working
        main.c  CMakeLists.txt  (builds against ../../external/neotolis-engine)
        screenshot.png
      README.md              what it does, deps, how to wire it, origin/license.

The example is BOTH the living demo and the smoke test — if the example builds and
runs, the system is usable.

## Use

- **Pull** a solution into a game: copy `systems_showcase/<name>/<name>.{c,h}` into
  the game's `src/systems/` (or wherever), add it to the game's CMake sources, and
  register it in the game's `frame()`. Customize your copy freely.
- **Promote**: when a game builds a good, self-contained system, copy it here with an
  `example/` so the next game can reuse it.

## Rules (so a solution stays pull-able)

- Self-contained: engine + World API only; no game globals, no cross-system reach-in.
- One responsibility per system; data-oriented (SoA over the World).
- Ships a building example. If the example rots, the system is broken.
