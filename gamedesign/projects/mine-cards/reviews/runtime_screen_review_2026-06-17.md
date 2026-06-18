# Mine Cards Runtime Screen Review - 2026-06-17

## Verdict

Current native screen is not acceptable as a game screen yet. It proves that the
native loop, Ozz/KayKit animation path, text, DevAPI capture, and runtime atlas
can run together, but the composition reads as a technical demo.

## Blocking Issues

- No clear focal action. The miner is centered, the target is missing from the
  framebuffer proof, and the progress bar crosses the same visual area instead
  of explaining the action.
- The 3D character is not visually owned by the UI. It is rendered by a separate
  full-screen 3D pass and only happens to overlap the top panel.
- The lower board has too little hierarchy. Tabs, node cards, upgrade panel, and
  bottom nav use similar rectangles and similar weight, so the player cannot
  tell what matters first.
- The screen is only checked as landscape. Portrait is a first-class target for
  the Capybara-like direction and must be proved separately.
- This is still not a game verdict. The screen shows a mining timer and an
  upgrade, but does not yet prove an understandable five-minute idle loop.

## Immediate Acceptance Criteria

- Landscape proof: `960x540` native screenshot.
- Portrait proof: `540x960` native screenshot.
- Top stage must read as actor -> target -> reward/progress in both orientations.
- The stage target must be real runtime art from the atlas, not debug shapes, and
  must be visible in the framebuffer capture.
- The 3D miner must be placed intentionally inside the stage composition, not
  hard-centered over unrelated UI.
- The lower board must make the selected mining verb and the next upgrade more
  visually important than locked/future tabs.

## Next Fix

Rebuild the first screen as a responsive action-stage layout:

- landscape: compact header, wide action stage, actor left-of-center, target
  right-of-center, progress/action HUD below the actor-target line, mechanics
  board underneath;
- portrait: taller action stage at the top, actor and target still visible as a
  pair, tabs wrap below, mechanics cards stack before the upgrade panel;
- both: bottom nav remains visible, but does not compete with the active mining
  board.

## Follow-Up Result

Evidence:

- `build/captures/mine_cards_layout_review_v006_landscape_surface.png`
- `build/captures/mine_cards_layout_review_v006_portrait_surface.png`

Fixed:

- Runtime sprite UI is now actually active. The earlier screen was mostly
  fallback shapes because atlas region lookup used runtime string hashes instead
  of generated normalized region hashes.
- Generated stone/copper target sprites are packed into
  `assets/mine_cards_ui.ntpack` and the stone target is visible in both
  landscape and portrait framebuffer captures.
- Portrait now has its own stacked lower mechanics layout instead of a squeezed
  landscape layout.
- The miner is no longer hard-centered in the framebuffer; it is biased into
  the actor side of the top action stage.

Still weak:

- The pickaxe motion does not yet physically contact the target. The stage reads
  as actor + target + progress, but not as a polished mining interaction.
- Progress/callout text is still too close to the actor line, especially in
  event/copper states.
- This remains a direction proof, not a game-complete first slice. The next pass
  should refine action-stage staging and then run the product/game-loop gate.

## Action Contact Follow-Up

Evidence:

- `build/captures/mine_cards_action_contact_v003_landscape_surface.png`
- `build/captures/mine_cards_action_contact_v003_landscape_copper_callout.png`
- `build/captures/mine_cards_action_contact_v003_portrait_surface.png`

Improved:

- Target placement now follows the actual Ozz/KayKit pickaxe swing direction
  instead of placing the rock on the opposite side of the actor.
- The progress bar is lower and thinner, so it reads more like HUD than a
  physical object under the character.
- Callout feedback is separated from the actor/progress strip in the copper
  state.

Still weak:

- The action stage uses hand-tuned camera/target placement. A proper stage
  camera/anchor contract is still needed before this becomes reusable.
- The miner and target now contact visually, but the hit moment still lacks
  impact feedback such as a chip, flash, particle, or reward pop.
- Product/game-loop gate is still not run; this remains a visual/core-moment
  improvement, not completion of the first slice.

## Stage Ownership / Aspect Follow-Up

Evidence:

