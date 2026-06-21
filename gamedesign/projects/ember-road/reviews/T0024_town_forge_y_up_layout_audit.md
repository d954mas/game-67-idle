---
type: LayoutAudit
project: ember-road
task: T0024
surface: town-forge-native-v2
verdict: pass
---

# T0024 Town Forge Y-Up Layout Audit

Verdict: **PASS**

## Source Boundary Evidence

- renderer boundary conversion: `src/clean_seed_main.c:97`
- Y-up hit testing: `src/clean_seed_main.c:102`
- input boundary conversion: `src/clean_seed_main.c:988`
- DevAPI boundary conversion: `src/clean_seed_main.c:1308`
- town forge logical box: `src/clean_seed_main.c:186`
- source-derived action plaque: `src/clean_seed_main.c:392`
- source-derived result strip: `src/clean_seed_main.c:685`

## Runtime Layout

Coordinates below are converted back from DevAPI screen rectangles to logical Y-up rectangles.

- `ember.map.old_gate`: x=104.0, y=154.0, w=150.0, h=54.0
- `ember.map.north_road`: x=306.0, y=168.0, w=160.0, h=58.0
- `ember.map.old_mine`: x=534.0, y=154.0, w=150.0, h=54.0
- `ember.scene.forge_workbench`: x=554.3, y=266.0, w=250.0, h=150.0
- `ember.town.lantern_upgrade`: x=878.0, y=172.0, w=324.0, h=62.0
- `ember.forge_mine_lantern`: x=878.0, y=172.0, w=324.0, h=62.0
- `ember.primary`: x=878.0, y=172.0, w=324.0, h=62.0

## Checks

- PASS `source:renderer boundary conversion` - static float sy(float y, float h)
- PASS `source:Y-up hit testing` - static bool contains_y_up
- PASS `source:input boundary conversion` - const float y_up = s_view_h - pointer.y;
- PASS `source:DevAPI boundary conversion` - const float y_down = s_view_h - box.y - box.h;
- PASS `source:town forge logical box` - s_forge_workbench_box = (UiBox)
- PASS `source:source-derived action plaque` - FORGE_ACTION_PANEL_V2
- PASS `source:source-derived result strip` - FORGE_RESULT_STRIP_SLICE9_V2
- PASS `state:town lantern upgrade open` - {"action_result": "ok", "message": "Returned to Old Gate.", "shape_index": 0, "shape": "cube", "render_mode_index": 0, "render_mode": "solid_wire", "camera_distance": 6, "test_ui_clicks": 0, "test_label_text": "Ember Roa
- PASS `runtime:viewport height` - 720.0
- PASS `runtime:node:ember.map.old_gate`
- PASS `runtime:node:ember.map.north_road`
- PASS `runtime:node:ember.map.old_mine`
- PASS `runtime:node:ember.scene.forge_workbench`
- PASS `runtime:node:ember.town.lantern_upgrade`
- PASS `runtime:node:ember.forge_mine_lantern`
- PASS `runtime:node:ember.primary`
- PASS `layout:route nodes ordered left-to-right`
- PASS `layout:forge is above route strip in Y-up` - forge=341.0, mine=181.0
- PASS `layout:forge is left of right rail action` - forge=679.3, action_x=878.0
- PASS `layout:right rail action stays below forge event` - action=203.0, forge=341.0
- PASS `input:ui.click forge workbench triggers lantern` - {"shape_index": 0, "shape": "cube", "render_mode_index": 0, "render_mode": "solid_wire", "camera_distance": 6, "test_ui_clicks": 0, "test_label_text": "Ember Road: Mine Lantern ready. Depth 2 route is lit.", "test_button
- PASS `input:ui.click unlocks depth 2` - {"shape_index": 0, "shape": "cube", "render_mode_index": 0, "render_mode": "solid_wire", "camera_distance": 6, "test_ui_clicks": 0, "test_label_text": "Ember Road: Mine Lantern ready. Depth 2 route is lit.", "test_button

## Notes

- Game/world/UI boxes remain authored in logical Y-up coordinates.
- Renderer/input/DevAPI conversion is boundary-only and named in code.
- This audit is proof for the current town forge native screen, not acceptance of broader Depth 2 content.
