---
id: T0001
title: Mine Cards Mining v0.01 first slice
status: dropped
epic: E001
priority: P1
tags: [mine-cards, prototype, native, visual, core-loop]
created: 2026-06-17
updated: 2026-06-18
---

## What

Build the first native Mining activity screen after the v0.01 parameters and
3D miner source path are locked.

Scope:

- one Mining skill;
- node picker;
- progress bar and timer;
- reward log;
- geode/rich-vein event callout or auto-collect feedback;
- pickaxe upgrade with exact missing cost and before/after speed;
- one modular 3D miner using real mesh assets;
- simple idle/mining transform animation or explicit scoped deferral recorded
  before implementation starts.
- use the reusable skeletal animation extension if it remains stable for the
  first Mining screen; otherwise record a bounded fallback before implementation
  starts.

Out of scope:

- card runs;
- combat;
- smithing/equipment grids;
- full offline progression;
- web prototype.

Dependencies:

- T0002 locks the first-session parameters.
- T0003 chooses the first public-safe 3D miner asset path.
- T0005 selects and proves the reusable skeletal animation extension path.
  T0001 should use it for the first Mining screen unless renderer/asset-path
  integration blocks, in which case the fallback must be explicit and bounded.

## Done when

- [x] T0002 is done or records that current parameters are accepted as-is.
- [x] T0003 is done or records an explicit deferral to a temporary blockout GLB.
- [x] Native first screen shows Mining progress, rewards, node selection, and
      pickaxe upgrade using real assets through the engine asset path.
- [x] Native first screen includes a modular 3D miner with simple idle/mining
      transform animation or records an explicit scoped deferral.
- [x] Native first screen either uses `extensions/skeletal_animation/` for the
      miner/tool motion or records a bounded fallback with the blocking evidence.
- [x] Native screenshot is captured and judged against
      `gamedesign/projects/mine-cards/visual/fake_shots/mining_v001_fake_shot_2026-06-17.png`.
- [x] Landscape and portrait are reviewed as separate authored compositions:
      landscape as a wide action strip plus mechanics board, portrait as a
      top hero action stage plus vertical idle loop.
- [x] Four-shot responsive proof is captured: landscape idle, landscape
      reward/geode, portrait idle, and portrait reward/geode.
- [x] UI readability zoom gate is run on the native screenshot.
- [x] Product/game-loop gate confirms a new player can tell what is running,
      what grows, what to upgrade, and why to continue.

## Open questions

- Does the native screen start from `src/clean_seed_main.c` or a new
  Mine Cards-specific entrypoint under `src/`?
- Is the first proof allowed to use temporary procedural GLB parts if T0003
  cannot produce a polished model quickly?

## Log

- 2026-06-17: Melvor-like pivot captured in
  `gamedesign/projects/mine-cards/`; cards removed from v0.01; Mining fake shot
  saved at
  `gamedesign/projects/mine-cards/visual/fake_shots/mining_v001_fake_shot_2026-06-17.png`.
- 2026-06-17: Lead confirmed the direction. Base GDD pass added
  `gdd.md`, `core_loop.md`, `parameters.md`, and `data/parameters.json` for the
  Mining-only v0.01 foundation.
- 2026-06-17: Added `systems_foundation.md` and
  `data/systems_registry.json` so future mechanics/synergies can expand from
  the Mining base without redefining v0.01.
- 2026-06-17: Added `visual/3d_character_direction.md`: use a living modular 3D
  voxel miner as a differentiator; start with transform-driven mesh-part
  animation, not skeletal animation.
- 2026-06-17: Added `visual/animation_runtime_options.md`: sidecar animation
  options compared; recommendation is mesh-part animation first, then
  Blockbench/cgltf/ozz spike later if needed.
- 2026-06-17: Base review split balance, asset path, and skeletal animation
  spike into separate tasks so this task can stay focused on the native Mining
  screen.
