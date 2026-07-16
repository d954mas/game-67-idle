# Game template (copy model)

A new game starts by **copying the template**. Reusable improvements go into
AI Studio, the engine, or a reusable feature pack after review.

**The template has two jobs:** (1) a runnable shell a game copies, and (2) — just
as important — it **lays down the architecture for all future work**: the worked
example of how code is decomposed (thin entry + systems + world state), how assets
flow (pull → pack → render), and how UI/state/audio are wired. Future games (and
the AI building them) follow the template's structure instead of inventing one or
piling everything into one file. Investing in the template's architecture pays off
in every game after it.

**Layout: in THIS repo, as folders** - `templates/<template-id>/` and
`games/<game-id>/`. Each game is a self-contained folder copied from a template.

- `games/new_game.mjs --visibility public|private` - copy `templates/template/`
  -> `games/<game-id>/`; the engine submodule and shared asset library are
  referenced, not copied. Public games are tracked by the parent Studio repo;
  private games are nested repos under `games/<game-id>/`.
- The asset library is a registered external source; never copied wholesale.

## Reuse tiers — what is shared vs copied-and-customized

Two KINDS of reuse, different by nature:

- **Dev pipeline** — `ai_studio/`, `.codex/skills/`, taskboard, reusable design
  knowledge, and the shared asset library. Shared infra, ONE copy used by every
  game; improvements are reviewed in their owning module. A copied game also
  has its own private design knowledge base under `games/<id>/design/knowledge/`.
- **Feature packs** — gameplay AND the shell (settings, audio, save, UI, terrain,
  character controller, ...). These inherently need PER-GAME tweaks, so they are
  **COPIED into a game when needed and the game owns/customizes its copy** — the same
  model as the asset library (pull a copy → edit locally → promote good ones back).
  NOT a frozen linked core: a single "terrain render" or "character controller" can't
  fit every game unchanged.

Tiers:
1. **Engine** — `external/neotolis-engine` (submodule; a linked, fix-once
   shared core; stable public API).
2. **Features** — storage of reusable feature packs. Three categories live here
   (`features/README.md` §"Categories: module vs feature-pointer vs game code"):
   **in-place module** (one shared copy, linked in-place by every consumer, e.g.
   `items-core`/`progression-core`/`game-state`) joins the engine as the
   LINKED-shared tier; **feature-pointer** (copy-then-own, e.g. `settings`/
   `resource_panel`) is the copy-then-own tier below. A game copies a
   feature-pointer when it needs one; good feature-pointers are promoted back.
3. **Template** (minimal) — thin `main.c` conductor + `world` + the basic shell as
   its OWN files (settings, audio, save, UI gear/panel, font, coloured+textured mesh,
   pack builder) so a new game runs immediately; copying the template brings them, and
   the game then customizes them. The shell lives in the template, not `features/`.
4. **Game-only** — a game's own systems, logic, pulled assets, tasks, design.

Trade-off (named honestly, per research): copy-then-customize IS "clone-and-own",
which needs discipline — keep each feature self-contained and PROMOTE improvements
back so `features/` stays the best version. The LINKED-shared tier (engine +
in-place modules) absorbs the cross-cutting fixes that clone-and-own otherwise
can't propagate; the copy-then-own tier (feature-pointers + game code) is where
per-game customization is expected and healthy.

## Feature library - copyable feature packs

Besides the template, `features/` is the shared pool of optional game capabilities:
browse what exists, copy what you need, customize the local copy, and promote useful
generalized improvements back. A feature can contain code, assets, state schema,
UI screens, DevAPI hooks, validation, and notes. Not every entry here copy-then-owns
the same way — see `features/README.md` §"Categories: module vs feature-pointer vs
game code" for which is which.

- **Each feature is its own folder.** e.g. `features/terrain/` can include
  source files, asset inputs, state keys, a copy note, and an `example/` when a tiny
  runnable demo is useful.
- **Self-contained is the entry rule**: a feature lists its dependencies explicitly
  and does not rely on a specific game's globals. Use the engine, the template's
  public world/state/system boundaries, and explicitly listed sibling features.
- **Copy** a feature into a game or template; the copied project owns its version.
  Promote a game's good local feature back to `features/` only after it generalizes.
- **Browsable later**: a future pass can add manifests, previews, install scripts,
  dependency checks, or enable/disable switches. For now this is a simple folder
  convention, not a plugin architecture.
- `features/` holds optional capabilities (terrain, inventory, dialogue, settings
  screen variants, day/night, etc.) that a game or template pulls on demand. The
  always-needed starter shell remains in the template.

## Reusable Studio Base

The reusable pipeline: `ai_studio/`, `.codex/skills/`, and
`ai_studio/game_design/knowledge_base/knowledge` + `sources`. Game-specific knowledge is not
stored there; it lives in each copied game's `design/knowledge/`.

## Template Seed - the runnable starter shell

The template is NOT a bare seed: a new game opens to a working shell and builds on it.

- **Design scaffold**: `design/concept.md`, `design/gdd.md`,
  `design/knowledge/`, and `design/data/*.json`. This scaffold belongs in the
  template and is copied into each new game.
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
- **Startup UX**: the template opens to a **live resource-panel HUD** (gold
  counter + xp bar, via `resource_panel`/`demo_hud`) over two sample cubes, with a
  **settings (gear) button in the top-right** of the GUI. Settings are NOT shown on
  launch — pressing the gear opens the panel below.
