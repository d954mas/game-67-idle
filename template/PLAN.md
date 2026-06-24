# Template — continuation plan (pick up cold)

The template builds, runs, and renders. This is the ordered to-do to finish it.
Read first: `template/README.md`, `tools/bootstrap/TEMPLATE.md` (the reuse model),
`template/CONVENTIONS.md` (rules), and the memory note "engine-first + styles".

## Build / run / verify

    cmake -S template -B template/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
    cmake --build template/build
    template/build/bin/game.exe                                  # window: cube + text + [SETTINGS]
    template/build/bin/game.exe --settings --capture C:/projects/game-67-idle/tmp/x.ppm
    python -c "from PIL import Image; Image.open('tmp/x.ppm').save('tmp/x.png')"   # then view x.png

New game from the template: `node tools/bootstrap/new_game.mjs --id <id>` (verified: builds).

## What's done

Decomposed, builds green, runs, screenshot-verified:
- `src/main.c` — conductor (init -> frame() calls subsystems -> teardown).
- `src/world/world.h` — the World (state).
- `src/systems/sys_move.c` — movement (WASD).
- `src/render/render_mesh.c` — render a coloured cube character (follow camera).
- `src/ui/hud.c` — HUD text.
- `src/systems/sys_settings.c` — gear -> panel: sliders, close, long-press reset.
  **NOTE: this is a TEXT-drawn HACK — replace with nt_ui widgets (task 1).**
- `src/render/capture.c` — `--capture` screenshot (PPM).
- `src/build_packs.c` — pack: font + slug_text + mesh_inst + white + cube.

## Rules (must follow)

- **Engine first**: use the engine's public API before hand-rolling. (AGENTS.md.)
- **Styles in their own file** (`ui/theme.{c,h}`). Decompose: one system per file,
  systems talk through the World, `main.c` stays the conductor.
- Source-first for assets (`find_assets.mjs --genre/--tags` -> `pull.mjs --apply`).
- All text via the engine text renderer + a real font.

## To do (in order)

### 1. Rebuild settings on nt_ui WIDGETS (priority — kills the text hack)

Reference: `external/neotolis-engine/examples/ui_showcase/{main.c,build_packs.c}`
and `engine/ui/nt_ui_{panel,slider,button,label}.h`. Steps:
1. **Atlas**: in `build_packs.c`, `nt_builder_begin_atlas` + `nt_builder_atlas_add`
   a white pixel (min) + CC0 Kenney slice9 panel/button PNGs (task 2). Add the
   `sprite.vert/.frag` shaders (already in `assets/shaders/`).
2. **Setup** (a new `ui/ui_runtime.c` or in main): `nt_sprite_renderer_init`,
   `nt_ui_module_init`, `nt_ui_create_context(arena)`, create a sprite material
   (atlas texture), `nt_ui_set_sprite_material` / `set_text_material` / `set_font`
   / `set_atlas_white_region`.
3. **Per frame**: `nt_ui_begin(ctx, w, h, dt, &g_nt_input.pointers[0], 1)` ->
   build the panel with `CLAY({...})` + `nt_ui_slider_float` (master/music/SFX),
   `nt_ui_button` (close, reset), `nt_ui_label`, `nt_ui_panel` -> `nt_ui_end` ->
   `nt_ui_walk(ctx, &target)` -> flush sprite + text renderers.
4. **Styles** all in `ui/theme.{c,h}` (button/slider/panel styles + colours), per
   the rule. `sys_settings.c` keeps only logic (values, reset, open) + the widget calls.
5. Link the UI libs in CMake: `nt_ui nt_sprite_renderer nt_atlas nt_sprite_comp`
   (+ whatever the linker asks for). Build, `--settings --capture`, verify the panel.

### 2. Base GUI art (CC0)

Source a Kenney UI pack (CC0) via the asset library / `find_assets`; pack the
slice9 panel + button + slider art into the atlas above. This is the "GUI art"
the panel renders with.

### 3. Textured mesh example

Add a SECOND mesh path for TEXTURED models so the AI learns it: a `mesh_tex`
shader (position + uv0, samples `u_texture`), a small CC0 textured glb in
`assets/meshes/`, a textured material, draw it next to the cube. (Current
`mesh_inst` is colour-only.)

### 4. Clean Blockside Heat from repo root

BH is committed (`9ed03da`) + tagged `little-lives-snapshot-2026-06-23` style
snapshots. Remove the root game (`src/blockside_*`, `clean_seed_main.c`,
`build_packs.c`, root `CMakeLists.txt`/`state`/`assets`, `gamedesign/projects/
blockside-heat`, BH tasks, `tools/blockside-heat`) so the repo is: shared root
(engine + tools + skills + docs + library + tasks) + `template/` + game folders.
Update AGENTS `## Project`. Note: `reset_to_seed.mjs` is the OLD in-place model —
superseded by `new_game.mjs`; retire or repurpose it.

## Then

A new game = `new_game.mjs --id <id>`, customise the copy, pull assets + systems.
Epic: `tasks/epics/E009-...`.