- 2026-06-17: T0005 proved the production skeletal path as a reusable extension
  connected to `game_seed`. First Mining screen should now plan around the
  extension path first, with mesh-part fallback only if real asset-path
  integration blocks.
- 2026-06-17: Implementation started in `src/clean_seed_main.c`: native Mining
  screen, Ozz-driven KayKit miner, text pack, node selection, mining ticks,
  reward log, and Copper Pickaxe interaction are wired in `game_seed`.
- 2026-06-17: Lead requested the whole game use Y-up coordinates. Updated the
  first screen so game layout, shape rendering, text rendering, and hit testing
  use Y-up internally; DevAPI UI bounds are converted back to top-left screen
  coordinates for automation.
- 2026-06-17: Y-up evidence: native build passed, screenshot captured at
  `build/captures/mine_cards_mining_v001_yup_first_screen.png`, and DevAPI
  click on `mining.node.copper` after Mining Lv2 selected `copper_vein`
  (`interval_seconds = 5`). UI readability audit produced warnings and montage
  `build/captures/mine_cards_mining_v001_yup_first_screen_uizoom.png`, so this
  task is still not done.
- 2026-06-17: Lead redirected the first screen toward a reduced Capybara Go-like
  layout: fixed top character/action stage and dense idle mechanics below.
  Added reference note
  `gamedesign/projects/mine-cards/references/capybara_go_layout_reference_2026-06-17.md`.
- 2026-06-17: Native Capybara-like baseline captured at
  `build/captures/mine_cards_capybara_layout_v005.png` and
  `build/captures/mine_cards_capybara_layout_v005_720x480.png`. Readability
  audit passed with montage
  `build/captures/mine_cards_capybara_layout_v005_uizoom.png`. This is the new
  composition baseline, not task completion: live-state coverage, product gate,
  and real UI asset pass remain open.
- 2026-06-17: Added explicit runtime UI asset pack path:
  `tools/assets/build_mine_cards_ui_pack.c` builds
  `assets/mine_cards_ui.ntpack` with slice9 panel/tab/card/button/progress
  atlas regions, then `src/clean_seed_main.c` draws the main UI panels through
  `nt_sprite_renderer_emit_slice9`. Manifest:
  `gamedesign/projects/mine-cards/visual/runtime_ui/mine_cards_ui_asset_manifest.json`.
  This is a procedural runtime scaffold to get off debug-only shape panels, not
  the final generated/artist UI kit.
- 2026-06-17: Runtime UI asset evidence captured at
  `build/captures/mine_cards_ui_assets_v002.png` and
  `build/captures/mine_cards_ui_assets_v002_720x480.png`; readability pass wrote
  `build/captures/mine_cards_ui_assets_v002_uizoom.png`. Remaining shape
  renderer use: screen bars/background, rock marker, and transient callout
  shapes.
- 2026-06-17: Extended the runtime UI pack to 20 regions, including screen
  background, top/bottom bars, callout, lock badge, and target marker assets.
  The normal asset-ready path now draws the first screen UI through
  `nt_sprite_renderer`; `nt_shape_renderer` remains fallback only while the pack
  is unavailable. Evidence:
  `build/captures/mine_cards_ui_assets_v008.png`,
  `build/captures/mine_cards_ui_assets_v008_720x480.png`, and readability
  montage `build/captures/mine_cards_ui_assets_v008_uizoom.png`. This is still
  not final art: the stage target uses a placeholder sprite and must be replaced
  by real stone/copper source art.
- 2026-06-17: Refreshed the Capybara-like runtime proof with a fixed top miner
  action stage, visible future activity tabs below, and framebuffer-based native
  screenshots after Windows BitBlt capture returned a white OpenGL client area.
  Added game-local DevAPI endpoint `game.capture.framebuffer` for reliable GL
  backbuffer proof. Evidence:
  `build/captures/mine_cards_runtime_framebuffer_v011.png`,
  `build/captures/mine_cards_runtime_framebuffer_v011_720x480.png`,
  `build/captures/mine_cards_runtime_framebuffer_v011_uizoom.png`, and
  `build/captures/mine_cards_runtime_framebuffer_v011_720x480_uizoom.png`.
