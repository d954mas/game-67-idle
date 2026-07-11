# Template (game starter)

The minimal runnable game every new game is copied from. Spec + reuse model:
`../TEMPLATE.md`.

Layout (decomposition the template teaches by example — no god-file):

    src/
      main.c                 THE CONDUCTOR: init -> nt_app_run(frame) -> teardown; frame()
                             only calls subsystems. No game logic here.
      world/world.h          the World: single source of truth (entity handles, sim state).
      features/              feature-based architecture. `game_features.c` is the 7-phase
                             aggregator (init/update/react/record/draw_world/draw_ui/
                             shutdown; call order = z-order):
                               settings/        gear panel, sliders, close, long-press reset.
                               items/           game-local reason_tags.h + on_new_game seed
                                                (ownership core lives in features/items-core/).
                               resource_panel/  HUD widget: gold counter + xp bar.
      systems/sys_move.c     game systems, one file each, over World.
      render/                render systems, separate from game logic:
                               render_mesh.{c,h}  draw meshes from world state + frame uniforms.
                               capture.{c,h}      screenshot/capture support.
      ui/                    hud.{c,h}, ui_runtime.{c,h} (per-feature draw_ui frame),
                             theme.{c,h}, demo_hud.{c,h} (resource_panel composition).
      game_{save,storage,state_json,events,event_render,log,analytics,format}.*
                             L0 shell: fragment registry/orchestration, JSON save/load,
                             typed events, formatting. `game_{save,events}_devapi.c` build
                             only under `GAME_DEVAPI_ENABLED`.
      build_packs.c          pack builder -> game.ntpack + generated asset-id header.
      game_audio.*           seed infra (music/SFX buses); not yet compiled into the game target.
    devapi/                  game-local Python bots and runtime smoke scenarios.
    assets/shaders/          common/ + slug_text + sprite + mesh_inst + mesh_tex.
    state/                   the 4 fragment schemas (codegen source): settings/items/
                             progression/game.
    content/                 items.json / progression.json / item_fields.schema.json /
                             items.lock.json — item + progression catalog content, read by
                             the codegen and the read-only op-CLI.
    design/                  game-owned concept, GDD, private knowledge base, and
                             structured design data scaffold.

Dropped on purpose: a src-level `devapi/` dir and a `scene/` builder do NOT
exist — game-owned DevAPI commands ship as `src/game_*_devapi.c` under
`GAME_DEVAPI_ENABLED` (not a `src/devapi/` dir), and a scene builder is future work.

Feature-based architecture: see `CONVENTIONS.md` + `src/features/README.md`;
reusable-feature ownership model: `features/README.md`.

Status: runnable shell — gear/settings panel (sliders + long-press reset), items
(L1) + progression (L2) systems, and a live resource-panel HUD on launch (gold
counter + xp bar via `resource_panel`/`demo_hud`) over two sample cubes;
feature-based architecture; native + 3 web presets green; test base 16 (→17
with this spec). The E009 arc is code-complete.

Debug builds enable the engine DevAPI path by default (`GAME_DEVAPI_ENABLED=ON`):
`--devapi <port>` starts the engine TCP transport and exposes engine-owned
`endpoints`, `command.describe`, `frame/time`, `input`, `ui`, `obs`, and
`capture.*` groups. The installed `game-state` feature also registers
`game.state.schema`, `game.state.get`, `game.state.set`, `game.state.patch`,
`game.state.save`, `game.state.load`, and `game.state.reset` from generated
sources when `GAME_DEVAPI_ENABLED` is on. Release
builds default DevAPI off, so those command registrations do not ship.
Game-specific commands belong under `src/devapi/` when a copied game needs them.

Feature flags:

- The game-state runtime always builds: the `FEATURE_GAME_STATE` on/off axis
  was removed 2026-07-07 (lead: a game without state is impossible).
- `GAME_DEVAPI_ENABLED=ON` enables engine DevAPI groups and the state DevAPI
  commands.

Persistent state = 4 fragments (`settings`/`items`/`progression`/`game`) over the
`features/game-state` registry, each registered by one line in `main.c`.

The state schema and migrations live in source under `state/`; CMake generates
`game_state.*` into `build/<config>/generated/game-state/` before compiling.

## Web build (browser, out of the box)

The template builds and runs in a browser with two commands (no manual wiring).
The tracked web shell is `web/index.html.in` (CMake `configure_file`s it into
`build/<preset>/bin/index.html`, substituting the tab title `@GAME_TITLE@` and,
on the DevAPI preset, `Module.arguments=['--devapi','17890']`). All three files
(`web/`, `tools/`, `tests/`) are tracked and copy into a new game via
`new_game.mjs`, so a copied game gets the same commands for free.

Build one preset, then serve it:

```bash
bash tools/build_web.sh --preset wasm-release        # or wasm-debug / wasm-devapi-debug
node tools/serve_web.mjs --preset wasm-release        # http://127.0.0.1:8080/
```

`build_web.sh` builds the wasm `game` target and copies the native asset pack
flat to `bin/assets/game.ntpack` (the engine streams packs over HTTP relative to
the page URL; the pack builder is native-only, so the pack is taken from the
native build). `serve_web.mjs` is a self-contained static server that serves
`game.wasm` as `application/wasm` (required for emscripten's streaming compile).

| preset | configure flags | port | DevAPI | notes |
|---|---|---|---|---|
| `wasm-release` | `-DCMAKE_BUILD_TYPE=Release` | 8080 | no | human default |
| `wasm-debug` | `-DCMAKE_BUILD_TYPE=Debug` | 8080 | no | carries ASan (larger/slower) |
| `wasm-devapi-debug` | `-DCMAKE_BUILD_TYPE=Debug -DGAME_DEVAPI_ENABLED=ON` | 8081 | yes | engine DevAPI over the web transport |

Each preset gets its own `build/engine/<preset>` archive dir, and every
build-type x DevAPI combination resolves to a unique preset name, so no build can
clobber another's engine archives. Use `wasm-devapi-debug` for DevAPI on web.

A copied game only needs `-DGAME_TITLE="My Game"` at configure to brand the tab
title; without it the title is `"Template"` (still works).

Two advisory headless probes (not part of `ctest`; one command, real signal —
they SKIP with exit 2 when EMSDK/Chrome is missing or a slow ASan boot times out):

```bash
node ai_studio/dev_environment/python_run.mjs templates/template/tests/web_devapi_check.py        # window.__devapi shim round-trip: endpoints + command.describe
node ai_studio/dev_environment/python_run.mjs templates/template/tests/web_persistence_check.py   # localStorage save survives a full Chrome quit+restart
```

`web_devapi_check.py` builds `wasm-devapi-debug`, loads it headless, and proves
`endpoints {}` returns a non-empty command list and `command.describe
{"method":"endpoints"}` returns the 7-field descriptor — both over
`window.__devapi.submit`. Exit 0 = PASS, 1 = FAIL, 2 = SKIP.

Runtime bots and smoke scenarios live under top-level `devapi/`. Start with:

```powershell
cmake --build build/devapi-debug --target devapi_smoke
```

or from the repository root:

```powershell
node ai_studio/dev_environment/python_run.mjs templates/template/devapi/smoke_bot.py --exe templates/template/build/devapi-debug/bin/game.exe
```

For QCLR_002 responsive-viewport screenshots:

```powershell
cmake --build build/devapi-debug --target quality_responsive
```

Bots can import `devapi/responsive_viewports.py` or pass
`--scenario path.py:prepare_state` to capture the same viewport matrix after a
specific game moment. Use `game.result("game.state.get", {"path": ""})` or a
semantic game command when a scenario needs state after an action.
