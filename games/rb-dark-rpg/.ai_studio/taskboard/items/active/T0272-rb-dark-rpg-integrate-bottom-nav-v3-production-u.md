---
id: T0272
title: "rb-dark-rpg: integrate bottom nav v11 production UI"
status: review
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, uiux, assets, onboarding]
created: 2026-07-04
updated: 2026-07-04
---

## What

Integrate the accepted bottom navigation v11 production UI into the first
`Последний Пост` scene for `rb-dark-rpg`.

Artlead release: v11 is approved for implementation. It keeps the rugged
garrison-token direction, equal-size buttons, runtime text troughs, and the
standalone magnifying-glass `Место` icon with no tiles or extra props.

Current reference/source assets:

- canvas project: `canvas://rb-dark-rpg-9874a1`;
- v11 approved canvas group: `grp_7c4025a4` /
  `UI direction - standalone glass inspect nav tokens 11`;
- v11 approved canvas image element: `el_7f7482f8`;
- v11 source sheet:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/bottom_nav_component_sheet_11.png`;
- v11 atlas rects:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/atlas_manifest.json`;
- v11 slices:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/slices/`;
- v11 implementation handoff:
  `games/rb-dark-rpg/design/ui_ux/bottom_nav_v11_production_handoff.md`;
- v11 provenance:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/provenance.md`.

Runtime bottom nav order:

1. `Снаряж.`
2. `Дневник`
3. `Карта`
4. `Место`
5. `Еще`

V11 implementation requirements:

- same runtime size and hitbox, but different visual/material identity per
  button;
- varied icon colors, not an all-gold icon set;
- preserve the rugged garrison-token material direction;
- keep all five buttons identical in runtime dimensions;
- normalize source rect width variance from the atlas at runtime;
- runtime draws labels, badges, and counters with the engine text/UI layer;
- do not use baked text from generated art.

The nav sits over the lower fade band. Do not introduce a heavy bottom panel.
Do not add a separate talk button; the first action remains tapping the guard.

## Done when

- [x] Five bottom nav buttons render in the accepted order and have identical
  runtime width, height, baseline, and hitbox.
- [x] `Карта` is centered by position only and is not larger than the other
  buttons.
- [x] Artlead has released v5+ or explicitly approved an earlier candidate
  despite the known issues.
- [x] Button frames preserve corners/bevels when scaled.
- [x] Icons remain readable and are not clipped by a button mask unless they
  overflow their safe area.
- [x] Text labels are drawn by the runtime text renderer, not baked into PNGs.
- [ ] Lock/badge states use runtime overlay assets and do not change button
  layout.
- [x] Top and bottom HUD still read as fade bands, not hard panels.
- [x] Bottom nav does not cover the guard tap target or the first interaction
  guidance.
- [x] `Место` and `Еще` have compact bottom-sheet hooks or clear placeholders
  for that behavior, not full-screen modal behavior.
- [x] Asset/provenance references remain traceable through
  `atlas_manifest.json`.

## Open questions

- Final runtime state mapping for every button can stay minimal in the first
  pass: current/selected, locked, and notification badge are enough.

## Log

- 2026-07-04: Created from artlead handoff after bottom nav v3 was accepted,
  sliced, alpha-cut, and exported from canvas. V3 was later rejected for
  production because borders were too wide, central icon area was too small,
  and icons were too similar.
- 2026-07-04: V4 fixed the frame width but was rejected as too visually uniform.
- 2026-07-04: V5 restored per-button identity and varied icon colors, but felt
  too rainbow/odd in `Еще`.
- 2026-07-04: V6 made the palette more muted and replaced ellipsis with stacked
  plaques; later rejected because `Еще` should return to three dots.
- 2026-07-04: V7 returned `Еще` to three physical dots and simplified map;
  later rejected because the place/lantern icon was unclear.
- 2026-07-04: V10 changed `Место` to a magnifying-glass inspect icon, but still
  implied a specific object because tiles were visible under the glass.
- 2026-07-04: V11 candidate created after V10 review: `Место` uses only one
  standalone magnifying glass, with no stone tiles or other props under the
  lens. Evidence:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/bottom_nav_component_sheet_11.png`.
- 2026-07-04: Artlead approved V11 for implementation. Handoff and atlas rects
  added. Evidence:
  `games/rb-dark-rpg/design/ui_ux/bottom_nav_v11_production_handoff.md`,
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/atlas_manifest.json`,
  canvas group `grp_7c4025a4`, image element `el_7f7482f8`.
- 2026-07-04: V11 bottom nav is wired into runtime and pack builder:
  `bottom_nav.c` draws `ui/nav_v11_*` atlas regions, `first_screen_hud.c`
  places it over the bottom fade band, and `build_packs.c` packs all five v11
  button sprites. Verification passed: `cmake --build
  games/rb-dark-rpg/build/native-debug --target game`, `first_scene_tests`,
  `game_dialogue_test`, and `quality_responsive`. Responsive evidence:
  `tmp/quality/qclr_002_responsive/contact_sheet.png`.
- 2026-07-04: 2026-07-04: V11 runtime acceptance refreshed after interactive id/hitbox fix. Evidence: quality_responsive passed with stable ui/nav_v11_* hitboxes in ui.tree; contact sheet tmp/quality/qclr_002_responsive/contact_sheet.png; hook check tmp/bottom_nav_v11_hook/summary.json and place_sheet_after_click.png verify raw player click opens compact Place sheet without overlapping FTUE prompt; git diff --check passed.
