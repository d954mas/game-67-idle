# Template (game starter)

The minimal runnable game every new game is copied from. Spec + reuse model:
`../../ai_studio/bootstrap/TEMPLATE.md`.

Layout (decomposition the template teaches by example — no god-file):

    src/
      main.c            THE CONDUCTOR: init -> nt_app_run(frame) -> teardown; frame()
                        only calls subsystems. No game logic here.
      world/world.h     the World: single source of truth (entity handles, sim state).
      ui/hud.{c,h}      on-screen text/UI (its own system).
      render/           render systems (setup + draw), separate from game logic.   [wip]
      systems/          game systems, one file each (input, camera, character move). [wip]
      scene/            build the starting world.                                    [wip]
      devapi/           DevAPI commands (state/reset/capture), not in main.          [wip]
      build_packs.c     pack builder -> game.ntpack + generated asset-id header.
      game_audio.*, game_devapi_ui.*, game_storage.*  seed infra.
    assets/shaders/     common/ + slug_text + sprite + mesh_inst.
    state/              game-state schema (codegen source).

Status: starter shell (empty scene + text). Build wiring (`CMakeLists.txt` referencing
`../../external/neotolis-engine`) and the full shell (settings panel + sliders + long-press
reset + GUI art, coloured + textured mesh examples, a character-walks example system)
are in progress — see epic E009.
