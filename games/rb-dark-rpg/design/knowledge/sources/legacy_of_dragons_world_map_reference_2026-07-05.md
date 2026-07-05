---
type: Project Source Notes
title: Legacy Of Dragons World Map Reference
description: Source notes for visual world-map regions, location contents, and map navigation for rb-dark-rpg.
tags: [project, references, world-map, uiux, locations]
timestamp: 2026-07-05T00:00:00Z
game_id: rb-dark-rpg
status: draft-source
source_quality: user-provided
checked: 2026-07-05
---

# Legacy Of Dragons World Map Reference

Scope: deconstruct the `Legend: Legacy of the Dragons` / `War of Dragons`
world-map screen as a visual-region reference for `rb-dark-rpg`.

This note supports the next world-map pass. It does not license copying the
map image, exact icons, borders, names, geography, or UI chrome.

## Source Matrix

| Source | Link/Path | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|
| User screenshot, world-map screen | `C:/Users/ROG/YandexDisk/Скриншоты/2026-07-04_11-26-07.png` | user-provided visual evidence | 2026-07-05 | The map is a painted terrain atlas with irregular highlighted regions, roads, settlements, rivers, icons for local contents, scroll/zoom controls, and breadcrumb text for current/selected area/location. | Exact hover behavior, tooltip wording, icon taxonomy, authenticated click path, or server-side map data. |
| User screenshot, live hero-on-map view | `C:/Users/ROG/Downloads/1412683972_4 (2).jpg` | user-provided visual evidence | 2026-07-05 | The map can be an active navigation surface: the hero token stands on the road inside the map with a current-position ring, flag/standard, movement timer, HUD, side actions, bottom region title, and quest feed still visible. | Exact movement rules, pathfinding, countdown semantics, server timing, or whether every map view shows the hero token. |
| Official Russian homepage | `https://w1.dwar.ru/` | official site | 2026-07-05 | The reference is the official live Russian game site and current public context. | The map interaction model; public homepage does not expose the authenticated map screen. |
| Official estate/help page | `https://w2.dwar.ru/info/library/?category_id=113` | official library/help | 2026-07-05 | The game has a global world map and a lower-left map-mode button for the estate/real-estate view. | Ordinary world-map region selection, NPC/enemy marker tooltips, or authenticated map microinteractions. |
| RAWG screenshot gallery | `https://rawg.io/games/legenda-nasledie-drakonov/screenshots` | public screenshot gallery | 2026-07-05 | A second map screenshot, `Кристаллические пещеры`, shows the same visual grammar in a different biome: ornate frame, current-location text, organic passable shapes, green crossing nodes, cave/track terrain art, and strong color accents. | Exact live interaction behavior, official provenance of every screenshot, or current production state. |
| Wikipedia overview | `https://ru.wikipedia.org/wiki/Легенда:_Наследие_Драконов` | secondary overview | 2026-07-05 | The game is a browser MMORPG with large content scale; the article reports hundreds of locations and many monster types in an older snapshot. | Map UI behavior or current live content counts; use only as secondary scale context. |
| Current rb-dark-rpg map code audit | `games/rb-dark-rpg/src/ui/world_map_screen.c` | current implementation evidence | 2026-07-05 | Current build renders a `graph_canvas`, map nodes, and edge segments from location coordinates/exits; place mode lists objects separately. | Final visual appearance without a fresh native screenshot. |
| Current rb-dark-rpg location data | `games/rb-dark-rpg/design/data/locations.json` | current content evidence | 2026-07-05 | Existing data already contains locations, map x/y, objects, object kinds, NPC character ids, combat encounter ids, hotspots, exits, and unlock requirements. | Region polygons, roads, terrain, icon positions inside a region, or visible map-art layers. |
| Current rb-dark-rpg UI/GDD flow | `games/rb-dark-rpg/design/gdd.md`, `games/rb-dark-rpg/design/data/ui_flow.json` | current design evidence | 2026-07-05 | The intended first path already includes hub -> map -> location -> autobattle, and `ash_border_map` is the first map screen. | A visual-region presentation; older wording still called it a node map before this review. |