- 2026-06-17: Added a runtime screen review after lead feedback that the screen
  still read as a demo, not a game:
  `gamedesign/projects/mine-cards/reviews/runtime_screen_review_2026-06-17.md`.
  Fixed the false asset-ready assumption by resolving UI atlas regions through
  generated normalized region hashes; `game.state` now reports
  `ui_assets_ready=true` and `ui_regions_ready=true`. Added accepted generated
  stone/copper node sprites to the game-local UI pack and proved the stone
  target in both orientations:
  `build/captures/mine_cards_layout_review_v006_landscape_surface.png` and
  `build/captures/mine_cards_layout_review_v006_portrait_surface.png`. This is
  a direction proof, not task completion: action-stage contact, callout/progress
  placement, live-state coverage, readability, and product/game-loop gate remain
  open.
- 2026-06-17: Improved the top action stage contact: target placement now follows
  the actual KayKit/Ozz pickaxe swing direction, progress is lower/thinner, and
  copper-state callout is separated from the actor/progress strip. Evidence:
  `build/captures/mine_cards_action_contact_v003_landscape_surface.png`,
  `build/captures/mine_cards_action_contact_v003_landscape_copper_callout.png`,
  `build/captures/mine_cards_action_contact_v003_portrait_surface.png`, plus
  before/after readability montages
  `build/captures/mine_cards_action_contact_v003_landscape_surface_uizoom_cmp.png`
  and
  `build/captures/mine_cards_action_contact_v003_portrait_surface_uizoom_cmp.png`.
  Still open: impact feedback, live-state matrix coverage, and product/game-loop
  gate.
- 2026-06-17: Added stage ownership and hit-feedback pass after lead review that
  the screen still was not a game and the character was not tied to UI. The
  KayKit/Ozz miner now renders inside a game-side miner-lane viewport owned by
  the top action panel, reward hit feedback is exposed in `game.state`, and
  landscape/portrait proof captures show normal and geode moments:
  `build/captures/mine_cards_layout_review_v008_landscape_surface.png`,
  `build/captures/mine_cards_layout_review_v008_landscape_geode.png`,
  `build/captures/mine_cards_layout_review_v008_portrait_surface.png`, and
  `build/captures/mine_cards_layout_review_v008_portrait_geode.png`. Readability
  compare montages passed without text-on-bright regression. Still open: lower
  board hierarchy, stronger selected-activity linkage, live-state matrix, and
  product/game-loop gate.
- 2026-06-17: Improved lower mechanics board hierarchy: `MINING ACTIVE` now
  dominates future activity chips, node cards show running/locked/yield state,
  and the Copper Pickaxe panel is framed as `NEXT GOAL`. Evidence:
  `build/captures/mine_cards_board_hierarchy_v002_landscape_surface.png`,
  `build/captures/mine_cards_board_hierarchy_v002_landscape_geode.png`,
  `build/captures/mine_cards_board_hierarchy_v002_portrait_surface.png`, and
  `build/captures/mine_cards_board_hierarchy_v002_portrait_geode.png`; compare
  montages:
  `build/captures/mine_cards_board_hierarchy_v002_landscape_geode_uizoom_cmp.png`
  and
  `build/captures/mine_cards_board_hierarchy_v002_portrait_geode_uizoom_cmp.png`.
  Product gate recorded a strict fail at
  `gamedesign/projects/mine-cards/reviews/product_gate_board_hierarchy_v002_2026-06-17.md`.
  Still open: landscape board breathing room, readable affordability explanation,
  future activity icons/silhouettes, and live-state matrix coverage.
