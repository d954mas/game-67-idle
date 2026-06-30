# Game template (copy model)

A new game starts by **copying the template**; when it finishes, its **universal**
parts copy **back** to the template, so the next game inherits the improvements.

**The template has two jobs:** (1) a runnable shell a game copies, and (2) — just
as important — it **lays down the architecture for all future work**: the worked
example of how code is decomposed (thin entry + systems + world state), how assets
flow (pull → pack → render), and how UI/state/audio are wired. Future games (and
the AI building them) follow the template's structure instead of inventing one or
piling everything into one file. Investing in the template's architecture pays off
in every game after it.

**Layout: in THIS repo, as folders** — `template/`, `game1/`, `game2/`, … . Each
game is a self-contained folder copied from `template/`.

- `new_game.mjs` — copy `template/` → `gameN/` (UNIVERSAL + SEED; engine submodule
  + shared asset library are referenced, not copied).
- `sync_to_template.mjs` — copy UNIVERSAL from `gameN/` → `template/`.
- Paths are defined in `template_paths.mjs` (the single source of truth; the
  UNIVERSAL/SEED lists are relative to a template-or-game folder, not the repo root).
- The asset library is shared and external (YandexDisk); never copied.

## Reuse tiers — what is shared vs copied-and-customized

Two KINDS of reuse, different by nature:

- **Dev pipeline** — `ai_studio/`, `.codex/skills/`, taskboard, reusable design
  knowledge, and the shared asset library. Shared infra, ONE copy used by every
  game; improvements are immediately everyone's. (The UNIVERSAL set below; flows
  both ways.)
- **Code systems** — gameplay AND the shell (settings, audio, save, UI, terrain,
  character controller, …). These inherently need PER-GAME tweaks, so they are
  **COPIED into a game when needed and the game owns/customizes its copy** — the same
  model as the asset library (pull a copy → edit locally → promote good ones back).
  NOT a frozen linked core: a single "terrain render" or "character controller" can't
  fit every game unchanged.

Tiers:
1. **Engine** — `external/neotolis-engine` (submodule; the ONLY truly linked,
   fix-once shared core; stable public API).
2. **Systems showcase** — storage of reusable code systems, each a folder with a
   runnable example. A game COPIES what it needs and customizes it; good systems are
   promoted back. (No separate frozen "core" — even shell/terrain/character systems
   get per-game edits, so copy-then-own, not link.)
3. **Template** (minimal) — thin `main.c` conductor + `world` + the basic shell as
   its OWN files (settings, audio, save, UI gear/panel, font, coloured+textured mesh,
   pack builder) so a new game runs immediately; copying the template brings them, and
   the game then customizes them. The shell lives in the template, not the showcase.
4. **Game-only** — a game's own systems, logic, pulled assets, tasks, design.

Trade-off (named honestly, per research): copy-then-customize IS "clone-and-own",
which needs discipline — keep each system self-contained and PROMOTE improvements
back so the showcase stays the best version. The engine as the stable linked core
absorbs the cross-cutting fixes that clone-and-own otherwise can't propagate.

## Systems library — a SHOWCASE of solutions (opt-in code reuse)

Besides the template, a **storage/showcase of solutions** (game systems): browse
what exists, SEE each one work, pull what you need. Like the asset library, but for
CODE — and like the engine's `examples/` folder, **each solution demonstrates itself**.

- **Each solution is its own folder with a runnable EXAMPLE.** e.g.
  `systems_showcase/terrain/` = the system (`terrain.{c,h}`) + `example/` (a tiny
  main/scene that shows it working, with a screenshot) + a record (what it does;
  deps = engine + the `World` API + any other systems it pulls; origin). The example
  is both the **living demo** and the **smoke test**.
- **Self-contained is the entry rule**: a solution depends only on the engine and
  the `World` API (and explicitly-listed sibling systems) — never a specific game's
  globals — so it pulls cleanly into any game.
- **Pull** a solution into a game (like `pull.mjs` for assets); the example stays in
  the showcase. **Promote** a game's good system into the showcase when it generalizes.
- **Browsable** like the asset viewer: an index + per-solution preview + the example
  you can actually run.
- The showcase holds the **optional** systems (terrain, inventory, dialogue, …) a game
  pulls on demand. The always-needed **starter** systems (settings, audio, save, UI
  shell, font/mesh/pack) are NOT here — they simply live **in the template**, since
  every game starts with them. No "starter vs optional" tag, no separate "core".

## UNIVERSAL (flows both ways)

The reusable pipeline: `ai_studio/`, `.codex/skills/`,
`AI_PIPELINE_HISTORY.md`, and `gamedesign/knowledge` + `sources`.

## SEED — the runnable starter SHELL (copied once, then game-owned)

The template is NOT a bare seed: a new game opens to a working shell and builds on it.

- **Pack pipeline**: `src/build_packs.c` + CMake `build_game_packs` → `<game>.ntpack`
  + generated asset-id header. Packs the items below.
- **Text**: a starter **OFL font** (`assets/fonts/`) + `slug_text` shaders +
  `nt_text_renderer`. Every game has readable text from the first run.
