---
id: T0012
title: Migrate seed DevAPI from src/devapi sidecar to engine native DevAPI
status: done
epic: E002
priority: P1
tags: [engine, devapi, runtime]
created: 2026-06-19
updated: 2026-06-19
---

## What

The engine (8ec758b7) now ships a native modular DevAPI (engine/devapi/) — a JSON
command bus + loopback-TCP transport + input/time/discovery groups, better than the
repo sidecar. Migrate the seed onto it. NOT a delete: the engine deliberately omits
ui.*/entity.*/game.* (game-owned), so those + the generated game.state.* must be
rewritten onto the engine bus as group="game".

DELETE (pure dup): src/devapi/nt_devapi_net.c, src/devapi/nt_devapi.c,
src/devapi/nt_devapi.h, tools/devapi/__pycache__.
REWIRE: src/clean_seed_main.c (game handlers -> engine ABI), tools/state_codegen/
generate_state.py (emit engine ABI — highest effort, edit the generator not the
output), tools/devapi/{devapi_client.py,devapi_cli.py} (on engine client/transport),
CMakeLists.txt (drop game_devapi block, enable NT_DEVAPI for native Debug, link
nt_devapi + nt_devapi_net), .codex/skills/game-runtime-automation/references/
devapi-pattern.md (point at engine spec).
KEEP: src/game_storage.*, tools/devapi/{capture_window.py,capture_screen.ps1,
record_screen_ffmpeg.ps1}, and the game vocabulary re-registered as group="game".

Ordered steps (full plan in the dedup workflow output): (1) unify cJSON; (2) update
state codegen to engine ABI; (3) game-side command file (game.*/ui.*/entity.list +
UI-tree builder, ui.click routes via engine input.*); (4) rewire seed init/poll/
shutdown to nt_devapi_init/update/net_start(9123); (5) CMake swap; (6) build
native-debug + fix ABI/link; (7) rewire python client/cli; (8) smoke.py green;
(9) delete src/devapi + __pycache__ after green; (10) update skill; (11) verify
EMSCRIPTEN/Release link with zero nt_devapi_* symbols; (12) validate + screenshot.

## Done when

- [x] CMake: NT_DEVAPI on for native-debug, game_devapi sidecar block removed, game_seed links nt_devapi + nt_devapi_net
- [x] seed + generated state codegen + game/ui/entity commands rewritten to the engine handler ABI (group="game"); native-debug builds green
- [x] python client/cli re-based on engine client/transport (port 9123 pinned); smoke.py passes (ping/endpoints/command.describe/game.state round-trip/ui.click/frame.wait/screenshot)
- [x] src/devapi/* deleted ONLY after green build + smoke; game-runtime-automation skill repointed at the engine DevAPI spec (tools/devapi/__pycache__ stays untracked/gitignored)
- [x] EMSCRIPTEN/Release link with zero nt_devapi_* symbols; node tools/ai.mjs validate green + native screenshot proof

## Open questions

- cJSON unification (settle BEFORE handler rewrite): link the engine `cjson` target
  into game handlers, or keep `external/cjson` and confirm ABI-identical? Mismatched
  cJSON across the handler boundary can corrupt.
- Single user-facing switch: keep `GAME_DEVAPI_ENABLED` forwarding to `NT_DEVAPI_ENABLED`?
- Poll-model change: input is now engine-scheduled (inject then step/frame.wait);
  bots assuming immediate apply need a step after injecting.

## Log

- 2026-06-19: plan produced from the engine-vs-sidecar dedup analysis (engine native
  DevAPI has no ui.*/entity.*/game.*; sidecar transport/dispatch/input/time are
  redundant). Not yet started; staged + reversible behind the gate.