- 2026-06-17: Added partial live-state matrix proof for upgrade states. Evidence:
  `build/captures/mine_cards_live_state_v002_upgrade_unaffordable.png`,
  `build/captures/mine_cards_live_state_v002_upgrade_affordable.png`,
  `build/captures/mine_cards_live_state_v002_upgrade_purchased.png`,
  `build/captures/mine_cards_live_state_v002_upgrade_unaffordable_portrait.png`,
  and `build/captures/mine_cards_live_state_matrix_v002.json`. Updated
  `gamedesign/projects/mine-cards/visual/live_state_acceptance_matrix.json` to
  the product-gate-compatible shape. Still open: missing-cost text is too small
  for final product-read pass and coverage uses accelerated DevAPI setup.
- 2026-06-17: Improved lower-board affordance with larger upgrade resource rows
  and placeholder atlas markers for future activity chips. Evidence:
  `build/captures/mine_cards_affordance_v001_landscape_surface.png`,
  `build/captures/mine_cards_affordance_v001_portrait_surface.png`,
  `build/captures/mine_cards_affordance_v001_upgrade_unaffordable.png`,
  `build/captures/mine_cards_affordance_v001_upgrade_affordable.png`, and
  `build/captures/mine_cards_affordance_v001_upgrade_purchased.png`. Product
  gate improved but still failed at
  `gamedesign/projects/mine-cards/reviews/product_gate_affordance_v001_2026-06-17.md`
  because art quality/future activity silhouettes are still placeholder-level.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_stage_review_v004_2026-06-17.md; screenshot: build/captures/mine_cards_stage_review_v004_landscape_surface.png; next: Freeze mechanic expansion. Produce a proper composition rescue: reference-backed stage layout, generated/artist UI/icon family, and native landscape+portrait screenshot proof before adding mechanics.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_stage_rescue_v007_2026-06-17.md; screenshot: build/captures/mine_cards_stage_rescue_v007_landscape_surface.png; next: Stop numeric layout tuning. Create the generated/artist UI and stage/icon source family, then integrate it into the native screen and rerun landscape/portrait product gate.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_stage_rescue_v008_2026-06-17.md; screenshot: build/captures/mine_cards_stage_rescue_v008_landscape_surface.png; next: Use art_requests/mine-cards-stage-ui-family-v001.json to generate/accept source sheets, then integrate the smallest stage/icon proof into the native screen and rerun landscape/portrait product gate.
- 2026-06-17: Added explicit visual director review after lead feedback that the
  screen still reads as repeated boxes and the character is not tied strongly
  enough to the UI:
  `gamedesign/projects/mine-cards/reviews/visual_director_review_stage_rescue_v008_2026-06-17.md`.
  Next iteration must treat landscape and portrait as separate authored
  compositions and prove actor -> rock -> reward/progress -> next upgrade before
  mechanics expand.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign\projects\mine-cards\reviews\product_gate_icons_runtime_v002_2026-06-17.md; screenshot: build/captures/mine_cards_icons_runtime_v002_landscape_surface.png; next: Keep feature expansion frozen. Use the accepted icon family in the next authored responsive layout pass, then generate/integrate stage background/FX source art and recapture four-shot proof.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_stage_anchor_v011_2026-06-17.md; screenshot: build/captures/mine_cards_stage_anchor_v011_landscape_surface.png; next: Generate or accept the stage background/floor/reward FX source sheet, integrate the smallest stage-art proof, then rerun landscape and portrait product gate before adding mechanics.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_stage_art_v002_2026-06-17.md; screenshot: build/captures/mine_cards_stage_art_v002_landscape_surface.png; next: Keep feature expansion frozen. Generate or accept a sprite/FX sheet for stone hit/geode reward, reduce lantern/stage visual dominance, and redesign lower-board hierarchy before adding mechanics.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_stage_fx_v003_2026-06-18.md; screenshot: build/captures/mine_cards_stage_fx_v003_landscape_geode.png; next: Keep feature expansion frozen. Generate or accept blank UI kit/decor overlay source families for board/card/button treatment, then recapture four-shot proof and rerun product gate.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_blank_ui_v004_2026-06-18.md; screenshot: build/captures/mine_cards_blank_ui_v004_landscape_surface.png; next: Generate or derive compact button/chip/card variants from the blank UI family, integrate them into the small lower-board controls, then recapture four-shot proof and rerun product gate.
