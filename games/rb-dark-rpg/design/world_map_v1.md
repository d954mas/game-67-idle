---
type: Game Design Spec
title: World Map V1
description: Visual-region map contract for RB Dark RPG.
tags: [world-map, uiux, locations, implementation-contract]
game_id: rb-dark-rpg
status: draft
updated: 2026-07-05
---

# World Map V1

Goal: replace the current abstract location graph with a visual map where the
player understands the region, current position, reachable places, roads,
objects, NPCs, and enemies before opening a detail list.

Primary reference:
[Legacy Of Dragons World Map Reference](knowledge/sources/legacy_of_dragons_world_map_reference_2026-07-05.md).

Existing project flow already expects this lane:
`hub_last_post -> ash_border_map -> location_old_mill -> autobattle`.
The old `node map` wording in `data/ui_flow.json` was a design mismatch; the
screen should now be treated as a visual region map.

## Problem

The current map is a technical route graph:

- `world_map_screen.c` renders `world_map/graph_canvas`.
- Locations become node buttons.
- Exits become orthogonal edge rectangles.
- NPCs, enemies, exits, quest boards, and hotspots are only visible in
  `Место` rows after leaving map mode.

That is useful for debugging movement, but it fails the intended fantasy-RPG
map job. The player should see a place, not a graph.

## Product Target

The map screen must answer six questions at a glance:

1. Where am I?
2. Which region am I in?
3. What does this region physically look like?
4. What NPCs, enemies, services, quests, and objects exist here?
5. Where can I go next?
6. Is my hero standing here now, selecting a destination, or already moving?

## V1 Scope

The first production slice is one authored region:

- region id: `ash_border`
- display name: `Пепельная граница`
- current hub: `Последний Пост`
- locations inside:
  - `hub_last_post`
  - `hub_gate_outskirts`
  - `old_mill`
- surrounding unknown terrain: dim/fogged border outside the region.
- first path support: map opens after the hub/onboarding unlock and leads toward
  `Старая мельница`, then into the focused autobattle screen.

Do not build a whole world-map editor in V1. Author the first region data by
hand and keep the schema small enough to extend later.

## Visual Grammar

### Visual Quality Bar

The target is a beautiful, saturated fantasy map like the Legend reference, not
a functional diagram. A first accepted screenshot must feel like an illustrated
place before any marker is clicked.

- The map must have a terrain-art base: ash ground, road, settlement/palisade,
  mill, broken crossing, dead trees, fog, and small landmarks.
- Region and location boundaries must be authored organic shapes. Rectangles,
  grid cells, circles connected by lines, and generic blobs fail this pass.
- Roads must look like roads in the terrain, not UI connectors.
- Interaction color must be deliberate: muted terrain base, then high-saturation
  state colors for current region, current location, quest target, NPC, enemy,
  and blocked path.
- Marker icons need category silhouettes plus color. Do not rely on text labels
  alone.
- Unknown areas should be visually present under fog/dim, so the world feels
  bigger than the unlocked slice.
- The frame and controls can be ornate, but the map art must remain the visual
  hero.

### Art Pipeline Contract

Reference:
[RPG World Map Art Pipeline Reference](knowledge/sources/rpg_world_map_art_pipeline_reference_2026-07-05.md).

The next quality step is not "draw nicer polygons in code". The player-facing
map should be built from a marker-free painted atlas base and a separate
runtime overlay:

- `ash_border_map_base.png`: terrain, roads, palisade, gate, old mill, fog,
  shadows, landmarks, and surrounding unknown world context.
- overlay/content data: region polygon, road anchors, location anchors, object
  marker offsets, hero/current-position anchor, destination anchor, and label
  safe zones.
- runtime render: region/current/selected highlights, hero marker, destination
  flag, NPC/enemy/service/quest markers, labels, tooltips, and click targets.

Code may generate masks, overlays, and postprocessing, but it should not be the
primary author of the final map art. If AI image generation is used, generate
only the marker-free painted base; exact gameplay markers and coordinates must
come from authored data.

The base map must pass a no-markers readability check: with gameplay markers
hidden, the viewer should still understand `Last Post -> gate -> old mill`,
the main road, the region silhouette, and the dangerous/unknown outer terrain.

### Atlas

- The map is a terrain image or composed map-art layer, not a flat UI panel.
- It shows roads, palisade/settlement silhouette, ash fields, old mill, broken
  crossings, dead trees, danger marks, and fogged unknown surroundings.
- The selected/current region is an irregular polygon-like shape. It must not
  read as a rectangle, card, grid, or graph node cluster.

### Region

- The current region has a warm highlighted outline and subtle translucent fill.
- Locked or unknown neighboring regions may be hinted by fog, dashed boundary,
  or darkened terrain, but V1 does not need clickable neighbors.
- Internal subareas can be separated by path lines, fences, ravines, or terrain
  seams.

### Roads And Crossings

- Movement connections are shown as roads, gates, bridges, and trails.
- Do not draw abstract l-shaped connectors.
- Locked roads should remain visible but blocked by a gate, debris, fog, or
  warning marker.

### Location Markers

Location markers sit on the map, not in a separate first-read list:

- current location: strongest highlight / pulse / ring.
- reachable location: clear marker with readable icon.
- known but locked location: dim marker plus blocked affordance.
- undiscovered location: absent or only hinted by fog, not a full marker.

### Hero And Travel State