## Source Ladder

- user-provided material: two world-map screenshots from the lead: one region
  overview and one live hero-on-map navigation view.
- official/store/trailer visuals: official homepage and official estate/help
  page checked; no public unauthenticated ordinary world-map screen found in
  this pass.
- raw gameplay evidence: lead screenshots plus public RAWG map screenshot. No
  video or authenticated live map capture.
- supporting interpretation: current `rb-dark-rpg` code/data audit; Wikipedia
  only for secondary scale context.
- source gaps / exceptions: exact hover/click behavior and tooltip contents are
  unknown. This packet is enough to redesign the visible map grammar, but not
  enough to clone Legend's exact microinteractions.

## Reference Lock

- mode: central deconstruction, scoped to world-map screen grammar and the
  rejected current-map mismatch.
- reference question: how to replace a node graph with a visual map containing
  shaped regions, current/selected location state, roads, terrain, objects,
  NPCs, and enemies.
- durable doc path:
  `games/rb-dark-rpg/design/knowledge/sources/legacy_of_dragons_world_map_reference_2026-07-05.md`.
- required source packet: lead screenshots, current map code, current location
  data, and later a native screenshot after implementation.
- current native capture path or capture plan: no fresh native capture in this
  pass; code audit proves the graph implementation. Next implementation pass
  must capture desktop and phone map screenshots.
- no-coding/no-final-art boundary: do not implement the visual map from memory
  alone. Use this packet plus `games/rb-dark-rpg/design/world_map_v1.md` as the
  first-pass contract.
- expected proof screenshot/scenario: open the `Карта` bottom-nav tab from
  `Последний Пост` on desktop and phone; verify a non-square highlighted
  region, visible road/settlement/objects, current location highlight, and
  NPC/enemy markers derived from content data. If movement delay is enabled,
  also capture a route in progress with the hero marker on the road, destination
  flag, and travel timer/state.
- unlock condition: implementation may start for the first visual-map pass
  after the contract exists; exact Legend-style tooltips remain a later
  source-gap item.

## Definition Of Ready Checklist

- [x] mode matches implementation risk.
- [x] source matrix is filled.
- [x] user-provided screenshot is linked as the primary visual evidence.
- [ ] six independent player-facing frames/screenshots exist. Gap accepted for
  this narrow visual-map pass because the lead supplied the target screenshot.
- [ ] gameplay footage/walkthrough or long screenshot sequence exists. Not
  available in this pass.
- [ ] current native capture exists. Replaced by current-code audit; native
  screenshots are required after implementation.
- [x] observation ledger has visible beats before conclusions.
- [x] borrow / avoid / copy-risk are explicit.
- [x] current-build mismatch is written.
- [x] next native proof is named.

## Reference Evidence Board

| Item | Source/timestamp/frame | What it proves | What it cannot prove |
|---|---|---|---|
| first screen | user screenshot | The map opens as a large framed atlas, not as a text list or abstract graph. | Opening animation or entry route. |
| current/selected state | top-left breadcrumb and highlighted region | The screen distinguishes current location, selected area, and selected location. | Whether selected state changes on hover or click. |
| visual region | yellow irregular boundary around right-side region | A region is a shaped geographic area with internal subdivision lines, not a square card. | Exact polygon data or hit masks. |
| terrain grammar | roads, river, lake, buildings, walled city, forest, fog | The map explains place identity through geography and landmarks before reading text. | Exact map-art pipeline. |
| local contents | blue person markers, red enemy markers, yellow/green/other markers | NPCs/enemies/objects are exposed inside the region as map icons. | Exact icon taxonomy; several marker meanings are inferred. |
| navigation controls | scrollbars plus bottom-left zoom/reset cluster | Large map is navigable with pan/scroll and zoom/center controls. | Keyboard shortcuts or wheel behavior. |
| repeated map grammar | RAWG `Кристаллические пещеры` screenshot | The same approach works for non-overworld biomes: irregular navigable shapes, strong route/crossing markers, thematic background art, and current-location header. | Whether this screenshot reflects the current live map client. |
| hero/travel state | second user screenshot, `Дымные сопки` map | The hero can be rendered directly on the map with a current-position ring, destination flag/standard, movement timer, and gameplay HUD still active. | Whether movement is real-time, queued, server-authoritative, or partly cosmetic. |