- `build/captures/mine_cards_layout_review_v008_landscape_surface.png`
- `build/captures/mine_cards_layout_review_v008_landscape_geode.png`
- `build/captures/mine_cards_layout_review_v008_portrait_surface.png`
- `build/captures/mine_cards_layout_review_v008_portrait_geode.png`
- `build/captures/mine_cards_layout_review_v008_landscape_geode_uizoom_cmp.png`
- `build/captures/mine_cards_layout_review_v008_portrait_geode_uizoom_cmp.png`

Improved:

- The 3D miner is now rendered through an explicit miner-lane viewport inside
  the top action panel instead of a full-screen camera pass. This fixes the
  worst ownership bug: the character no longer floats over unrelated UI.
- The stage is larger in landscape and the miner/target/reward signal now read
  as one action cluster in both landscape and portrait.
- Reward feedback no longer competes with duplicate stage callouts during the
  hit moment; the screen shows one local reward pop near the mined target.
- Readability montage passed without text-on-bright regression in both aspect
  ratios.

Still weak:

- This is still not a game-complete first slice. It is a stronger action-stage
  proof, but the lower mechanics board still looks like repeated rectangles
  rather than a readable set of future idle activities.
- The selected Mining card is not visually tied strongly enough to the top
  action stage. A new player can see a miner hitting stone, but the screen does
  not yet explain the broader loop well enough.
- Landscape spends more height on the stage, which helps the character/action
  read but compresses the lower board. The next pass must redesign the board
  hierarchy, not just resize it.
- The miner is now stage-owned but still hand-positioned; reusable character
  anchoring should become a small game-side module before other activities reuse
  the same stage.

## Lower Board Hierarchy Follow-Up

Evidence:

- `build/captures/mine_cards_board_hierarchy_v002_landscape_surface.png`
- `build/captures/mine_cards_board_hierarchy_v002_landscape_geode.png`
- `build/captures/mine_cards_board_hierarchy_v002_portrait_surface.png`
- `build/captures/mine_cards_board_hierarchy_v002_portrait_geode.png`
- `build/captures/mine_cards_board_hierarchy_v002_landscape_geode_uizoom_cmp.png`
- `build/captures/mine_cards_board_hierarchy_v002_portrait_geode_uizoom_cmp.png`
- `gamedesign/projects/mine-cards/reviews/product_gate_board_hierarchy_v002_2026-06-17.md`

Improved:

- The lower board now has an explicit `MINING ACTIVE` block instead of eight
  equal-weight activity tabs.
- Future systems are quieter preview chips, so the current activity reads first
  while still showing the Melvor-like breadth target.
- Mining node cards now identify the running node, show node yield, and keep
  the locked Copper Vein visible as the next activity target.
- The upgrade panel now reads as `NEXT GOAL` rather than just another side card.
- Portrait improved substantially: the screen reads top action stage -> active
  mining board -> next goal -> bottom nav.

Still weak:

- Landscape is still tight. It no longer overlaps the bottom nav, but the board
  has little breathing room and needs a more deliberate desktop arrangement.
- The missing-cost line is technically present but too small to carry the
  affordability explanation by itself.
- Future activity chips are lower priority now, but they still lack icons or
  distinct silhouettes; the board is more readable, not yet polished.
- This is still a failed product/game-read gate until live-state matrix coverage
  proves upgrade unaffordable, affordable, purchased, and geode states.

## Live-State Matrix Follow-Up

Evidence:

- `build/captures/mine_cards_live_state_v002_upgrade_unaffordable.png`
- `build/captures/mine_cards_live_state_v002_upgrade_affordable.png`
- `build/captures/mine_cards_live_state_v002_upgrade_purchased.png`
- `build/captures/mine_cards_live_state_v002_upgrade_unaffordable_portrait.png`
- `build/captures/mine_cards_live_state_matrix_v002.json`
- `build/captures/mine_cards_live_state_v002_upgrade_unaffordable_uizoom_cmp.png`
- `build/captures/mine_cards_live_state_v002_upgrade_unaffordable_portrait_uizoom_cmp.png`

Improved:

- Upgrade unaffordable, affordable, and purchased states now have native
  screenshot evidence plus DevAPI state JSON.
- Portrait upgrade callout was moved out of the actor/target line so it no
  longer covers the miner and rock.
- The live-state matrix now uses the product-gate-compatible `required_states`
  and `states` shape.

Still weak:

- The upgrade card's inline missing-cost text is too small; the state is
  technically covered, but not good enough for final product-read pass.
- Upgrade state coverage is partial because it relies on accelerated DevAPI
  setup, not a normal first-session path.