- **Meshes**: `mesh_inst` shaders through `nt_mesh_renderer`, with TWO worked
  examples so the AI learns both paths and uses the right one:
  - **untextured / per-instance colour** — 1×1 white texture + `baseColorFactor`
    (most library glb, e.g. poly.pizza, are vertex/colour-only),
  - **textured** — a starter glb that ships a texture; pack the texture, bind it as
    `u_texture`, sample it. So games take **textured** meshes from the first run, not
    only flat-coloured ones.
  Ready to draw library glb: pull → pack → render (skill `nt-asset-workflow`). A small
  CC0 textured glb is sourced into `assets/meshes/` as the textured example.
- **State + saves**: base game state (`state/` schema + codegen) and
  `src/game_storage.*` (save/load, autosave).
- **Audio**: `src/game_audio.*` (music/SFX buses).
- **Startup UX**: the template opens to an **empty scene** with a **settings
  (gear) button in the top-right** of the GUI. Settings are NOT shown on launch —
  pressing the gear opens the panel below.
- **Settings panel** (`src/game_devapi_ui.*` / a settings module):
  - volume sliders (master / music / SFX),
  - a **Close** button,
  - a **Reset** button that **resets the game state on a long-press** (hold to
    confirm — avoids an accidental wipe).
- **Base GUI art** (CC0, sourced from the library/Kenney UI): panels, buttons,
  a close (✕) icon — a 9-slice panel + button kit so menus look intentional day one.

## Architecture — decomposition the template teaches BY EXAMPLE

The biggest problem: agents dump the whole game into one file. The template ships
a **decomposed reference structure** so a copied game inherits it instead of a
mega-`main`. Rules the seed demonstrates:

- **Thin entry point** (`src/main.c`): init systems → run the frame loop → tear
  down. NO game logic lives here — it only wires systems together.
- **Systems, one file each** (`.c`/`.h`), single responsibility, data-oriented
  (SoA, typed handles) per the engine. The template ships a minimal but REAL set:
  - **world state** (`world_state.*`) — the world's source of truth: entity
    handles/references, lookups, spawn/despawn. Other systems read/write it; they
    do not own entities.
  - a sample **character** entity with TWO separate systems:
    - **movement** (`sys_character_move.*`) — walks the character around the world
      (input/AI → position in world state),
    - **render** (`sys_character_render.*`) — draws it from world state (mesh +
      transform), separate from movement.
  - **settings** (`sys_settings.*`) — the gear panel, sliders, long-press reset.
  - input / camera as their own systems.
- Systems communicate through the **world state**, not by calling each other's
  internals. A new game adds its own systems the same way (copy a system file,
  register it in `main`), never by growing one file.

This is what "the AI understands the structure" means: the template is the worked
example of decomposition, not a blank seed.

### Concrete `src/` layout the template ships

Motivated by the anti-pattern: the current Blockside Heat `clean_seed_main.c` is an
896-line god-file mixing scene build, input, update, render, dozens of DevAPI
endpoints, the HUD/UI tree, material setup and `main`. The template ships these as
SEPARATE modules from day one so a copied game keeps them apart:

    src/
      main.c                  THE CONDUCTOR: init subsystems -> nt_app_run(frame) ->
                              teardown. frame() only CALLS subsystems in order
                              (sys_input -> game-system updates -> render systems);
                              it holds no game logic of its own.
      world/world_state.{c,h} the world: entity handles, lookups, spawn/despawn.
      scene/scene_setup.{c,h} build the starting world (add_object, create entities).
      systems/                GAME SYSTEMS, one responsibility each, over world state:
        sys_input.{c,h}         input -> intents
        sys_character_move.{c,h} walk a character through the world
        sys_camera.{c,h}        camera follow
        sys_settings.{c,h}      gear panel: sliders, close, long-press reset
      render/                 RENDER SYSTEMS, separate from game logic:
        render_setup.{c,h}      materials (mesh + text), shaders, font, frame UBO,
                                fallback texture  (was inline in main)
        render_world.{c,h}      draw meshes from world state + frame uniforms
        render_character.{c,h}  draw the character (separate from its movement)
      ui/
        hud.{c,h}               the HUD/UI tree (panels/buttons, the top-right gear)
        ui_devapi.{c,h}         UI-driving DevAPI (register_ui_devapi)
      devapi/
        game_devapi.{c,h}       state_json, emit_state, endpoint registration; the
                                game's ep_* commands live here, NOT in main
      build_packs.c           pack builder (font + shaders + white + textured sample)

Engine-owned pieces stay engine-side (public API): `nt_mesh_renderer`,
`nt_text_renderer`, `nt_devapi`, ECS comps, `nt_resource`. Seed infra files
(`game_storage.*`, `game_audio.*`, `state/`) stay as-is. A new game adds a system
by dropping `systems/sys_<thing>.{c,h}` and registering it in `main`; do not grow
one large file.

## GAME-ONLY (never in the template)

`gamedesign/projects/<id>/`, per-game `tasks/active|epics|archive`, `tools/<id>/`,
game-local asset source/storage folders, built runtime packs (`*.ntpack`), and
game src modules (anything in `src/` beyond the seed files).

## Build approach

Most of the shell already exists as working code in the current Blockside Heat
runtime (pack pipeline, text, meshes, state, storage, audio). The template is
**distilled** from it: genericize names (`blockside_*` to generic), strip game
logic, keep the shell. The settings screen + GUI art + long-press reset are added
on top, sourcing the panel/button kit from the shared library (Kenney UI, CC0).
