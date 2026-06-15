# Project Status

Short live project-status index. Workflow rules live in `tasks/README.md`.

## Current Goal

Build an original Roblox-like casual 3D fishing game: research references,
discuss the direction with the lead, write the full concept/GDD, then implement
a native PC playable prototype with bright juicy visuals and passive workflow
profiling.

## Active Product State

- Active game concept: `roblox-fishing` / working title `Splash Rods`.
- Active project wiki: `gamedesign/projects/roblox-fishing/`.
- Active runtime target: native PC first; current source now contains a playable
  Splash Rods fishing slice with perspective/depth 3D scene, generated UI/icon
  texture assets, and a native GLTF/GLB mesh-pack path through the engine
  builder/resource/material/mesh-renderer pipeline.
- Fishing task queue: `E002` active; `T0009` done/archived; `T0008` review;
  `T0011` review; `T0010` review for technical playable proof; `T0012`
  doing as the P0 visual/product rescue after lead rejection.
- Older Rune Marches tasks may still appear in the global taskboard summary;
  they are not the active goal for this thread.
- Profiling: passive profiling is part of the task. Session notes begin at
  `tmp/roblox_fishing_profile.md`; use `node tools/ai.mjs` facade for long or
  repeated commands where practical.

## Source Pointers

- Start here: `AGENTS.md`, `AI_PIPELINE.md`,
  `AI_PIPELINE_SESSION_PROFILING.md`.
- Concept draft: `gamedesign/projects/roblox-fishing/concept.md`.
- Reference study:
  `gamedesign/projects/roblox-fishing/references/fishing_reference_study.md`.
- Visual direction brief:
  `gamedesign/projects/roblox-fishing/art/visual_direction_brief.md`.
- GDD:
  `gamedesign/projects/roblox-fishing/gdd.md`.
- First fake shot:
  `gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png`.
- Implementation handoff draft:
  `gamedesign/projects/roblox-fishing/game_implementation_plan.md`.
- Runtime source: `src/` after GDD/implementation gate.
- Task epic: `tasks/epics/E002-roblox-like-casual-3d-fishing-prototype.md`.

## Current Evidence

- Research sources checked on 2026-06-15:
  Fisch Roblox page, Fishing Simulator Roblox page, WEBFISHING Steam page,
  Russian Fishing 4 Steam page, PC Gamer current Fisch guide, and Kenney asset
  pages for nature, watercraft, blocky characters, and fish pack.
- Lead direction accepted on 2026-06-15: casual audience, strong
  progression/grind, simple gameplay, feel and fake shot are important,
  progression clarity matters, realism is forbidden.
- Reference study status: ready enough for first native prototype after fake
  shot review. It includes a screenshot walkthrough/evidence board and current
  native mismatch capture.
- First fake shot exists and needs lead review:
  `gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png`.
- Runtime generated UI proof exists:
  `gamedesign/projects/roblox-fishing/art/source_sheets/splash-rods-ui-icons-source-v2-magenta-clean.png`
  -> `assets/runtime/roblox-fishing-ui-v1/` -> native HUD/buttons/icons.
- Runtime GLTF/GLB model pipeline proof exists:
  `tools/roblox_fishing/generate_model_sources.py` emits first-pass low-poly
  fish/boat/shop-sign/palm-leaf/bobber source models under
  `gamedesign/projects/roblox-fishing/art/models/`, and
  `tools/roblox_fishing/build_packs.c` packs them plus the engine `cube.glb`
  into `build/game_seed/native-debug/assets/roblox_fishing_models.ntpack`.
  `src/main.c` loads the pack through an absolute native build path and renders
  16 GLTF/GLB-backed props through `nt_mesh_renderer`; the playtest probe now
  fails if mesh instances/draw groups are zero.
- Current native visual/product gate is red:
  `gamedesign/projects/roblox-fishing/reviews/product_read_gate_latest.json`.
  The durable failure report is
  `gamedesign/projects/roblox-fishing/reviews/visual_product_failure_report_2026-06-15.md`.
  The next screenshot contract is
  `gamedesign/projects/roblox-fishing/art/visual_rescue_screen_contract_v1.md`.
- Machine-readable draft contracts exist:
  `gamedesign/projects/roblox-fishing/data/balance.json`,
  `gamedesign/projects/roblox-fishing/data/ui_flow.json`, and
  `gamedesign/projects/roblox-fishing/data/game_asset_manifest.json`.
- Current native mismatch capture:
  `tmp/roblox_fishing/current_native_before_fishing.png`; it shows the current
  Rune Marches RPG/map/combat screen, not a 3D fishing scene.

## Blocking Work

- Lead rejected the current native screenshot as visually bad. Feature
  expansion is frozen under `T0012` until the native visual rescue screenshot
  passes a product-read gate. Do not treat mesh/UI technical proof as visual
  acceptance.
- Generated UI is wired as a first runtime slice, but the current source sheet
  is still a prototype/partial crop family; a final UI kit pass should separate
  buttons, panels, meter, icons, and decor with cleaner gutters and no key-color
  conflicts.
- External CC0 fishing/location model integration is not wired yet. The engine
  and builder support GLTF/GLB mesh packs, and the game now has generated
  low-poly source-model props in the pack; the next model pass should replace
  rough first-pass generated meshes with selected/final low-poly fishing,
  island, shop, and character assets.

## Current Gate

- Gate: native visual/product rescue.
- Status: first playable proof exists technically, but the current screenshot
  fails the visual/product bar. `T0012` must produce a stronger fake-shot-aligned
  native screenshot before more feature/content work.
- Current proof: `tmp/roblox_fishing/native_first_slice.png` shows native 3D
  water/dock/avatar/rod/bobber/fish, generated HUD/buttons/icons, catch reward,
  currency/index/backpack, and first upgrade affordance, but this proof is
  explicitly rejected for product visuals.

## Next Priorities

1. Execute `T0012`: generate/select and integrate focal-area world/UI assets
   from `visual_rescue_screen_contract_v1.md`, then produce a new native
   screenshot that passes product-read gate.
2. Replace rough generated low-poly mesh props with selected/final fishing
   props: dock decor, shop props, boat/island pieces, fish silhouettes, and
   blocky character pieces.
3. Run a second generated UI-kit pass with cleaner source-family separation,
   then replace the prototype/partial crops.
4. Continue visual pass against fake shot: water polish, avatar silhouette,
   fish scale/readability, and UI clarity.

## Validation Policy

- Research/docs: run `node tools/taskboard/cli.mjs validate`.
- Visual/generated art: create source records, inspect outputs, and keep final
  assets in the project folder; no final art from named refs until the reference
  gate is ready or an exception is approved.
- Playable prototype: native PC build and screenshot/input proof first.
- Web/mobile/browser validation is out of scope unless the lead explicitly
  approves web work for this fishing project.
