---
id: T0023
title: Authored starter mech visual pass
status: review
epic: ""
priority: P0
tags: [implementation, visual, assets, 3d, mechs, native]
created: 2026-06-19
updated: 2026-06-19
---

## What

Replace the current cube-kitbashed starter mech presentation and plain prototype
world with a Roblox-like mech/toy-block visual pass while preserving the working
native harness and the proven `mech_builder_battler_mesh.ntpack` runtime route.
Combat, economy, and upgrade depth are deliberately paused until the mech and
world look good at first glance.

Scope boundaries:

- In scope: starter mech source/runtime asset, stronger modular silhouette,
  non-cube-pile mech readability, Roblox-like toy/block world, stylized studs
  surfaces, separate readable parts, idle/move/attack animation, weapon recoil,
  foot motion, body lean, toy-plastic/painted material language, stronger
  lighting/shader response, bevel/normal readability, shoulder rocket
  sockets/modules, screenshot proof, and strict visual product gate.
- Out of scope: web/mobile export, PvP, new economy systems, broad enemy
  roster, reward tuning, progression expansion, final monetization, exact
  reference UI copying, and full MechLab.

Design inputs:

- `gamedesign/projects/mech-builder-battler/design/visual_target_review_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/references/current_build_mismatch_audit_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/evidence/t0021_runtime_visual_review_2026-06-19.md`
- `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T16-05-21-117Z_desktop-battle.md`

## Done when

- [x] Native `game_seed` still builds and runs the first mech loop.
- [x] The starter mech no longer reads as cube-kitbashed at gameplay scale.
- [x] The world reads as an intentional Roblox-like block arena, not debug cubes.
- [x] Movement reads as a mech: clear facing, strafe/turn feedback, body weight,
      step motion, and no "only forward/back" feel.
- [x] Attack reads as a mech action: visible weapon recoil, muzzle flash/tracer,
      hit feedback, and heat/vent response.
- [x] The mech keeps visible modular slots, including shoulder rocket sockets
      before purchase and equipped rocket modules after purchase.
- [x] Materials/lighting show stronger painted metal, dark joints, rim/key
      separation, bevel/normal readability, and contact shadow grounding.
- [x] The hangar and rocket battle screenshots are recaptured and linked from
      evidence.
- [x] Strict visual product gate passes with `art_quality >= 4` and no major
      issue for starter mech model quality, or any failure is logged with the
      next corrective action.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Open questions

- Best asset route is open for implementation: procedural authored mesh,
  generated/kitbashed GLB, or permissively licensed model. The route must keep
  provenance clear and avoid protected reference silhouettes.

## Log

- 2026-06-19: Created from T0022 mismatch audit. Current blocker is not the
  runtime mesh/material path; that works. The blocker is hero-mech asset quality:
  the starter still reads as cube-kitbashed rather than authored.
- 2026-06-19: Lead visual review rejected the current direction: mech still
  looks bad, UI looks bad, and the handmade pixel `draw_text` font is
  unacceptable. Corrective action: use existing engine systems (`nt_font` and
  `nt_text_renderer`) instead of inventing a custom shape-font renderer, then
  re-review screenshots before any pass claim.
- 2026-06-19: Project convention pinned after lead correction: all game/world/UI
  projection and logical layout work must stay Y-up. Platform/window input that
  arrives Y-down is converted only at the boundary. Future UI text must use the
  engine font/text renderer and packed fonts; debug shape text must not be used
  for product screenshots.
- 2026-06-19: Lead clarified that current play still feels like a set of cubes,
  movement is boring/unclear, and mech quality is more important than adding
  battle/progression depth. Current corrective path: pause combat/economy
  expansion and make the mech itself beautiful, modular, animated, weighty, and
  supported by better material, lighting, and shader work.
- 2026-06-19: First hero-mech corrective pass implemented: 42 runtime mesh parts
  instead of 26, stronger hangar hero camera, Y-up UI/DevAPI click contract,
  engine font renderer, movement inertia/facing/walk cycle, body lean, weapon
  recoil, facing-aware muzzle/tracer origins, richer shader lighting, and
  recaptured smoke screenshots. Residual visual risk remains: silhouette and
  source model quality still need another art pass before calling the mech
  beautiful.