- The next gate should either improve lower-board affordability UI or accept
  this as known debt before expanding systems.

## Affordance / Placeholder Icon Follow-Up

Evidence:

- `build/captures/mine_cards_affordance_v001_landscape_surface.png`
- `build/captures/mine_cards_affordance_v001_portrait_surface.png`
- `build/captures/mine_cards_affordance_v001_landscape_geode.png`
- `build/captures/mine_cards_affordance_v001_portrait_geode.png`
- `build/captures/mine_cards_affordance_v001_upgrade_unaffordable.png`
- `build/captures/mine_cards_affordance_v001_upgrade_affordable.png`
- `build/captures/mine_cards_affordance_v001_upgrade_purchased.png`
- `build/captures/mine_cards_affordance_v001_state.json`
- `gamedesign/projects/mine-cards/reviews/product_gate_affordance_v001_2026-06-17.md`

Improved:

- The upgrade panel now uses larger resource rows for Stone, Copper, and Coins
  instead of one tiny missing-cost line.
- Affordable and purchased states are easier to distinguish: green Upgrade
  button, all-ready text, and equipped/speed text.
- Future activity chips now have small lock/silhouette markers, and the active
  Mining tab has a small stone marker from the runtime atlas.
- Product gate scores improved to 4/5 on composition, readability, controls,
  and action direction.

Still weak:

- Product gate still fails on art quality and audience fit. The icon treatment
  reuses placeholder atlas sprites; it is not a proper generated/artist icon
  family.
- Future systems are now visually quieter and marked, but their silhouettes are
  not yet meaningful enough for a polished idle RPG hub.
- Next step is either generated runtime icon/UI family integration or an
  explicit decision to accept placeholder-art debt before expanding mechanics.

## Stage Composition Review Follow-Up

Evidence:

- `build/captures/mine_cards_stage_review_v004_landscape_surface.png`
- `build/captures/mine_cards_stage_review_v004_landscape_geode.png`
- `build/captures/mine_cards_stage_review_v004_portrait_surface.png`
- `build/captures/mine_cards_stage_review_v004_portrait_geode.png`
- `gamedesign/projects/mine-cards/reviews/product_gate_stage_review_v004_2026-06-17.md`

Improved:

- Stage target and reward callouts are no longer drawn as a late overlay that can
  sit over the 3D character body.
- The active Mining state is duplicated in the top stage, making the intended
  connection between the board and the character action more explicit.
- Landscape and portrait screenshots are both captured from the native runtime
  for the same stage review pass.

Blocking:

- Product gate still fails. The top stage is a large empty framed area with a
  small action cluster, not a strong character/action hero.
- The miner/rock/pickaxe relationship still does not read as one satisfying
  mining action; the miner remains too small/partial and target contact is
  ambiguous.
- Portrait is present but not phone-first enough. It stacks the same dense panel
  language instead of making the character/action area the clear first read.
- The lower board remains too same-weight: active activity, future chips, node
  cards, upgrade panel, and nav still share similar rectangular treatment.

Next:

- Freeze mechanic/content expansion.
- Do a composition rescue before more systems: rebuild the stage layout around a
  readable actor-target-reward triangle, then create/integrate a generated or
  artist UI/icon family for the stage chrome, activity icons, resources, and
  upgrade states.

## Stage Rescue v007 Follow-Up

Evidence:

- `build/captures/mine_cards_stage_rescue_v007_landscape_surface.png`
- `build/captures/mine_cards_stage_rescue_v007_landscape_geode.png`
- `build/captures/mine_cards_stage_rescue_v007_portrait_surface.png`
- `build/captures/mine_cards_stage_rescue_v007_portrait_geode.png`
- `gamedesign/projects/mine-cards/reviews/product_gate_stage_rescue_v007_2026-06-17.md`

Improved:

- The 3D miner now renders through a narrower actor viewport, so the character
  is larger and no longer collapsed by a very wide stage aspect ratio.
- The disconnected `ACTIVE MINING` badge was removed from the stage; the screen
  relies on the stage title, active Mining board row, progress, and local reward
  pop instead.
- The stone target is separated from the body rather than sitting directly under
  the character.

Still fail:

- The product gate still fails. Numeric layout tuning has reached its limit:
  the top stage remains mostly empty procedural chrome instead of a strong mine
  scene.
- Portrait is usable but not phone-first enough; the character/action zone still
  does not dominate the first read.