## Observation Ledger

| Beat | Source | Visible screen state | Player action | Visible response | Reward/UI feedback | Inferred meaning |
|---|---|---|---|---|---|---|
| 1 | screenshot | Full map atlas inside ornate frame; right side is a highlighted region; neighboring terrain remains visible but dimmer/fogged. | Open map. | Current map context appears at once. | Top text reads current location and selected area/location. | The map is a spatial orientation tool first, not a route table. |
| 2 | screenshot | Region outline is organic and follows terrain/settlement edges. Internal boundaries split smaller places. | Select or inspect a region. | Region is emphasized with bright yellow outline and translucent fill. | Selected area/location text updates in the header. | Region selection is a visual state, not only a row selection. |
| 3 | screenshot | Roads connect gates, buildings, bridges, and subzones; rivers/lakes cut through the region. | Read route visually. | Player can infer how areas connect before clicking. | Yellow gate-like markers sit at border crossings. | Roads and crossings are the map's primary movement grammar. |
| 4 | screenshot | Blue person icons, red creature icons, and other activity icons sit inside the selected region. | Inspect a marker. | Marker communicates local contents without opening a separate list first. | Icon color/silhouette indicates category. | NPCs, enemies, quest/service/resource objects should be visible on the region map. |
| 5 | screenshot | Bottom-left controls include minus, reset/center-like control, plus, and compass-like control; right/bottom scrollbars show map can move. | Pan or zoom. | View can shift across a map larger than the viewport. | Controls remain in a fixed map-corner cluster. | The map is a navigable atlas viewport, not a fixed mini diagram. |
| 6 | RAWG screenshot | Cave map uses the same frame/header grammar but swaps green fields for caves, rail tracks, crystal clusters, green crossing markers, and isolated passable islands. | Compare biome. | The map remains readable even when the terrain material changes. | Color accents identify passages and points of interest. | The visual system is a reusable map language, not one hand-made screen. |
| 7 | second user screenshot | Hero token stands on the road inside `Дымные сопки`; a yellow ring marks current position; a red flag/standard and `00:17` timer sit near the character. | Choose a destination or travel route, inferred from the visible timer. | The map shows progress/travel state without leaving the map. | HUD, side actions, bottom title plaque, and quest feed remain visible. | The map is also a live navigation surface, not only a modal selector. |

## Screen Grammar

- `observed` The map has three visible hierarchy levels:
  1. whole world/atlas context, where neighboring areas remain visible;
  2. selected area/region, highlighted as an irregular land shape;
  3. selected location/subzone, named in the header and shown through local
     boundaries, roads, buildings, and markers.
- `observed` The current/selected state is communicated redundantly: header
  breadcrumb text, bright outline, and visual focus.
- `observed` The region contains authored geography: roads, water, buildings,
  settlement walls, bridges/crossings, vegetation, and fog.
- `observed` Local contents are spatial markers inside the region, not a
  separate abstract list.
- `inferred` Blue person icons represent NPCs or services; red icons represent
  enemies/threats; other symbols represent quests/resources/services/points of
  interest. Exact taxonomy is unknown.
- `observed` Map navigation uses scrollbars and a zoom/control cluster.
- `observed` A separate screenshot shows the hero token standing on the map,
  with a current-position ring, flag/standard, and timer while the normal HUD
  and quest feed remain active.