- 2026-06-17: product gate FAIL (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_compact_ui_v001_2026-06-18.md; screenshot: build/captures/mine_cards_compact_ui_v001_landscape_surface.png; next: Keep the saved/cut compact sheet as source evidence. Create a calmer compact runtime variant: use generated art for selected/primary states, derive simpler idle rows/tabs from the same source or generate a low-ornament compact sheet, then recapture landscape/portrait and rerun product gate.
- 2026-06-17: product gate PASS (responsive); review: gamedesign/projects/mine-cards/reviews/product_gate_compact_ui_v002_2026-06-18.md; screenshot: build/captures/mine_cards_compact_ui_v002_landscape_surface.png; next: continue to the next narrow slice
- 2026-06-17: product gate PASS (core-moment-motion); review: gamedesign/projects/mine-cards/reviews/core_moment_mining_v004_2026-06-18.md; screenshot: build/captures/mine_cards_core_moment_v004_sheet.png; next: continue to the next narrow slice
- 2026-06-18: Core mining moment is now progress-synced instead of free-looped:
  `mining_animation_sample_time()` drives the Ozz/KayKit pose from the active
  mining interval and hit-feedback window, the actor/target framing was adjusted
  so the fixed stage reads as miner -> rock -> reward, and the pickaxe attachment
  was enlarged/lightened for the dark stage. Evidence:
  `build/captures/mine_cards_core_moment_v004_sheet.png` and
  `build/captures/mine_cards_core_moment_v004.gif`. Caveat: this is a motion
  and production-path proof, not final custom Mine Cards character art.
- 2026-06-17: product gate PASS (responsive-v003); review: gamedesign/projects/mine-cards/reviews/product_gate_compact_ui_v003_2026-06-18.md; screenshot: build/captures/mine_cards_compact_ui_v003_landscape_surface.png; next: continue to the next narrow slice
- 2026-06-18: T0002/T0003/T0005/T0006 review dependencies closed as done.
  Current T0001 gate evidence: four-shot proof
  `build/captures/mine_cards_compact_ui_v003_landscape_surface.png`,
  `build/captures/mine_cards_compact_ui_v003_landscape_geode.png`,
  `build/captures/mine_cards_compact_ui_v003_portrait_surface.png`, and
  `build/captures/mine_cards_compact_ui_v003_portrait_geode.png`; readability
  compare montages
  `build/captures/mine_cards_compact_ui_v003_landscape_surface_uizoom_cmp.png`
  and
  `build/captures/mine_cards_compact_ui_v003_portrait_surface_uizoom_cmp.png`;
  strict responsive gate
  `gamedesign/projects/mine-cards/reviews/product_gate_compact_ui_v003_2026-06-18.md`.
  Still open before closeout: refresh live-state scenarios for copper unlock,
  upgrade affordable, upgrade purchased, and 720x480 stress.
- 2026-06-18: T0001 closeout gate passed with full live-state matrix coverage.
  Evidence: `gamedesign/projects/mine-cards/visual/live_state_acceptance_matrix.json`,
  `build/captures/mine_cards_live_state_v003_state.json`,
  `build/captures/mine_cards_live_state_v003_copper_unlocked.png`,
  `build/captures/mine_cards_live_state_v003_upgrade_affordable.png`,
  `build/captures/mine_cards_live_state_v003_upgrade_purchased.png`,
  `build/captures/mine_cards_live_state_v003_small_window_stress.png`, and
  `gamedesign/projects/mine-cards/reviews/product_gate_t0001_closeout_v001_2026-06-18.md`.
  Ready for lead review; not marked done until accepted.
- 2026-06-18: Pre-review slice hygiene ran and failed only on broad diff size:
  `build/captures/mine_cards_t0001_slice_hygiene.md` reports 51 changed paths
  over the 30-file normal-slice threshold. Build/probe/product-gate/screenshot
  evidence is present; no snapshot override was used.
- 2026-06-18: Review cleanup reran slice hygiene with an explicit
  end-of-experiment snapshot. New evidence:
  `build/captures/mine_cards_t0001_slice_hygiene_snapshot.md` is WARN, not
  FAIL; the only warnings are accepted broad diff size and missing profiler
  guard, which is advisory for normal game work.
- 2026-06-18: Lead-review packet prepared:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_packet_2026-06-18.md`
  with overview sheet
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_sheet_2026-06-18.png`.
  T0001 remains in review until the lead accepts or names a rejection axis.
- 2026-06-18: Added acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0001_acceptance_audit_2026-06-18.md`.
  Agent recommendation is accept as first Mining baseline if the lead accepts
  temporary KayKit/Ozz character art and broad snapshot evidence.
- 2026-06-17: product gate PASS (responsive-v003-closeout); review: gamedesign/projects/mine-cards/reviews/product_gate_t0001_closeout_v001_2026-06-18.md; screenshot: build/captures/mine_cards_compact_ui_v003_landscape_surface.png; next: continue to the next narrow slice
- 2026-06-18: Added unified lead review board:
  `gamedesign/projects/mine-cards/reviews/lead_review_board_2026-06-18.md`.
  T0001 remains in review until the lead accepts the baseline or names exactly
  one rejection axis.
- 2026-06-18: Lead rejected T0001 for PC usability: the screen is too small and
  unclear on a real PC window. Root cause: Mine Cards UI is authored directly in
  raw framebuffer pixels with fixed font sizes/compact thresholds instead of the
  engine `nt_ui_scale` reference-resolution contract. Runtime crash also found
  and mitigated: non-positive slice9/icon dimensions are now skipped before
  calling the renderer. Evidence and next gate:
  `gamedesign/projects/mine-cards/reviews/t0001_ui_scale_rejection_2026-06-18.md`,
  `build/captures/mine_cards_crash_fix_640x360.png`, and
  `build/captures/mine_cards_crash_fix_640x360_uizoom.png`. T0001 is back in
  `doing`; feature/mechanic expansion remains frozen until the UI scale path is
  fixed and recaptured.
- 2026-06-18: UI scale fix pass 1 implemented: `game_seed` links `nt_ui`,
  `src/clean_seed_main.c` computes `nt_ui_scale_t` each frame, authored UI now
  lays out in a `960x540` logical reference viewport, pointer input is mapped
  through `nt_ui_scale_apply_pointer`, 3D actor box is converted back to
  physical framebuffer pixels, and default native window is `1280x720`. Build
  passed. Evidence:
  `build/captures/mine_cards_nt_ui_scale_1280x720_window.png` and
  `build/captures/mine_cards_nt_ui_scale_1280x720_window_uizoom.png`. Still not
  ready for review: lower-board composition/clipping needs a focused layout pass.
- 2026-06-18: Lead rejected the scaled PC screen for missing focus: unclear
  current location, too many pseudo-buttons, and no obvious action hierarchy.
  Focus pass 2 keeps one active lane (`1. MINING NOW`), turns future skills into
  muted `LATER` text instead of buttons, removes the inactive bottom `SKILLS`
  pseudo-tab, and only renders/enables the Copper Pickaxe button when it is
  actually affordable. Build and readability passed. Evidence:
  `build/captures/mine_cards_focus_v002_1280x720.png`,
  `build/captures/mine_cards_focus_v002_1280x720_uizoom.png`, and
  `build/captures/mine_cards_focus_v002_1280x720_uizoom_cmp.png`.
- 2026-06-18: Closed by lead as a pipeline test run, not an active game to continue. Keep evidence for reusable pipeline lessons; do not expand Mine Cards mechanics.
