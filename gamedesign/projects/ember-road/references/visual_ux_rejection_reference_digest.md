---
type: Project Reference Digest
title: Ember Road Visual/UX Rejection Reference Digest
description: Focused reference reset after lead rejected the current Ember Road visual and UX.
tags: [project, ember-road, references, ux, visual-direction, lead-rejection, y-up]
updated: 2026-06-20
---

# Ember Road Visual/UX Rejection Reference Digest

## Reference Lock

- mode: central deconstruction refresh, scoped to visual/UX grammar for the
  next accepted target screen.
- reference question: what should the player-facing Ember Road screen look and
  feel like so it reads as a fantasy hero RPG with map travel, town/route
  hubs, quests, items, grinding/leveling, and automated battles?
- current rejected capture:
  `build/captures/ember-road/state_old_mine_depth_encounter.png`
  after the 2026-06-21 follow-up; the older
  `build/captures/ember-road/state_modal_or_choice_open.png` remains the
  original rejection evidence.
- no-coding/no-final-art boundary: no new dungeon, economy, enemy, or reward
  content should be implemented until the next direction/fake-shot target
  closes the mismatch below and passes a strict visual/UX gate.
- Y-up invariant: game, world, and UI layout are always Y-up. Renderer, input,
  screenshot, and DevAPI rectangle conversion may be Y-down only at explicit
  boundary adapters.
- expected proof: a new accepted direction board or fake shot, then a native
  screenshot where the main scene, quest/route decision, encounter promise,
  reward preview, and control hierarchy are readable without explanation.

## Why The Current Screen Is Rejected

Current capture: `build/captures/ember-road/state_modal_or_choice_open.png`.

- The screen looks like an ornate frame pasted onto an unfinished choice state,
  not like a playable RPG decision. The strongest action is `SCOUT`, but it is
  labeled `NEXT SLICE`, which tells the player this is production scaffolding.
- The UX is still modal-first. The cave art is visible, but the actual decision
  is a small rail widget instead of a diegetic route/threshold interaction.
- The route plaques are mostly decorative. They do not behave like a clear map
  or destination system with current node, next node, lock reason, and reward.
- There is no encounter/resource/depth promise on the mine screen. The player
  cannot see what scouting reveals, what threat is ahead, or why the Old Mine
  is worth entering.
- The right rail fights the scene: it duplicates location title, route text,
  and choice panel, but does not create a clean primary action hierarchy.
- The art quality is higher than the earlier road reuse, but the screen grammar
  still does not match the desired game.

## 2026-06-21 Follow-Up Rejection

Current follow-up capture:
`build/captures/ember-road/state_old_mine_depth_encounter.png`.

- The new screen removed `NEXT SLICE` and proves one Cave Bat result, but it
  still does not yet read like the desired game strongly enough.
- The mine scene is visually dominant, but the meaningful RPG decision is still
  mostly in the right rail and bottom prose. The threshold itself needs clearer
  threat, resource, depth, and route affordances.
- `CLEARED` communicates completion, not a next player choice. If the next loop
  is a mine grind, the screen must show what the player can do next: delve a
  cache, push a depth, return, or see a lock/reason.
- The Cave Bat result is stateful but under-visualized. Similar RPG references
  usually give enemy/reward/progression a visible anchor: monster, item,
  quest marker, inventory slot, route marker, or result panel.
- The route strip is closer, but it still reads as labels first and destination
  system second. It needs current node, next node/depth, lock/reason, and reward
  promise grammar.
- Y-up is non-negotiable. All game, world, UI, route, and DevAPI logical layout
  stays Y-up. Convert to Y-down only in named renderer/input/screenshot/DevAPI
  boundary adapters.

## Closer Reference Set

These are role references, not templates to copy. They are useful only for
observed screen grammar and UX vocabulary.

