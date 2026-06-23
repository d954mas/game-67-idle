---
id: T0109
title: "M2: city - multiple lots, neighborhood map, travel between houses"
status: review
epic: E007
priority: P2
tags: [little-lives, 3d, milestone-2, city]
created: 2026-06-22
updated: 2026-06-22
---

## What

Grow the single lot into a neighborhood: several house lots with roads, multiple
households, an overview map, and travel between houses.

## Done when

- [x] Multiple 3D lots render (2x2 around a cross-road with centerlines + lawns).
- [x] Each lot is a furnished household with its own Sims (4 lots x 3 = 12 Sims).
- [x] Travel between households (key N, click the lot strip, or DevAPI
      game.action.travel {lot}); active lot highlighted, others dimmed.
- [x] Neighborhood overview/map camera (key M / MAP button; click a house to enter).
- [x] Player commands + selection scoped to the active household; AI runs for all.
- [x] Native build + DevAPI verify + screenshot.

## Open questions

- Cross-lot visiting (a Sim walking to a neighbor's house) is minimal; deepen later.

## Log

- Refactored to lot-relative placement: `Lot` struct + per-Sim/Object `lot`;
  `s_lots[4]`, `s_active_lot`, `s_overview`. Render: `draw_ground` (lawn + roads)
  + `draw_lot` per lot (grid only on active, active ring, dim inactive).
- Camera: lot view (focus active lot) vs overview (top-down neighborhood).
- DevAPI: game.state adds active_lot/lot_count/overview + per-sim lot;
  game.action.travel {lot}; select follows the Sim's household.
- Verified: `python tmp/ll_city.py` (4 lots, 12 sims 3/lot, travel + overview).
  Evidence: `gamedesign/projects/little-lives/reviews/city_overview.png`.
