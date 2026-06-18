---
type: VisualDirectorReview
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17
evidence:
  - build/captures/mine_cards_stage_rescue_v008_landscape_surface.png
  - build/captures/mine_cards_stage_rescue_v008_landscape_geode.png
  - build/captures/mine_cards_stage_rescue_v008_portrait_surface.png
  - build/captures/mine_cards_stage_rescue_v008_portrait_geode.png
---

# Visual Director Review - Stage Rescue v008

## Verdict

This is not a satisfying game screen yet. It proves that the native UI,
Ozz/KayKit miner, target sprite, progress, rewards, and portrait capture can run
together, but it still reads as a technical arrangement of boxes.

The next iteration must stop treating the screen as one responsive rectangle
layout. Landscape and portrait need separate composition rules with the same
game logic.

## What To Look At

1. Focal hierarchy is weak.
   The eye should go to miner -> rock -> reward/progress -> next upgrade. In the
   current screenshots the eye goes to frame borders, repeated purple panels,
   and empty stage space.

2. The character is not fully owned by the stage.
   The miner is no longer full-screen-overlay wrong, but he still has no mine
   floor, shadow, lighting, or UI anchor that makes him feel placed in the
   action stage. He is a model inside a frame, not the visible embodiment of the
   active Mining tab.

3. The action contact is still soft.
   The rock is near the miner, but the pickaxe hit, target damage, and reward
   pop do not form a clear cause-effect chain. In the geode frame, the reward
   label reads as floating on the character rather than coming from the mined
   object or reward lane.

4. The lower board is too same-weight.
   Active Mining, future activities, node cards, upgrade, and bottom nav all use
   similar rectangular treatment. It looks like a tool panel, not an idle RPG
   hub with a clear current action and future breadth.

5. Portrait is present, not designed.
   The portrait capture no longer overlaps text, but it still stacks the same
   box language. A phone-first version should make the top character/action
   stage the first read and keep the current node plus next upgrade close
   together.

6. Landscape wastes the stage.
   The wide stage has lots of empty space while the board below is compressed.
   The wide orientation should use the width for a deliberate actor-target-
   reward composition, not just a larger frame.

## Landscape Composition Target

For `960x540`, the first read should be:

```text
compact HUD
wide mine action strip: actor left / rock center-right / reward lane right
mechanics board: active Mining + nodes center / next goal right / future tabs quiet
bottom nav
```

Requirements:

- Miner is placed on a visible mine floor patch with shadow or contact base.
- Rock sits on the pickaxe arc, not merely near the body.
- Reward pop appears near the rock or in a dedicated reward lane, never over the
  character body.
- Progress belongs to the mined node, visually below or beside the target.
- Active Mining has a visual bridge to the stage: color ribbon, icon, label, or
  line treatment.
- Future activities are visible for Melvor-like breadth but quieter than current
  Mining and next upgrade.
- Next Goal remains readable on the right without competing with the stage.

## Portrait Composition Target

For `540x960`, the first read should be:

```text
compact HUD
tall character/action stage
active Mining strip
current node stack
next goal upgrade card
quiet future activity grid
bottom nav
```

Requirements:

- The miner/action stage occupies the first visual beat, not just the top slot.
- Stage title never crosses the character silhouette.
- Rock, progress, and reward are grouped below or beside the tool contact.
- Current node and Copper Pickaxe goal stay close enough to read as one loop:
  mine stone -> unlock copper -> buy pickaxe.
- Future activities collapse into compact icon chips; they must not become the
  dominant middle of the screen.
- Bottom nav remains stable and readable, but it should not compete with the
  active Mining board.

## Next Native Iteration

Do not add more mechanics until this screen passes as a game screen.

1. Accept or regenerate source art for the stage and icons.
   Current procedural UI has hit its limit. The next screen needs at least a
   mine background layer, activity/resource/upgrade icons, and reward FX.

2. Rebuild the responsive layout as two authored compositions.
   Keep one game state model, but separate landscape and portrait placement
   rules for stage, board, future activities, and upgrade card.

3. Anchor the 3D miner to the stage.
   Add stage-local actor anchors: foot/base position, target contact point,
   reward lane, progress lane, and screen-safe bounds.

4. Rerun proof as a four-shot matrix.
   Required screenshots: landscape idle, landscape reward/geode, portrait idle,
   portrait reward/geode.

5. Review the whole screen, not only text metrics.
   UI readability can pass while the screen still fails as a game. The next
   product gate must explicitly judge focal hierarchy, action cause-effect,
   orientation fit, and art cohesion.
