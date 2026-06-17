---
id: T0001
title: First native playable slice for Voxelheim
status: doing
epic: E001
priority: P1
tags: [prototype, voxelheim, native-first]
created: 2026-06-16
updated: 2026-06-17
---

## What

Umbrella for the first playable slice **"Frost Keep Approach"** (see
`gamedesign/projects/voxelheim/gdd.md` + `game_implementation_plan.md`). Visual
direction LOCKED: Theme A "Bright Roblox" (`visual/fake_shot_first_screen.png`).
Build proceeds via subtasks under the visual-first freeze:
T0002 (art) → T0003 (first screen + visual gate) → T0004 (core loop).

## Done when

- [x] `gamedesign/projects/voxelheim/gdd.md` names the first playable loop and player-readable goal.
- [x] A fake shot or visual target exists before runtime polish starts.
- [x] A 5-line visual session contract exists: goal, non-goal, proof, stop
      condition, likely files.
- [x] Current native screenshot or capture plan is compared against the fake
      shot/target in a mismatch list before visual code expands.
- [x] Native PC build/run command is identified and captured in the task log.
- [x] First native screenshot/product-read proof is captured before expanding content.

## Open questions

- (resolved 2026-06-16) Visual target = Theme A fake shot
  `visual/fake_shot_first_screen.png`.

## Log

- 2026-06-17 Rescue first native proof implemented for **Frost Keep Rebuilder**:
  Gold + Frost Blocks HUD, Keep Rank, Gate/Forge/Campfire repair list, next-room
  repair CTA, Gate repair, pending 3-card rune choice, combat pause while
  choosing. Evidence: `build/captures/rescue_gate_cards.png`,
  `build/captures/rescue_gate_cards_uizoom.png`,
  `py -3.12 tmp/rescue_probe.py 9144 build/captures/rescue_gate_cards.png`,
  `cmake --build --preset native-debug --target game_seed`.
- 2026-06-17 Extended the first playable chain past Gate: Forge now unlocks
  stronger rune card values plus compact 3-control training; Campfire unlocks a
  visible helper and +25% damage. Evidence:
  `build/captures/rescue_campfire_helper.png`,
  `build/captures/rescue_campfire_helper_uizoom.png`,
  `build/captures/rescue_campfire_helper_uizoom_cmp.png`,
  `py -3.12 tmp/rescue_full_loop_probe.py 9145 build/captures/rescue_campfire_helper.png`.
- 2026-06-17 Product gate fail -> fix -> pass: fail found that Forge/Campfire
  repairs were mostly right-rail row state, not in-world changes. Added world
  Forge/Campfire repair markers and labels; latest product-read gate passes for
  the repair-chain slice:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_repair_chain_pass.md`.
- 2026-06-17: product gate PASS (desktop); review: gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_repair_chain_pass.md; screenshot: build/captures/rescue_campfire_helper.png; next: continue to the next narrow slice
- 2026-06-17: product gate PASS (desktop_avalanche_reset); review: gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17T06-41-25-128Z_desktop_avalanche_reset.md; screenshot: build/captures/avalanche_reset_after.png; next: continue to the next narrow slice
- 2026-06-17: product gate PASS (desktop_card_choice_ui_rescue); review: gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_card_choice_ui_rescue.md; screenshot: build/captures/ui_rescue_card_choice.png; next: continue to the next narrow slice
- 2026-06-17: product gate PASS (desktop_blueprints_layout); review: gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_blueprints_layout.md; screenshot: build/captures/ui_rescue_blueprints_layout.png; next: continue to the next narrow slice
- 2026-06-17: product gate PASS (desktop_reward_feedback); review: gamedesign\projects\voxelheim\reviews\product_read_gate_2026-06-17_reward_feedback.md; screenshot: build/captures/ui_reward_offline.png; next: continue to the next narrow slice