- **Settings panel** (`src/features/settings/` + `src/ui/*`):
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
  per the engine. The template ships a minimal but REAL set:
  - **world state** (`world/world.h`) — the World's source of truth (player
    position/yaw, mesh entity handles). Other systems read/write it; they do
    not own entities.
  - **movement** (`systems/sys_move.c`) — WASD input drives the World's player
    position/yaw, its own system, separate from rendering.
  - **render** (`render/render_mesh.c`) — draws the World's mesh entities (a
    coloured player cube + a textured prop cube) from world state, separate
    from movement.
  - **settings** (`src/features/settings/`, the reference feature-pointer) —
    the gear panel, sliders, long-press reset.
- Systems communicate through the **world state**, not by calling each other's
  internals. A new game adds its own systems the same way (copy a system file,
  register it in `main`), never by growing one file.

This is what "the AI understands the structure" means: the template is the worked
example of decomposition, not a blank seed.

### Concrete `src/` layout the template ships

Motivated by the anti-pattern: an earlier game-local seed grew into a large
god-file mixing scene build, input, update, render, DevAPI endpoints, the
HUD/UI tree, material setup, and `main`. The template ships these as SEPARATE
modules from day one so a copied game keeps them apart:

    src/
      main.c                   THE CONDUCTOR: init subsystems -> nt_app_run(frame) ->
                               teardown. frame() only CALLS subsystems in order
                               (game_features -> render); it holds no game logic
                               of its own.
      world/world.h            the World: single source of truth (player position/
                               yaw, mesh entity handles).
      systems/
        sys_move.{c,h}          WASD input -> the World's player position/yaw.
      features/                FEATURE-BASED ARCHITECTURE: folder per feature, ONE
                               public header (rules: src/features/README.md):
        game_features.{c,h}      the frame aggregator (7 phases: init/update/react/
                                 record/draw_world/draw_ui/shutdown; list = z-order)
        settings/                 feature-pointer: gear panel, sliders, close,
                                 long-press reset (public API: settings.h)
        items/                    game-local reason_tags.h + on_new_game seed; the
                                 ownership core is the in-place module
                                 features/items-core/ (L1)
        resource_panel/           feature-pointer: HUD widget, gold counter + xp
                                 bar over items/progression (L2)
      render/                  RENDER SYSTEMS, separate from game logic:
        render_mesh.{c,h}        materials (coloured + textured), the mesh
                                 renderer, a follow camera; draws the World's
                                 mesh entities from world state + frame uniforms
        capture.{c,h}             screenshot/capture support
      ui/
        hud.{c,h}                 the HUD/UI tree (panels/buttons, the top-right gear)
        ui_runtime.{c,h}          per-feature draw_ui frame (ctx + z-order calls)
        theme.{c,h}               shared UI theme
        demo_hud.{c,h}            resource_panel composition (idle-income demo)
      game_{save,storage,state_json,events,event_render,log,analytics,format}.*
                               L0 SHELL: fragment registry/orchestration, JSON
                               save/load, typed events, formatting.
      game_save_devapi.c      hand-written universal `game.state` DevAPI dispatch
                              over the fragment registry (A5); registered by
                              `game_save_register_devapi()`, built only under
                              `GAME_DEVAPI_ENABLED`. Engine-owned DevAPI groups
                              (ui.*, input.*, frame/time, obs, capture.*) are
                              wired from the engine, NOT duplicated here
      build_packs.c           pack builder (font + shaders + white + textured sample)
      game_audio.*            seed infra (music/SFX buses); not yet compiled into
                              the game target
    devapi/
      smoke_bot.py            game-local Python runtime bot: launch via DevAPI,
                              discover commands, click a stable UI id, capture
                              evidence; copy this pattern for scenario tests
      responsive_viewports.py reusable QCLR_002 helper: relaunch each viewport,
                              let a bot prepare a state, then capture screenshots
                              plus ui.tree bounds for responsive-layout review
    state/                    the 4 fragment schemas (codegen source): settings/
                              items/progression/game
    content/                  progression.json + items.lock.json (Items release history)
    items.lua.json            canonical Items Lua evaluator manifest
    design/items/             modular Items Lua declarations

Outside `src/`, two in-place modules carry the L1/L2 learning-feature runtime
(one shared copy, linked in-place, NOT copy-then-own): `features/items-core/`
(ownership, catalog, reconcile) and `features/progression-core/` (curve,
level-ups); see `features/README.md` §"Categories: module vs feature-pointer
vs game code".

Engine-owned pieces stay engine-side (public API): `nt_mesh_renderer`,
`nt_text_renderer`, `nt_devapi`, `cjson`, ECS comps, `nt_resource`. Seed infra
files (`game_storage.*`, `game_audio.*`, `state/`) stay as-is. A new game adds a
system by dropping `systems/sys_<thing>.{c,h}` and registering it in `main`; do
not grow one large file.

## GAME-ONLY

The template contains only a placeholder `design/` scaffold. Filled-in game
design content, private knowledge pages, Taskboard project/epic/task entries in
`ai_studio/taskboard/items/`, game-local runtime scripts, game-local asset
source/storage folders, built runtime packs (`*.ntpack`), and game src modules
(anything in `src/` beyond the seed files) are owned by `games/<id>/`.

## Build approach

Most of the shell already exists as proven reusable runtime code: pack pipeline,
text, meshes, state, storage, and audio. The template is distilled from earlier
prototype work: genericize game-specific names, strip game logic, keep the
shell. The settings screen + GUI art + long-press reset are added on top,
sourcing the panel/button kit from the shared library (Kenney UI, CC0).