- `observed from official help` The game exposes a global world map and a
  lower-left map-mode button for estate-related map behavior.

## Visual Art Deconstruction

The "juicy" look is not just decorative polish. It comes from layered map
information where every visual layer has a job.

### Composition

- `observed` The map uses a full atlas surface with an ornate frame, not a
  floating UI card. The map feels like an object in the game world.
- `observed` The selected region occupies a large readable mass, while the
  surrounding world remains visible. This gives context without making the
  active region tiny.
- `observed` There is no empty flat background. Even inactive areas contain
  terrain, roads, water, buildings, walls, and fog.

### Region Shape

- `observed` Region borders are irregular and follow natural/settlement
  geography. The highlighted area bends around roads, walls, water, and
  terrain.
- `observed` Internal subregions are also organic. They look like fields,
  streets, fenced lots, cave chambers, or terrain pockets, not UI cells.
- `borrow` For `rb-dark-rpg`, the first region shape should feel authored:
  palisade, ash road, mill field, broken crossing, and fog edge should drive
  the outline.

### Terrain And Landmarks

- `observed` The map is readable before icons: road network, lake/river,
  settlement walls, buildings, bridges, trees, and fog tell the player what the
  place is.
- `observed` Landmarks are small but specific. The walled city, villages,
  waterways, bridges, and scattered houses make the map memorable.
- `observed` The cave screenshot keeps the same idea with different materials:
  cave floor islands, rail tracks, crystals, glowing zones, and green passages.
- `borrow` Each `rb-dark-rpg` map region needs 3-5 unique landmarks before
  marker icons are added. Otherwise it will read as a themed UI panel, not as a
  place.

### Color And Value

- `observed` Base terrain is muted and textured. The bright colors are reserved
  for state and interaction: yellow-green borders, yellow crossings, blue NPC
  markers, red danger markers, gold/green activity markers.
- `observed` The right side fog/dim layer lowers contrast in non-focused areas
  while still revealing geography beneath it.
- `borrow` The first `rb-dark-rpg` map should use a low-saturation ash/earth
  base, then spend saturation on active state: warm amber current-region
  outline, red enemy/threat markers, blue/steel NPC markers, gold quest/service
  markers, and pale fog for unknown zones.

### Icon Grammar

- `observed` Icons are symbolic but placed spatially. They sit on roads,
  buildings, clearings, or edges, so the icon is tied to the actual place.
- `observed` Category colors and silhouettes separate local contents without a
  legend-heavy side panel.
- `inferred` Blue person markers likely mean NPC/service; red face/skull-like
  markers mean enemies; green/gold symbols mean resources, services, quests, or
  special objects. Exact taxonomy is unknown.
- `borrow` In `rb-dark-rpg`, marker categories must have different silhouettes,
  not only different colors, so the map works on phone and for color-weak
  players.

### UI Chrome

- `observed` The ornate frame is thin relative to the map area. It adds tone
  without consuming the screen.
- `observed` The header text block sits over the map but does not hide the
  selected region's core geometry.
- `observed` Scrollbars and bottom-left controls are visible but subordinate.
  They do not compete with the map itself.
- `avoid` Do not copy Legend's exact frame or buttons. The safe borrow is a
  framed atlas object with compact controls and breadcrumb state.

## Live Hero-On-Map Navigation

The second lead screenshot adds a separate requirement: the map can act as the
place where travel is displayed, not just the place where travel is selected.

- `observed` The hero token stands directly on the route inside the map.
- `observed` A yellow ring under the hero marks the exact current/progress
  position.
- `observed` A red flag/standard and `00:17` timer appear near the hero,
  strongly implying movement or travel-in-progress feedback.
- `observed` Combat/player HUD, top resources, side action buttons, bottom
  region title, and quest feed remain visible while the map is active.