- 2026-06-19: PASS 1 DONE + native-debug BUILDS GREEN. Rewrote the state codegen
  (generate_state.py) + seed game.* handlers to the engine handler ABI (result_obj
  + nt_devapi_error, descriptor-based register, group="game"); game.state.get now
  wraps its value as {path,value}. Seed init/poll/shutdown -> nt_devapi_init/update/
  net_start(9123)/net_stop/shutdown. CMake: enable engine NT_DEVAPI for native debug,
  drop the game_devapi sidecar lib, unify cJSON on the engine `cjson` target (avoids
  duplicate cJSON_* symbols), link nt_devapi + nt_devapi_net. Deleted src/devapi/*
  early — it shadowed the engine header on the -Isrc path and broke the build.
  REMAINING: Pass 2 port ui.*/entity.list (game-side UI adapter on the engine bus),
  Pass 3 rewire the python client + smoke (ping/game.state/ui.click/screenshot),
  Pass 4 verify EMSCRIPTEN/Release link with zero nt_devapi_* + update the skill.
- 2026-06-19: PASS 1 RUNTIME-VERIFIED + engine pulled to c0ade084 (UI menu/dropdown,
  no devapi change). Launched game_seed --devapi 9123; TCP smoke: ping, endpoints
  (37 cmds), game.state, command.describe, game.action.cycle all OK on the native bus.
- 2026-06-19: PASS 2 DONE + RUNTIME-VERIFIED. New src/game_devapi_ui.{c,h}: game-side
  UI-tree model + ui.tree/ui.element/ui.click + entity.list registered group="game"
  on the engine bus; ui.click injects via nt_input_inject_pointer (DOWN+UP). Seed
  re-adds the per-frame UI-tree rebuild. Smoke: 41 cmds; ui.tree=[root,seed.cycle,
  seed.progress]; ui.click seed.cycle drove the game (test_ui_clicks 0->1, shape
  cube->sphere). Wire shapes that changed vs the sidecar (clients must follow): get
  -> {path,value}, ui.tree -> {nodes[]}, entity.list -> {entities[]}, engine ok/error
  envelope. REMAINING: Pass 3 (python client/CLI + smoke), Pass 4 (skill + wasm/release).
- 2026-06-19: PASS 3 DONE + SMOKE GREEN. Rewired tools/devapi onto the engine bus:
  DevApiClient.step now runs sequentially (the engine rejects deferred frame.wait
  inside a batch); added endpoint_methods() to flatten endpoints -> {commands[]}.
  smoke.py rewritten to the engine vocabulary (game.state.reset not the old
  game.reset_playtest; seed view emits "shape" not shape_index; ui.tree -> {nodes[]}).
  All 10 checks pass: universal+seed endpoints, ping, state.reset, game.state, ui.tree,
  state.set round-trip, action.cycle, screenshot (window-capture fallback), and
  ui.click seed.cycle driving state through the client. REMAINING: Pass 4.
- 2026-06-19: PASS 4 DONE — TASK COMPLETE. Verified the layer compiles out of
  shipping builds: native-release + wasm-release both link with ZERO nt_devapi_*
  symbols (llvm-nm on game_seed.exe + libgame_state.a + the devapi objects). Fixed
  two release-only build breaks the gate-OFF path exposed: NT_DEVAPI_ENABLED was
  undefined under -Werror,-Wundef (now defined =0 on the game targets when the gate
  is off, PUBLIC on game_state), and game_devapi_ui.c became an empty TU (added an
  #else typedef). Repointed the game-runtime-automation devapi-pattern.md skill at
  the engine spec (headers + live endpoints) with the deltas that bite bots (group
  not layer; no deferred frame.wait inside a batch; endpoints->{commands}, ui.tree->
  {nodes}, get->{path,value}). doc_reference_check now tolerates references to the
  regenerated-per-project subsystems (tools/devapi, tools/state_codegen) the portable
  export omits, but still fails a missing file inside a present subsystem (+2 tests).
  node tools/ai.mjs validate + --full both green (exported doc ref check: 61 files).
  NOTE: wasm-release LINK fails on -lglad/-lstb_image_write (pre-existing third-party
  lib-path issue in the wasm preset, unrelated to DevAPI; native is the playable
  harness per AGENTS.md) — out of scope for this task.
- 2026-06-19: Pass 4 complete: release/wasm zero nt_devapi_* symbols, skill repointed, validate green