- 2026-06-19: Lead rejected the current direction as still not working and
  pivoted the art target to "mechs in a Roblox world." New interpretation for
  this native harness: Roblox-like toy/block world, intentional chunky modular
  mechs, bright readable materials, and playful block environment. Do not keep
  chasing realistic/complex mech fidelity before this style pass lands.
- 2026-06-19: Added Roblox-like runtime visual pass: bright green block
  baseplate, raised studs, stylized grass/leaf motifs, colored block props,
  toy-plastic shader tuning, and a chunkier block-form starter mech. Evidence:
  `gamedesign/projects/mech-builder-battler/evidence/t0023_roblox_like_visual_pass_2026-06-19.md`.
- 2026-06-19: Added `game-texture-generation` skill for standalone texture work
  only. It records generated/downloaded texture source, license/provenance, and
  whether the texture must tile. Atlas, trim-sheet, icon, and UI sheet work
  remains owned by existing UI/icon and asset-pipeline workflows.
- 2026-06-19: Corrected route after lead challenge: stopped relying only on the
  procedural mech and integrated a downloaded CC0 Poly Pizza/Quaternius `Mech`
  GLB as the visible runtime hero silhouette. It is scaled/rotated into Y-up and
  packed through the existing mesh pack. Current caveat: original GLB texture
  atlas/material colors are not restored yet; runtime uses a temporary green
  toy material.
- 2026-06-19: Restored the downloaded GLB material colors by extracting its
  embedded 32x32 `Atlas.png`, packing it as `NT_ASSET_TEXTURE`, adding `uv0`
  to the source mech stream, and sampling it in the mech shader. Also added
  raised studs to the hangar pad and block props. New smoke screenshots show a
  textured green/gray/orange toy mech on a more physical Roblox-like baseplate.
- 2026-06-19: Strict product-read gate passed for the hangar screenshot with
  all visual scores at 4/5 and no major issue. Minor caveat remains: future
  passes should present customization slots more strongly.
- 2026-06-19: product gate PASS (desktop-hangar); review: gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T17-37-47-327Z_desktop-hangar.md; screenshot: build/captures/mech_t0021_hangar_smoke.png; next: continue to the next narrow slice
- 2026-06-19: Added runtime shoulder hardpoint overlay for the downloaded hero
  mech: locked orange sockets before purchase, then amber/cyan rocket pods
  after the rocket purchase. Recaptured smoke screenshots prove both states.
  Also downloaded Poly Pizza/Quaternius `Robot Enemy Legs Gun` as a CC0 source
  candidate for future modular robot/mech parts; it is not the runtime hero.
- 2026-06-19: product gate PASS (desktop-rocket-modules); review: gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T17-52-23-263Z_desktop-rocket-modules.md; screenshot: build/captures/mech_t0021_rockets_smoke.png; next: continue to the next narrow slice
- 2026-06-19: Bound cannon/rocket attack effects to visible source-mech module
  points instead of the old procedural cube-mech muzzle coordinates. Added
  twin cannon flashes/tracers, visible rocket tube plumes, target hit rings,
  and heat/vent lines, then extended the smoke harness with specific cannon and
  rocket attack captures.
- 2026-06-19: product gate PASS (desktop-attack); review: gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T23-05-35_desktop-attack.md; screenshot: build/captures/mech_t0023_rocket_attack_smoke.png; next: prove movement weight with moving/strafe screenshot evidence
- 2026-06-19: Added movement-readability feedback tied to real WASD input:
  source-mech facing/lean is supported by foot stomp rings, dust puffs, ground
  streaks, strafe streaks, and a smoke check that holds `W+D` through
  `input.key` before capture.
- 2026-06-19: product gate PASS (desktop-movement); review: gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-19T23-11-37_desktop-movement.md; screenshot: build/captures/mech_t0023_moving_strafe_smoke.png; next: source or author a stronger modular/rigged Roblox-like mech asset