- The lower board remains same-weight rectangular UI. This needs a coherent
  generated/artist UI and icon family, not more placeholder markers.

Next:

- Stop broad numeric layout tuning.
- Create the generated/artist source family for stage chrome, activity icons,
  resources, upgrade states, and reward FX; integrate the smallest native proof
  after source art is accepted.

## Stage Rescue v008 / Art Job Follow-Up

Evidence:

- `build/captures/mine_cards_stage_rescue_v008_landscape_surface.png`
- `build/captures/mine_cards_stage_rescue_v008_landscape_geode.png`
- `build/captures/mine_cards_stage_rescue_v008_portrait_surface.png`
- `build/captures/mine_cards_stage_rescue_v008_portrait_geode.png`
- `build/captures/mine_cards_stage_rescue_v008_landscape_surface_uizoom_cmp.png`
- `build/captures/mine_cards_stage_rescue_v008_portrait_surface_uizoom_cmp.png`
- `gamedesign/projects/mine-cards/reviews/product_gate_stage_rescue_v008_2026-06-17.md`
- `gamedesign/projects/mine-cards/art_requests/mine-cards-stage-ui-family-v001.json`

Improved:

- Portrait stage label no longer overlaps the enlarged character.
- Readability compare montages pass without text-on-bright regression.
- The next art-source packet is draft-valid and names concrete source families
  and prompt packets for icon and stage-background generation.

Still fail:

- Product gate still fails. The screen needs real source art: stage background
  layers, activity/resource/upgrade icons, reward FX, and less same-weight UI
  chrome.
- Do not continue broad numeric layout tuning before source art exists; it is
  now optimizing around placeholder panels.

Next:

- Generate/accept source sheets from
  `gamedesign/projects/mine-cards/art_requests/mine-cards-stage-ui-family-v001.json`,
  starting with `art/prompts/mine-cards-icons-v001-prompt.md` and
  `art/prompts/mine-cards-stage-bg-v001-prompt.md`.

## Generated Icon Runtime Proof Follow-Up

Evidence:

- `gamedesign/projects/mine-cards/art/candidates/mine-cards-icons-v001-candidate-e-alpha.png`
- `gamedesign/projects/mine-cards/reviews/mine-cards-icons-v001-candidate-e-alpha-intake.md`
- `gamedesign/projects/mine-cards/reviews/mine-cards-icons-v001-candidate-e-contact_sheet.png`
- `gamedesign/projects/mine-cards/reviews/mine-cards-stage-ui-family-v001-generated_ui_asset_audit.md`
- `build/captures/mine_cards_icons_runtime_v002_landscape_surface.png`
- `build/captures/mine_cards_icons_runtime_v002_landscape_geode.png`
- `build/captures/mine_cards_icons_runtime_v002_portrait_surface.png`
- `build/captures/mine_cards_icons_runtime_v002_portrait_geode.png`
- `gamedesign/projects/mine-cards/reviews/product_gate_icons_runtime_v002_2026-06-17.md`

Improved:

- The first generated Mine Cards icon source sheet is accepted for runtime
  proof. Earlier candidates were rejected for merged components, unsafe gutters,
  or key-color conflicts; candidate E alpha-cleaned source passed intake with 15
  components and the runtime PNG pixel audit passes.
- `assets/mine_cards_ui.ntpack` now includes generated activity, resource,
  upgrade, and state icons through the engine atlas/sprite path. No engine
  submodule changes were made.
- Lower-board placeholders are replaced in the native screen: activity chips,
  node rows, upgrade costs, and the upgrade button now use generated icons.
- Landscape and portrait screenshot proof exists for idle and geode states, and
  readability compare montages pass without text-on-bright regression.

Still fail:

- This is not a product pass. The stage remains the core visual failure: empty
  chrome, weak actor-target-reward cause/effect, and reward feedback still near
  the character body rather than clearly from the rock/reward lane.
- Icon scale is inconsistent. Future-chip icons are very small, while node and
  upgrade icons are noisy at current sizes.
- The accepted icon sheet is a first runtime proof, not completion of the
  generated UI family. Stage background layers, frame art, and reward FX remain
  open.

Next:

- Keep feature expansion frozen.
- Rebuild landscape and portrait as separate authored compositions using the
  accepted icon family.
- Generate or accept stage background/FX source art, then recapture the four-shot
  proof and rerun the visual director/product gate.