| Reference | Use For Ember Road | Avoid |
| --- | --- | --- |
| Legend: Legacy of the Dragons / War of Dragons (`https://warofdragons.com/`) | Primary taste anchor: dense fantasy browser-RPG chrome, illustrated locations, location/object/NPC lists, inventory/status density, map/hunt mode with visible entities. | Exact frames, faction names, icons, old-browser clutter, monetization and social scope. |
| Dragon Eternity interface (`https://dragoneternity.com/about/interface/`) | Quest UX: NPC quest markers, quest log goals/rewards, objective tracking, helper arrows, map highlights, completion popups. | Auction/social/trade complexity and UI density beyond the first slice. |
| DragonFable screenshots (`https://www.dragonfable.com/Screenshots`) | Readable fantasy locations, NPCs, monsters, pets/equipment, quest-entry clarity, scene-first storytelling. | Exact Flash-era side-view layout, humor, characters, and oversized menu stacks. |
| AdventureQuest screenshots (`https://www.battleon.com/Screenshots`) | Single-player quest/battle readability: NPC/town entry points, enemy/reward silhouettes, equipment fantasy. | Copying battle layout or making Ember Road a static battle menu. |
| AdventureQuest Worlds screenshots (`https://www.aq.com/embed/spil/agame/screenshots.asp`) | Scene-first fantasy UI with avatar/NPC/action affordances over an actual location. | MMO crowding, chat dependence, exact AQW proportions and assets. |
| KingsRoad | Adjacent map/town/dungeon reference: heroic fantasy travel, quest regions, dungeon entry promise, loot/equipment loop. | Action-RPG control scheme; Ember Road remains map travel plus automated battle. |
| Wartune / Wartune Reborn | Cautionary dense browser-RPG reference: quest tracker, auto-route feeling, reward bundle, progression windows, bottom icon belt. | City-builder, VIP/daily/event clutter, strategy scope. |
| League of Angels / Dragon Awaken / Hero Wars | Adjacent autobattle references: party/hero status, campaign map, equipment/upgrade loops, battle result/reward surfaces. | Gacha/live-service pressure, exact formation UI, premium clutter, copied characters. |
| Shakes & Fidget (`https://sfgame.net/`) | Compact hero identity, tavern/adventure/dungeon/equipment promise, readable class fantasy. | Parody tone, idle timers, wait-pressure framing. |
| Drakensang Online | Spatial dungeon caution: dungeon entrances and interiors should feel like places with depth, threat, and loot promise. | Action-RPG controls/camera, exact environment art, and combat layout. |

## Observed / Supported Facts

- `observed` - The rejected Ember Road mine screen has a dedicated cave image,
  but the controls still expose production scaffolding (`NEXT SLICE`) instead
  of a believable RPG action.
- `observed` - Legend-like browser RPG screens put location, navigation,
  inventory/status, and log density into one persistent fantasy frame. The
  frame supports play; it is not only decoration.
- `observed` - Dragon Eternity documents quest markers, objective tracking,
  helper arrows, map highlights, and quest completion feedback as first-class
  UX. Ember Road needs that clarity more than another decorative panel.
- `observed` - AdventureQuest/DragonFable/AQW screenshots favor readable
  fantasy locations with NPCs, monsters, and action affordances in the scene.
- `inferred` - KingsRoad, Wartune, League of Angels, Dragon Awaken, and Hero
  Wars are useful adjacent refs for map/quest/autobattle/reward grammar, but
  they need a fuller source packet before they can drive balance or final UI.
- `inferred` - Drakensang Online is useful only as a spatial dungeon caution:
  Old Mine should feel like a physical threshold and depth promise, but Ember
  Road should not copy action-RPG controls or isometric combat.

## Revised Visual/UX Target

The next target should be a playable-looking RPG screen, not a more ornate
modal. For the Old Mine pass:

1. Main scene: the cave entrance stays the dominant read, with hero orientation
   and threat direction obvious.
2. Route grammar: `Old Gate -> North Road -> Old Mine` is a route strip with
   current node, previous route, locked/depth state, and a reason to continue.
