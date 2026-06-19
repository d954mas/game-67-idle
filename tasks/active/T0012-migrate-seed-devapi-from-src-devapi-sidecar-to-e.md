---
id: T0012
title: Migrate seed DevAPI from src/devapi sidecar to engine native DevAPI
status: backlog
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

- [ ] CMake: NT_DEVAPI on for native-debug, game_devapi sidecar block removed, game_seed links nt_devapi + nt_devapi_net
- [ ] seed + generated state codegen + game/ui/entity commands rewritten to the engine handler ABI (group="game"); native-debug builds green
- [ ] python client/cli re-based on engine client/transport (port 9123 pinned); smoke.py passes (ping/endpoints/command.describe/game.state round-trip/ui.click/frame.wait/screenshot)
- [ ] src/devapi/* + tools/devapi/__pycache__ deleted ONLY after green build + smoke; game-runtime-automation skill repointed at the engine DevAPI spec
- [ ] EMSCRIPTEN/Release link with zero nt_devapi_* symbols; node tools/ai.mjs validate green + native screenshot proof

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
