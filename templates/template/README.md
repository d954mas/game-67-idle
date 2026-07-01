# Template (game starter)

The minimal runnable game every new game is copied from. Spec + reuse model:
`../TEMPLATE.md`.

Layout (decomposition the template teaches by example — no god-file):

    src/
      main.c            THE CONDUCTOR: init -> nt_app_run(frame) -> teardown; frame()
                        only calls subsystems. No game logic here.
      world/world.h     the World: single source of truth (entity handles, sim state).
      ui/hud.{c,h}      on-screen text/UI (its own system).
      render/           render systems (setup + draw), separate from game logic.   [wip]
      systems/          game systems, one file each (input, camera, character move). [wip]
      scene/            build the starting world.                                    [wip]
      devapi/           game-owned DevAPI commands only; engine groups are wired in CMake/main. [wip]
      build_packs.c     pack builder -> game.ntpack + generated asset-id header.
      game_audio.*, game_storage.*  seed infra.
    devapi/             game-local Python bots and runtime smoke scenarios.
    assets/shaders/     common/ + slug_text + sprite + mesh_inst.
    state/              game-state schema (codegen source).
    design/             game-owned concept, GDD, private knowledge base, and
                        structured design data scaffold.

Status: starter shell (empty scene + text). Build wiring (`CMakeLists.txt` referencing
`../../external/neotolis-engine`) and the full shell (settings panel + sliders + long-press
reset + GUI art, coloured + textured mesh examples, a character-walks example system)
are in progress — see epic E009.

Debug builds enable the engine DevAPI path by default (`GAME_DEVAPI_ENABLED=ON`):
`--devapi <port>` starts the engine TCP transport and exposes engine-owned
`endpoints`, `command.describe`, `frame/time`, `input`, `ui`, `obs`, and
`capture.*` groups, plus the template-owned `game.state` snapshot registered from
`src/devapi/game_state_devapi.c`. Release builds default this off. Game-specific
commands belong under `src/devapi/` when a copied game needs them.

Runtime bots and smoke scenarios live under top-level `devapi/`. Start with:

```powershell
cmake --build build/devapi-debug --target devapi_smoke
```

or from the repository root:

```powershell
py -3.12 templates/template/devapi/smoke_bot.py --exe templates/template/build/devapi-debug/bin/game.exe
```

For QCLR_002 responsive-viewport screenshots:

```powershell
cmake --build build/devapi-debug --target quality_responsive
```

Bots can import `devapi/responsive_viewports.py` or pass
`--scenario path.py:prepare_state` to capture the same viewport matrix after a
specific game moment. Use `game.result("game.state")` or `observe="game.state"`
when a scenario needs the live World/settings snapshot after an action.
