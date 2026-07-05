---
type: Design Review
title: Location Sprite Review
description: Location-by-location audit of scene art, characters, props, enemies, and sprite gaps for rb-dark-rpg.
tags: [game-design, visual-proof, assets, locations, sprites]
game_id: rb-dark-rpg
status: draft
date: 2026-07-05
---

# Location Sprite Review

## Scope

This review covers the visual state of `rb-dark-rpg` locations and the sprite
requirements for characters, enemies, props, hotspots, exits, and major objects.

Source files checked:

- `games/rb-dark-rpg/design/data/locations.json`
- `games/rb-dark-rpg/design/data/characters.json`
- `games/rb-dark-rpg/design/data/combat.json`
- `games/rb-dark-rpg/design/data/items.json`
- `games/rb-dark-rpg/design/data/asset_manifest.json`
- `games/rb-dark-rpg/src/render/render_hub_scene.c`
- `games/rb-dark-rpg/src/scene/scene_interactions.c`
- `games/rb-dark-rpg/src/ui/combat_flow.c`
- `games/rb-dark-rpg/src/build_packs.c`

## Verdict

The current runtime has three main illustrated scene backgrounds:

- `hub_last_post` / `Последний Пост`
- `hub_gate_outskirts` / `У ворот Последнего Поста`
- `old_mill` / `Старая мельница`

The data model also contains future draft locations:

- `forest_road`
- `ash_convoy`
- `black_sun_hideout`
- `equipment` as a system screen, not a world scene

The accepted city background is the style reference. The new road and mill
backgrounds now follow that direction, but the sprite layer is incomplete. At
runtime the hub scene only draws one separate scene character sprite: the gate
guard. All other NPCs, objects, enemies, and hotspots are either data-only,
represented as list/map markers, represented by combat UI cards, or still
missing final scene sprites.

Rule going forward: every visible, interactive, stateful, or quest-critical
thing needs its own transparent sprite. Static architecture and non-clickable
dressing can stay baked into the background.

## Sprite Types

Use separate asset classes instead of treating every image as interchangeable.

| Type | Usage | Requirement |
| --- | --- | --- |
| `background` | Full location backdrop | 1280x700, no UI text, no baked active NPCs, same style as Last Post |
| `scene_character` | NPC standing in a location | Transparent PNG, full/half body, readable at hub scale |
| `scene_prop` | Clickable object, hotspot, exit, board, clue | Transparent PNG, anchored to scene, no pasted-sign look |
| `combat_actor` | Large animated/posed fight figure | Transparent PNG, normalized to the combat actor canvas |
| `enemy_card` | Prefight/details card thumbnail | Not enough for scene or combat actor by itself |
| `item_icon` | Inventory/reward/shop icon | Small UI icon; not enough for scene props |
| `map_node` | World map location marker | Small map marker; separate from scene sprite |

## Current Runtime Gap

`render_hub_scene.c` supports background selection for:

- `hub_last_post`
- `hub_gate_outskirts`
- `old_mill`

`scene_interactions.c` contains only one scene object:

- `last_post/guard`

`combat_flow.c` maps every non-mill enemy to `combat_actor_gate_scavenger`, and
every encounter id containing `mill` to `combat_actor_mill_scavenger`. This
means later enemies have data/cards, but not distinct combat actor sprites in
runtime.

## Location Review

### 1. `hub_last_post` / `Последний Пост`

Status: strongest scene, current style reference.

Available:

- Background: `asset_location_last_post_bg`
- Gate guard portrait/full-body: `asset_portrait_gate_guard`,
  `asset_character_gate_guard`

Runtime-visible separate scene sprite:

- `hub_last_post.gate_guard`

Needed scene sprites:

| Entity | Type | Current state | Required art |
| --- | --- | --- | --- |
| `player_seeker` | player | portrait available, full body missing | `scene_character` / full body for profile and future scene use |
| `blacksmith` | NPC/vendor | portrait/full body needed | distinct blacksmith scene sprite and portrait |
| `town_trader` | NPC/vendor | currently points at blacksmith asset ids | distinct trader scene sprite and portrait; do not clone blacksmith |
| `elder` | NPC/contract giver | portrait/full body needed | elder scene sprite and portrait |
| `healer` | NPC/service | portrait/full body needed | healer scene sprite and portrait |
| `council_scribe` | NPC/reports | portrait/full body needed; not currently in location object list | scribe scene sprite and portrait before adding to hub |
| `contract_board` | quest board | prop needed | clickable board sprite, readable but not oversized |
| `dragon_memorial` | hotspot | prop needed | dim Dragon memorial/beacon prop |
| `map_gate` | exit | prop needed | gate/exit affordance sprite or gate-state overlay |
| `caged_scavenger` | combat object | enemy card exists; scene object sprite missing | cage plus scavenger scene sprite or compact threat prop |

Review notes:

- The city should keep the guard as the first active object, but the rest of the
  hub should not stay empty forever. The player will read the place as a dead
  background if vendors and service NPCs remain list-only.
- `locations.json` has mojibake for the elder display name inside the location
  object even though `characters.json` has the correct `Староста`. Fix the
  location object text before content polish.
- The first pass can add only silhouettes/half-body scene sprites for the extra
  NPCs, but they must be separate from the background so quest/service state can
  be highlighted.

### 2. `hub_gate_outskirts` / `У ворот Последнего Поста`

Status: good background, weak location design.

Available:

- Background: `asset_location_gate_outskirts_bg`

Current data objects:

- none

Needed if it remains a playable location:

| Entity | Type | Current state | Required art |
| --- | --- | --- | --- |
| road sign / route marker | prop | missing data and sprite | small `scene_prop` showing route toward mill |
| return gate marker | exit | implicit only | subtle exit/waypoint sprite if player can act from this scene |
| road threat | enemy/combat | no location object | enemy `scene_character` or threat marker if combat happens here |
| travel clue / tracks | hotspot | missing data | optional scene prop only if the route teaches investigation |

Review notes:

- If this is only a travel transition, background-only is acceptable.
- If it is a real main-screen region, it needs at least one interactive object.
  A completely empty `objects: []` location will feel unfinished compared with
  the city and the mill.
- Do not invent many props yet. One route marker plus one threat/hotspot is
  enough for first playable clarity.

### 3. `old_mill` / `Старая мельница`

Status: background accepted after removing the pasted Black Sun overlay from the
main scene.

Available:

- Background: `asset_location_old_mill_bg`
- Black Sun mark prop: `asset_object_black_sun_mark`, but it should not be
  drawn as a large sign on the building facade.
- Enemy cards: `asset_enemy_mill_scavenger`,
  `asset_enemy_cellar_knifeman`
- Combat actor: `combat_actor_mill_scavenger` exists; cellar knifeman does not.

Needed scene sprites:

| Entity | Type | Current state | Required art |
| --- | --- | --- | --- |
| `old_mill.main_yard` / mill scavenger | combat object | enemy card exists; scene sprite missing | visible yard enemy/threat sprite |
| `old_mill.black_sun_mark` | hotspot/clue | prop exists but looked alien as facade overlay | use as small inspect-detail art, cellar-wall prop, or regenerated integrated clue |
| `old_mill.cellar_knifeman` | combat object | enemy card exists; combat actor missing | cellar knifeman scene sprite and combat actor |
| grain sacks / bread contract cargo | quest prop | item icon exists for `grain_sacks`; no scene prop | optional prop for reward/turn-in clarity |
| cellar entrance | hotspot/transition | implicit only | small door/hatch prop if cellar combat is selected from scene |

Review notes:

- The mill background already carries the place identity. Do not paste symbolic
  signs over it unless they are integrated into the surface and scale.
- The Black Sun clue should be discovered through an inspect state, cellar view,
  or close-up modal. On the main screen it should be a subtle hotspot, not a
  large black emblem on the mill.
- The first missing visible sprite is the yard threat. Without it, the location
  reads as a static painting and the combat object is only a list entry.