3. Primary decision: one action should be visually and semantically primary,
   e.g. `Scout entrance` or `Enter depth 1`, not `NEXT SLICE`.
4. Scout result: show what changes after the action: depth, threat, resource,
   XP/gold/item preview, and a compact log line.
5. Quest rail: the rail should summarize objective/progress/reward, not host a
   second modal that competes with the scene.
6. Bottom log/action belt: reward/result feedback should appear in a persistent
   RPG log area, not only as prose inside the panel.
7. UI art: use fewer but clearer controls; no duplicate titles; no debug/future
   labels in player-facing text.
8. Next-action state: after Depth 1 is cleared, show a real next choice such as
   delve cache, push deeper, return, or locked next depth. Do not leave the
   player at a dead-feeling `CLEARED` button.

## Y-Up Layout Contract

- `UiBox.x`, `UiBox.y`, `UiBox.w`, and `UiBox.h` remain logical Y-up layout
  values. Larger `y` means higher on the screen.
- Layout helpers must express vertical relationships in Y-up terms:
  `above`, `below`, `top`, `bottom`, `center_y`.
- Draw/input/DevAPI adapters may convert to framebuffer Y-down with a named
  boundary conversion only. Do not store converted Y-down rectangles as layout
  truth.
- Screenshot annotations and reference docs must describe vertical placement
  using Y-up logic unless explicitly talking about a rendered image.
- Future visual QA should reject any runtime change that silently flips UI,
  route, or world layout semantics to Y-down.
- Treat "Y up" as a gameplay/UX invariant, not as a renderer preference.

## Borrow / Avoid / Copy-Risk

- borrow: scene-first RPG screen, quest marker/rail clarity, route/destination
  highlight, visible enemy/resource/reward preview, bottom result log, compact
  hero/equipment identity, persistent fantasy frame that supports actions.
- avoid: decorative-only chrome, admin dashboard panels, production labels,
  dead buttons, clutter imported from MMO/live-service references, idle/wait
  pressure, and feature expansion before the accepted target is clear.
- copy-risk: exact screenshots, UI ornaments, names, factions, map art,
  character designs, icons, fonts, monetization surfaces, and full layout
  reconstruction from any reference.

## Next Proof

Created direction/fake-shot target for review:

`gamedesign/projects/ember-road/art/ember-road-old-mine-scout-result-direction-v001.png`

Review gate:

`gamedesign/projects/ember-road/reviews/T0018_old_mine_direction_target_review.md`

Target scenario:

`Old Mine entrance -> scout entrance -> first scout result`

Required proof elements:

- current Old Mine scene remains visible and dominant;
- primary action is real player-facing text, not future-slice scaffolding;
- route strip has current/previous/locked-depth grammar;
- scout result shows threat, resource, reward/progress, and log feedback;
- quest rail has one clear purpose;
- Y-up layout notes are explicit;
- strict visual/UX gate compares against this rejection digest and the current
  capture.

## Reference Digest

- mode: central deconstruction refresh after lead rejection.
- sources checked: current native capture, prior Legend/Dragon Eternity/AQW/
  DragonFable/AdventureQuest/Shakes & Fidget reference notes, and newly added
  adjacent reference roles for KingsRoad, Wartune, League of Angels, Dragon
  Awaken, and Hero Wars.
- observed facts: current UI exposes `NEXT SLICE`; quest clarity should come
  from markers/objectives/result feedback; fantasy RPG screens need the scene
  to carry the action; route/map/reward should be visible before input.
- current-build mismatch: the mine backdrop is a better asset, but the UX still
  reads as non-playable scaffolding rather than a real RPG route/encounter
  decision.
- borrow: scene-first route decision, objective markers, scout/result feedback,
  reward preview, bottom log, compact RPG frame.
- avoid: copied layouts, MMO clutter, decorative-only panels, dead controls,
  and any Y-down layout drift.
- next native proof: if the direction fake shot is accepted, implement a native
  screenshot of the Old Mine scout result with Y-up layout audit and strict
  gate.

