---
type: UI UX Spec
title: Voxelheim UI/UX Rescue Spec
description: First-screen UI/UX contract for replacing the unclear Voxelheim idle prototype surface.
tags: [voxelheim, ui, ux, visual, rescue]
checked: 2026-06-17
---

# Voxelheim UI/UX Rescue Spec

## Visual Session Contract

- Goal: rebuild the first screen so a new player instantly understands combat,
  loot, and keep repair.
- Non-goal: do not add more late-game systems, events, stores, or monetization
  before the first screen reads.
- Proof: native screenshot + `tools/devapi/ui_readability.py` zoom montage +
  product gate against this spec.
- Stop condition: if the still screenshot does not show one obvious next action
  and readable text, feature/content expansion remains frozen.
- Likely files: `src/voxelheim_main.c`, `assets/raw/*`, `assets/voxelheim.ntpack`,
  `gamedesign/projects/voxelheim/visual/*`, `gamedesign/projects/voxelheim/data/rescue_loop.json`.

## Current UI Problems

- Top-left HUD is too heavy and black for the bright toy art.
- Bottom four-button row gives four equal choices before the player knows why
  any choice matters.
- Button internals are cramped: icon, name, level, effect, and cost compete.
- Minimap has no first-slice gameplay value and steals attention.
- Brand/Frost Shards plate is prominent before Frost Shards matter.
- Combat reward feedback is not clearly tied to the next decision.
- Text relies on outlines and busy backgrounds instead of calm content plates.

## 2026-06-17 UI Rescue Pass

Runtime pass status: **partial pass, not final art**.

Changed in the native screen:

- replaced near-black HUD/room/popup plates with colder frosted-blue panels that
  still support high-contrast light text;
- moved the Frost Shards badge out of the Frost Keep header area so meta
  currency no longer collides with the room list;
- added two-line room rows: price/status plus the player-facing unlock
  (`opens rune cards`, `opens training`, `helper +25%`);
- added a dedicated card-choice instruction plate (`Choose 1 Rune`) so the
  choice beat no longer floats directly over the combat scene;
- kept the runtime to one primary choice state instead of adding new systems.

Evidence:

- card-choice screenshot: `build/captures/ui_rescue_card_choice.png`;
- card-choice readability montage: `build/captures/ui_rescue_card_choice_uizoom.png`;
- offline popup screenshot: `build/captures/ui_rescue_offline_popup.png`;
- offline popup readability montage: `build/captures/ui_rescue_offline_popup_uizoom.png`;
- product gate:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_card_choice_ui_rescue.md`.

### 2026-06-17 Meta Rail Cleanup

Runtime pass status: **right-rail layout pass**.

Changed in the native screen:

- after Frost Blueprints unlock, the Frost Keep panel collapses from a dense
  3-room list into a compact objective strip: `Rooms N/3` plus the next repair;
- Frost Blueprints stays as the spend panel below the Keep objective, so
  permanent upgrades no longer stack into the same visual block as room repair;
- the post-Avalanche FTUE prompt shortens to `Recover Blocks for Gate` /
  `Repair Gate to rebuild again`, avoiding overlap with the meta panel.

Evidence:

- blueprints layout screenshot:
  `build/captures/ui_rescue_blueprints_layout.png`;
- blueprints readability montage:
  `build/captures/ui_rescue_blueprints_layout_uizoom.png`;
- offline layout screenshot:
  `build/captures/ui_rescue_offline_layout.png`;
- product gate:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_blueprints_layout.md`.

### 2026-06-17 Reward Feedback Pass

Runtime pass status: **first reward/audio pass, not final juice**.

Changed in the native screen:

- repairs, rune-card picks, hero training, Avalanche Reset, Blueprint purchase,
  and offline collect now fire generated audio cues through `game_audio`;
- major rewards now spawn short sprite bursts, sparkles, coin fountains, and
  event-specific pulse overlays instead of only static row/state changes;
- DevAPI `game.state` exposes `audio` cue counts and `reward_pulses`, so probes
  can verify that success/notify/error feedback actually fired;
- Avalanche Reset feedback was reduced after visual review so it no longer
  blankets the bottom CTA or Frost Blueprints panel;
- Blueprint feedback copy was shortened to `Learned!` / `Need Shards` to avoid
  covering the panel header.

Evidence:

- reward probe:
  `py -3.12 tmp/reward_feedback_probe.py`;
- screenshots:
  `build/captures/ui_reward_gate_repair.png`,
  `build/captures/ui_reward_card_choice.png`,
  `build/captures/ui_reward_avalanche.png`,
  `build/captures/ui_reward_blueprint.png`,
  `build/captures/ui_reward_offline.png`;
- readability compare:
  `build/captures/ui_reward_blueprint_uizoom_cmp.png`,
  `build/captures/ui_reward_offline_uizoom_cmp.png`;