## Future Draft Locations

These locations exist in `locations.json`, but their background asset ids are
not present in `asset_manifest.json` and they are not supported by
`render_hub_scene.c` background selection yet.

### `forest_road` / `Лесная дорога`

Current data:

- `forest_road.cutthroat`
- `forest_road.cart_tracks`

Needed:

- background `asset_location_forest_road_bg`
- road cutthroat scene sprite and combat actor
- cart tracks scene prop; current `asset_icon_route_clue` is an item/UI icon,
  not a scene prop
- optional broken cart/wheel prop if this becomes a missing-cart quest location

### `ash_convoy` / `Пепельный обоз`

Current data:

- `ash_convoy.scout`
- `ash_convoy.chain_bearer`

Needed:

- background `asset_location_ash_convoy_bg`
- ash trail scout scene sprite and combat actor
- chain bearer scene sprite and combat actor
- burned wagon / chain cargo prop to make the location readable before combat

### `black_sun_hideout` / `Тайник Черного Солнца`

Current data:

- `black_sun_hideout.acolyte`
- `black_sun_hideout.relic_warden`
- `black_sun_hideout.captain`

Needed:

- background `asset_location_black_sun_hideout_bg`
- acolyte, relic warden, and captain scene sprites
- acolyte, relic warden, and captain combat actors
- hideout entrance, relic/altar, proof/clue props

Review notes:

- These should not be generated before the current three-scene sprite layer is
  coherent. Otherwise the project will have many backgrounds but no playable
  scene language.

## Combat And Item Art Review

Items:

- 37 item icons are referenced and available.
- 15 of those are procedural placeholders from `release_content_30m_01`.
- Item icons are acceptable for inventory/reward/shop readability, but not for
  scene props.

Enemies:

- Enemy cards exist for the location encounters.
- Distinct combat actors currently exist only for hero, gate scavenger, and
  mill scavenger.
- Needed distinct combat actors:
  - `mill_brute`
  - `black_sun_runner`
  - `cellar_knifeman`
  - `night_attacker`
  - `road_cutthroat`
  - `ash_trail_scout`
  - `chain_bearer`
  - `black_sun_acolyte`
  - `relic_warden`
  - `black_sun_captain`

## Priority Backlog

### P0: Current Main-Screen Coherence

Do these before expanding to more regions:

1. Generate/source scene sprites and portraits for city NPCs:
   `blacksmith`, `town_trader`, `elder`, `healer`.
2. Add city props:
   `contract_board`, `dragon_memorial`, `map_gate`, `caged_scavenger`.
3. Add old mill scene sprites:
   `mill_scavenger`, `cellar_knifeman`, subtle/integrated clue treatment.
4. Decide whether `hub_gate_outskirts` is a true interactive region. If yes,
   add one route prop and one threat/hotspot object.
5. Fix `town_trader` art ids so the trader does not reuse blacksmith assets.
6. Fix the elder mojibake in `locations.json`.

### P1: Combat Quality

1. Add distinct combat actors for all encounters used in current and near-future
   quests.
2. Stop routing every non-mill enemy to `combat_actor_gate_scavenger`.
3. Keep enemy cards as UI thumbnails only.

### P2: Future Locations

1. Add manifest entries and backgrounds for `forest_road`, `ash_convoy`, and
   `black_sun_hideout`.
2. Add scene props that make each location readable before the player opens a
   list.
3. Wire the new backgrounds into `render_hub_scene.c` or move the scene renderer
   toward data-driven background selection.

## Production Rule

For every location object in `locations.json`, answer these before accepting the
asset:

1. Is it visible in the main scene, map, combat screen, dialogue, inventory, or
   only in data?
2. If visible in the main scene, does it need a transparent `scene_character` or
   `scene_prop` sprite?
3. If it can change state, glow, unlock, be selected, or be hidden by quest
   requirements, it must be separate from the background.
4. If it is only decorative and never interactive, keep it baked into the
   background.
5. If it is an enemy, it needs both a scene/threat representation and a combat
   actor, not just an enemy card.