- `inferred` Selecting a destination likely starts a travel/wait state that
  keeps the player on the map until arrival. Exact timing and pathfinding are
  unknown.
- `borrow` `rb-dark-rpg` should support a visible player marker on the authored
  region map. When moving from `Последний Пост` toward the gate or old mill, the
  map should be able to show a destination flag and optional short travel
  timer/progress state.
- `avoid` Do not copy Legend's exact hero pose, flag, timer frame, resource HUD,
  side buttons, or live-service clutter. The safe translation is the state model:
  current hero position, destination, and travel feedback on the map.

## Marker Taxonomy From The Screenshot

Observed or inferred categories that matter for `rb-dark-rpg`:

| Category | Evidence | Map Role | RB Dark RPG Translation |
|---|---|---|---|
| Current/selected location | Header and highlighted region/location | Player orientation | Strong current marker and breadcrumb. |
| Hero/current position | Second screenshot hero token, yellow ring, timer | Live player orientation and travel feedback | Player token/ring on current road or location; optional destination flag and short travel state. |
| Region/subregion boundary | Yellow-green organic lines | Scope and selection | Irregular polygon outline and internal terrain seams. |
| Road/crossing | Light roads, bridges, yellow gate marks | Movement grammar | Roads, gate, broken crossing, locked debris/fog. |
| NPC/service | Blue person markers, inferred | Who is here | Guard, blacksmith, elder, healer markers. |
| Enemy/threat | Red markers, inferred from danger color/silhouette | What can fight you | Gate scavenger and mill enemies. |
| Quest/special object | Gold/star/anchor-like symbols, inferred | What matters now | Quest board, Black Sun sign, clue hotspot. |
| Resource/activity | Green plant-like symbols, inferred | Repeatable or environmental activity | Future gathering/resource points; not needed in first pass unless tied to a quest. |

## Quality Bar For "Beautiful And Juicy"

Minimum visual quality before accepting a first `rb-dark-rpg` map screenshot:

- The map still reads if all markers are hidden. Terrain, roads, and landmarks
  alone must explain the place.
- The active region is an authored non-rectangular shape, not a box, blob, or
  Voronoi placeholder.
- There is at least one strong local landmark per playable location.
- Roads are drawn as terrain objects, not UI lines.
- Unknown/locked areas use fog, blockers, or dimmed terrain, not just disabled
  buttons.
- Icon categories use both silhouette and color.
- Current location, selected location, quest target, and enemy threat are
  visually distinct in one screenshot.
- Saturation is concentrated on interaction/state; the terrain base stays
  textured and readable.
- The frame/control layer supports the map fantasy but does not become the main
  visual mass.
- Desktop and phone captures must both look like a map, not a cramped menu.

## Current rb-dark-rpg Mismatch

- `observed` `world_map_screen.c` uses `WORLD_MAP_MODE_MAP` and
  `WORLD_MAP_MODE_PLACE` as two separate modes.
- `observed` map mode renders `world_map/graph_canvas`, edge rectangles, and
  node buttons from `location.map.x/y` and `location.exits`.
- `observed` place mode then lists exits and objects through rows.
- `observed` `locations.json` already has the content that a visual map needs:
  NPCs, enemies, quest board, healer, hotspots, exits, encounters, and map
  coordinates.
- `mismatch` The player sees topology, not place. Current map answers "which
  node connects to which node", but the requested map must answer "what is this
  region, where am I, what is inside this zone, and what can I do here".
- `mismatch` Current graph nodes are geometric UI widgets; reference regions
  are geographic shapes with terrain and landmark meaning.
- `mismatch` NPCs/enemies are hidden in place rows; reference exposes them
  directly on the region map.
- `mismatch` Current edges are orthogonal l-shaped connectors; reference uses
  roads, gates, bridges, and paths as readable world geography.

## Borrow

- Build the world map as a painted/illustrated atlas viewport with authored
  regions and terrain, not as a graph.