## 2026-06-21 Reference Refresh Digest

- mode: central deconstruction refresh after renewed lead rejection.
- sources checked: existing project source packet plus newly promoted adjacent
  roles for DragonFable/AdventureQuest quest-location readability, Shakes &
  Fidget compact dungeon/equipment promise, Drakensang Online spatial dungeon
  caution, and autobattle/result caution refs.
- observed facts from current build: T0019 has a better mine scene and reward
  log, but the action still reads as completed/rail-driven rather than a next
  RPG choice.
- current-build mismatch:
  `build/captures/ember-road/state_old_mine_depth_encounter.png` needs stronger
  scene-integrated threat/resource/depth/next-action grammar.
- borrow: visible dungeon threshold, route/depth state, enemy/resource/reward
  anchors, compact next-action choice, bottom result log.
- avoid: adding more systems before the next proof, copied reference layouts,
  gacha/VIP clutter, action-RPG camera/control scope, and any Y-down layout
  drift.
- next native proof candidate:
  `build/captures/ember-road/state_old_mine_next_delve_choice.png`.

## 2026-06-21 Stronger Reference Refresh

Durable refresh:

`gamedesign/projects/ember-road/references/stronger_visual_ux_reference_refresh_2026-06-21.md`

The renewed lead rejection applies to the current town forge screen too:

`build/captures/ember-road/state_town_lantern_upgrade.png`

Key update:

- `observed` - The forge loop was functionally readable but panel-heavy; the
  latest native T0024 pass adds a scene forge workbench/lantern/shards anchor,
  partial source-derived floor/signpost/panel/badge crops, and
  `ember.scene.forge_workbench` DevAPI node, reducing the panel-only mismatch.
- `observed` - The existing fake shots expect route/reward/equipment meaning
  to be anchored in the scene and object surfaces, not only in text panels.
- `inferred` - The next proof should not be Depth 2 content. It should be a
  town forge/equipment visual-UX proof or a mine threshold proof with a visible
  lantern/depth affordance.
- `user-provided` - Y is up, always. Game/world/UI layout stays Y-up, with
  Y-down conversion only at renderer/input/screenshot/DevAPI boundaries.
- `updated` - The stronger refresh now uses a similarity filter instead of a
  loose reference list: closest refs must improve town hub, route choice, RPG
  action, equipment/result, or next-depth readability.
- `updated` - Additional similar refs promoted for loop structure: Broken
  Ranks for hand-finished fantasy locations and tactical combat identity;
  Gladiatus, BattleKnight, and Tanoth for browser-RPG mission/expedition/
  dungeon, loot, and equipment progression grammar.
- `updated` - The next proof must show Y-up authored layout: larger logical
  `y` means higher on the game screen. Y-down values may exist only inside
  named renderer/input/screenshot/DevAPI boundary conversions.
- `updated` - Current native gate is still REVIEW, not PASS:
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_native_scene_anchor_review.md`.
  The scene anchor now uses source-derived workshop/worktable/lantern/result
  assets, and the latest pass reduces the rail to a Mine Lantern item/result
  strip instead of the prior dense form. Remaining work is lead acceptance or a
  new visual-only correction if this direction is rejected.
- `updated` - Y-up layout proof now passes for this forge screen:
  `gamedesign/projects/ember-road/reviews/T0024_town_forge_y_up_layout_audit.md`.
  The audit converts DevAPI screen rectangles back to logical Y-up rectangles
  and confirms `ui.click` on the forge workbench triggers the lantern result.

Next proof candidate:

Lead review of `build/captures/ember-road/state_town_lantern_upgrade.png` and
`build/captures/ember-road/state_town_lantern_forged.png`.

Lead acceptance packet:

`gamedesign/projects/ember-road/reviews/T0024_town_forge_lead_acceptance_packet.md`

Do not expand gameplay content until this proof or an accepted replacement
target closes the mismatch.
