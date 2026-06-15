---
id: T0012
title: Visual product rescue for Splash Rods native slice
status: dropped
epic: E002
priority: P0
tags: [visuals, ui, ux, native, rescue, generated-art]
created: 2026-06-15
updated: 2026-06-15
---

## What

Rebuild the Splash Rods native visual slice after lead rejection. The goal is
not more features; it is a screenshot that reads as a beautiful, juicy,
Roblox-like casual fishing game instead of a technical prototype.

## Done when

- [x] Feature expansion is frozen until the rescue screenshot passes.
- [x] Visual failure report identifies concrete world/UI/UX/art mismatch
      against the fake shot and current native screenshot.
- [x] Rescue art bible update defines the next screen composition, UI hierarchy,
      model/prop quality bar, material language, and forbidden debug/procedural
      fallbacks.
- [ ] Runtime uses improved generated or selected art/model assets for the
      first focal area; shape-renderer/programmer art is not the visible product
      answer.
- [ ] Native screenshot proves the rescue composition and passes
      `tools/product_gate/review.mjs` with `verdict: pass`.
- [x] Playtest probe still validates the fishing loop and GLTF/GLB mesh gate.
- [x] Profiling notes capture the failure cause and the visual rescue loop.

## Current Evidence

- Failed screenshot:
  `tmp/roblox_fishing/native_first_slice.png`.
- Product-read gate fail:
  `gamedesign/projects/roblox-fishing/reviews/product_read_gate_latest.json`.
- Visual failure report:
  `gamedesign/projects/roblox-fishing/reviews/visual_product_failure_report_2026-06-15.md`.
- Rescue screen contract:
  `gamedesign/projects/roblox-fishing/art/visual_rescue_screen_contract_v1.md`.
- Target fake shot:
  `gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png`.

## Rescue Constraints

- Native PC remains the validation harness.
- Do not start web/browser prototype work.
- Do not add gameplay scope while the visual/product gate is red.
- Do not count technical mesh/UI integration as visual acceptance.
- Do not replace failed generated UI with procedural debug panels and call it
  final.

## Next Implementation Slice

1. Write the rescue art bible/screen contract.
2. Generate or select a focused world/UI asset set for the first focal screen.
3. Integrate only the focal-area assets into the native runtime.
4. Capture the new native screenshot.
5. Run product gate pass/fail and the fishing probe.

## Log

- 2026-06-15: Created after lead rejection that the current native screenshot
  looks bad. Product gate is explicitly red; feature expansion is frozen until
  visual rescue passes.
- 2026-06-15: Added visual failure report and rescue screen contract. Next work
  should generate/select focal world/UI assets and integrate only the new rescue
  composition before rerunning the native screenshot gate.
- 2026-06-15: Fixed the immediate unreadable-text issue in the native fishing
  HUD: larger pixel text, shadowed runtime labels, shorter HUD labels, readable
  catch card text, readable secondary buttons, and moved the next-goal copy
  above the primary action. Verified with `cmake --build --preset native-debug`
  and `py -3.12 tools/playtest/roblox_fishing_probe.py 9123`; screenshot:
  `tmp/roblox_fishing/native_first_slice.png`. Overall visual/product gate
  remains red pending the broader rescue pass.
- 2026-06-15: Ran an emergency native rescue pass after lead rejection of ugly
  buttons/icons/text/models/water/rod/progress bar. Added TTF HUD rendering,
  fixed shape/text flush order, hid the always-on reel meter outside bite/reel,
  removed circle-wire water artifacts, replaced the broken thick-line rod with
  small forward-facing 3D segments, reduced/parked bad generated GLB props, and
  reframed the camera. Verified with `cmake --build --preset native-debug` and
  `py -3.12 tools/playtest/roblox_fishing_probe.py 9123`; screenshot:
  `tmp/roblox_fishing/native_first_slice.png`. This fixes immediate readability
  and artifact issues, but the product gate remains red because final generated
  UI/model art and fake-shot-level composition are still missing.
- 2026-06-15: product gate FAIL (desktop); review: gamedesign/projects/roblox-fishing/reviews/product_read_gate_2026-06-15T10-00-17-910Z_desktop.md; screenshot: tmp/roblox_fishing/native_first_slice.png; next: Replace the focal world/character/water with stronger selected or generated 3D art assets and then rerun the native screenshot/product gate.
- 2026-06-15: Fixed reported purple halo on generated UI art. Asset audit previously failed on `fishing_secondary_button_slice9` and `fishing_upgrade_button_slice9`; cleanup now reruns after color/contrast enhancement, `fishing_upgrade_button_slice9` remaps purple material to ocean blue, and the primary green button declares intentional green edge preservation. Verified with generated UI audit PASS, edge proof total=0, native build, and `py -3.12 tools/playtest/roblox_fishing_probe.py 9123`; screenshot: `tmp/roblox_fishing/native_first_slice.png`.
- 2026-06-15: Dropped by lead direction: the fishing game test iteration is
  complete and will not continue. The red product gate is retained as evidence
  for pipeline improvement rather than a blocker for further Splash Rods work.
- 2026-06-15: Dropped by lead direction: Splash Rods product rescue will not continue; use failures as pipeline-improvement evidence under E003.