- Give each region an irregular boundary polygon with a highlight state.
- Keep current and selected state visible in the header and on the map.
- Show the hero/current-position marker directly on the map, with optional
  destination flag and travel-in-progress timer/state when movement is delayed.
- Draw roads, crossings, landmarks, water/terrain, and settlement silhouettes
  before icons.
- Place NPC/enemy/object icons inside the region, derived from authored
  location objects.
- Provide pan/zoom or at least viewport controls when the map is larger than
  the modal.
- Use the map to preview local contents before opening the focused `Место`
  panel.

## Avoid

- Do not copy Legend's exact map image, border ornament, icons, geography,
  names, faction markers, hero token, timer frame, HUD layout, or color shapes.
- Do not keep a square node/card grid and call it a map.
- Do not hide region contents behind a separate list as the first read.
- Do not overpack every system into the first pass. For `rb-dark-rpg`, show
  NPC, threat, quest/service/hotspot, exit/crossing, and current-location
  markers first.
- Do not use handcrafted debug text or shape-only placeholders for final
  product UI labels; runtime text must use the engine text renderer.

## Copy-Risk

- Exact Legend map assets, ornate frame, marker icons, colors tied to their
  game semantics, hero token/flag/timer presentation, location names, and world
  layout are protected reference material.
- Safe translation is structural: shaped region boundaries, map-as-place,
  terrain-first readability, visible local contents, and current/selected state.

## RB Dark RPG Translation

For `rb-dark-rpg`, the first map should be the `Ash Border` / `Последний Пост`
region. It should show one non-square region with:

- `Последний Пост` as the current hub/settlement;
- the gate/outskirts threat outside the post;
- the old mill as a locked or newly unlocked quest location;
- a road from post to gate and onward to the mill;
- region terrain: ash fields, broken road, palisade, mill, sparse dead trees,
  warning markers, and fog/dimmed unknown surroundings;
- icons for guard/blacksmith/elder/healer/quest board at the post, enemies at
  the gate and mill, and a hotspot marker for the Black Sun sign when relevant.

## Implementation Gate

- next code/art pass: replace `world_map/graph_canvas` with a visual atlas map
  component that renders region background, region polygon highlight, roads,
  location icons, object markers, and a selected-location details sheet.
- minimum data change: add `regions` / `region_id` / polygon or path data /
  icon map positions to the authored content model without breaking existing
  `locations.json` ids.
- proof: desktop and phone screenshots showing the current region highlighted,
  current location and hero/current-position marker visible, local
  NPC/enemy/object markers visible, and no abstract node/edge graph. If travel
  delay exists, proof must also show the in-progress route state.

## Reference Digest

- mode: central deconstruction scoped to visual world-map grammar.
- sources checked: user screenshots, official homepage, official estate/help
  page, current map code, current location data, current GDD/UI flow.
- observed facts:
  - the reference map is a large painted atlas with scroll/zoom controls;
  - regions use irregular highlighted boundaries, not square nodes;
  - the header distinguishes current location, selected area, and selected
    location;
  - roads, rivers, buildings, gates, and fog explain the place visually;
  - NPC/enemy/object markers are visible inside the selected region;
  - the hero can be visible directly on the map with a current-position ring,
    flag/standard, and movement timer.
- current-build mismatch: `rb-dark-rpg` currently draws a graph canvas with
  nodes and orthogonal edges, while local contents live in a separate place
  list.
- borrow: shaped regions, terrain-first map, visible contents, current/selected
  state, hero/current-position marker, optional travel state, road/crossing
  movement grammar.
- avoid: copying Legend's exact art/icons/ornament and keeping an abstract graph.
- copy-risk: exact map image, icon silhouettes/colors, hero/flag/timer art,
  geography, and protected names.
- next native proof: `Карта` open from `Последний Пост` on desktop and phone,
  showing a visual region map with NPC/enemy markers, a hero/current-position
  marker, and no graph edges.