- product gate:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_reward_feedback.md`.

Remaining UI/UX gaps:
- Forge/Campfire world markers are still placeholder-level and need real room
  art.
- Reward feedback now exists, but transient floaters can still overlap one line
  of Blueprint detail for a short frame and final juice timing/audio mix needs a
  later polish pass.
- This pass improves readability and hierarchy, but it is not a release-quality
  visual pass.

### 2026-06-17 Live UI Regression: CTA Text and Purple Edge

Runtime pass status: **hotfix proof, not final product gate**.

What failed:

- the previous accepted evidence used an offline popup/reward screenshot, which
  hid the normal post-offline live state with Gate affordable, Blueprints open,
  and combat floaters active;
- `assets/raw/button.png` still carried a chroma-key/magenta source edge, so a
  pack rebuild could reintroduce purple pixels on the green CTA button;
- CTA text used the same outline style as floating combat labels, which made
  black text on the green button read as muddy duplicate text;
- the HUD Blocks icon reused a coin-like badge and read as a dirty green second
  currency instead of an icy block resource.

Changed in the native screen:

- cleaned the durable `assets/raw/button.png` source and rebuilt
  `assets/voxelheim.ntpack`;
- changed the CTA and HUD Blocks icon to the rock/block sprite instead of the
  coin/badge art;
- removed the duplicate center FTUE prompt when the repair CTA is visible;
- draws transient floaters before UI text so labels/buttons stay readable;
- hides the top-right Frost Shards badge while Frost Blueprints is visible;
- added a focused live-state proof probe for the exact missed state.

Evidence:

- live stress screenshot:
  `build/captures/ui_text_live_overlap_fix.png`;
- zoom montage:
  `build/captures/ui_text_live_overlap_fix_uizoom.png`;
- before/after zoom:
  `build/captures/ui_text_live_overlap_fix_uizoom_cmp.png`;
- live-state probe:
  `py -3.12 tmp/ui_text_overlap_probe.py 9154 build/captures/ui_text_live_overlap_fix.png`;
- purple edge audit:
  `py -3.12 tmp/audit_cta_purple.py build/captures/ui_text_live_overlap_fix.png`
  reports `assets/raw/button.png` at 0 purple-like pixels. The remaining
  screenshot matches are outside the button edge in the wide crop, not the old
  magenta source halo.

Open risk:

- `tools/devapi/ui_readability.py` still warns that the CTA crop has thin text
  strokes. The zoom is readable, but this should remain a polish item instead of
  being recorded as a strict final product pass.

## New Screen Hierarchy

1. **Primary next action**: claim loot, choose a card, or repair the next keep
   room. Only one can be visually dominant at a time.
2. **Combat result**: hero, one enemy, hit/reward numbers, enemy HP.
3. **Visible progression object**: Frost Keep repair track with 3 rooms.
4. **Run resources**: Gold, Frost Blocks, Keep Rank.
5. **Secondary upgrades**: compact hero training controls.
6. **Locked/future systems**: shown only as small previews with one unlock
   reason.

## First-Slice Layout

Desktop 960x540 target:

- Top bar: one frosted rounded plate, 3 resources max.
- Center-left: hero and one large enemy in a clean combat slot.
- Center-right or right rail: Frost Keep room progress: Gate, Forge, Campfire.
- Bottom center: one dominant next-action button or 3 reward cards when a choice
  is pending.
- Bottom secondary row: compact hero training, no more than 3 visible stats in
  the first session.
- No minimap in the first slice.

## Component Rules

- Minimum runtime text size target: 24 px equivalent on 960x540; primary numbers
  larger.
- Every text label sits on a calm plate that fully contains it.
- Icons and labels are separate runtime elements; no baked labels in art.
- Every control has states: normal, pressed, disabled, locked, affordable,
  ready, active.
- Affordable/ready uses a gold or cyan rim plus one pulse, not permanent noise.
- Locked controls show one short reason, e.g. `Repair Gate first`.
- Reward feedback starts near the enemy/action, then flies to Gold/Blocks.

## Art Direction Changes

Replace the current dark RPG overlay with:

- frosted glass/ice plates with warm gold accents;
- chunky toy-block room icons;
- large readable resource icons;
- clean card backs for 1-of-3 loot/rune choices;
- distinct hero gear/companion silhouettes;
- background kept lighter and less contrasty behind UI plates.

The visual target is "cozy toy diorama with readable game UI," not "black RPG
HUD over a Roblox-like scene."

## First-Player Test

A still screenshot must answer:

1. What is fighting what?
2. What did I earn?
3. What should I press next?
4. What will that press change?
5. What is being rebuilt or improved?

If any answer requires the designer to explain the screen, the UI pass fails.