The map must support a visible hero/current-position layer:

- idle/current: hero marker or ring sits on the current location or road.
- selecting destination: selected target has its own highlight or flag, distinct
  from the hero marker.
- moving/travel-in-progress: if movement is delayed, show the hero on the route,
  destination flag, and a compact timer/progress label.
- arrived/ready: current-position ring resolves onto the destination location.

V1 may keep actual movement instant if the game loop does not need travel delay
yet, but the visual contract must leave room for the live map state from the
Legend reference.

### Object Markers

Objects inside the current/selected location should be visible as small category
markers:

- NPC: person/portrait marker.
- enemy/threat: red danger marker.
- quest board / quest source: notice or star marker.
- healer/service/vendor: service marker.
- inspect hotspot: eye/lens marker.
- exit/crossing: gate/road marker.

V1 does not need unique art for every NPC. Category markers plus a details
sheet are enough if the map still reads as a place.

## Interaction Model

### Desktop

- `Карта` opens a large map modal or map surface.
- Hover/select a region or location to update the header/details.
- Click a reachable location to move or open its selected-location detail.
- If movement uses a delay, keep the map readable while showing hero position,
  destination flag, and timer/progress instead of hiding travel in text.
- Click an NPC/enemy/object marker to open the same action that `Место` exposes:
  dialogue, pre-fight card, healer/service, inspect, or quest list.
- Provide zoom in/out and center-current controls only if the map exceeds the
  viewport. Controls should be icon buttons, not text buttons.

### Phone

- Map occupies the main visible area.
- Hero/current-position marker remains visible without relying on header text.
- Details use a bottom sheet with the selected location title and compact
  marker list.
- Primary action stays in the bottom sheet.
- Marker hitboxes are touch sized; text must not overlap the map.

### Header

Use a breadcrumb-style line:

- current: `Пепельная граница > Последний Пост`
- selected region: `Пепельная граница`
- selected location: selected location or object name

The header can be compact, but it must distinguish current and selected state.

## Data Contract

Keep existing location ids stable. Add map-region data around them.

Recommended authored shape:

```json
{
  "regions": [
    {
      "id": "ash_border",
      "display_name": "Пепельная граница",
      "atlas_asset_id": "asset_map_region_ash_border",
      "polygon": [[0.12, 0.18], [0.72, 0.10], [0.88, 0.62], [0.48, 0.86], [0.10, 0.64]],
      "roads": [
        {"from": "hub_last_post", "to": "hub_gate_outskirts", "kind": "gate_road"},
        {"from": "hub_gate_outskirts", "to": "old_mill", "kind": "broken_road"}
      ]
    }
  ]
}
```

Recommended location extensions:

```json
{
  "id": "hub_last_post",
  "region_id": "ash_border",
  "map": {
    "x": 0.28,
    "y": 0.38,
    "icon": "settlement",
    "label_priority": 1
  }
}
```

Recommended object extensions:

```json
{
  "id": "hub_last_post.gate_guard",
  "kind": "npc",
  "map": {
    "x": 0.23,
    "y": 0.41,
    "icon": "npc"
  }
}
```

If object marker positions are absent, V1 may cluster object markers around the
location marker by category. Do not block the map pass on perfect object
coordinates.

## Runtime Contract

- Use existing `game_content` location/object ids.
- Preserve `game_actions_can_move_location`, `game_actions_move_location`, and
  object interaction selection.
- Replace only the presentation layer first: map visuals can call the same
  actions as current map/place rows.
- Keep map travel state separate from location unlock state. The presentation
  should be able to show idle/current, selected destination, moving, and arrived
  even if V1 starts with instant movement.
- Keep `Место` as the focused list/detail mode, but make `Карта` preview what
  the selected place contains.
- User-visible labels must use the engine text renderer and packed fonts.
- Internal map logic stays Y-up; convert to UI coordinates at the render/input
  boundary.

## Acceptance Criteria

- The map no longer contains `graph_canvas`-style node/edge presentation in the
  player-facing view.
- The current region has a non-rectangular highlighted boundary.
- `Последний Пост`, gate outskirts, and old mill appear as geographic places on
  one terrain map.
- Roads/trails/gates show how locations connect.
- Current location is visibly marked.
- Hero/current-position marker is visible on the map, not only in HUD or text.
- Selected destination, current position, and optional travel-in-progress timer
  are visually distinct.
- NPC, enemy, service/quest, hotspot, and exit markers are visible for the
  selected/current region.
- Selecting a marker exposes the correct action: dialogue, pre-fight card,
  service, inspect, or movement.
- Locked locations are visually different from reachable ones without
  disappearing into confusion.
- Desktop and phone screenshots show no text overlap and no cramped controls.
- The screenshot proof shows an actual region map, not a diagram.

## Out Of Scope For V1

- A full world-map editor.
- Procedural region generation.
- Exact Legend-style icon taxonomy or tooltip behavior.
- Copying Legend assets or ornate frame art.
- Multi-region travel beyond the first authored `ash_border` region.

## Next Proof

After implementation, capture:

- desktop: `Карта` open at `Последний Пост`;
- phone portrait: same state with bottom detail sheet;
- selected `hub_gate_outskirts` threat marker opening the pre-fight card;
- selected `old_mill` locked/unlocked state after the relevant flag changes;
- hero/current-position marker visible on the map, plus a travel-in-progress
  route/timer state if delayed movement is enabled.
