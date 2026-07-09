---
type: Build Plan
title: M0 Dress Room freeplay
game_id: web-dressup
status: done
date: 2026-07-09
---

# M0 Build Plan — Dress Room freeplay

## Goal

Ship a **classic 2D freeplay dress-up** on Poki-ready dual layout. No Fake Show,
no timer, no map, no multiplayer.

## DoD

1. Boot → Dress Room (no text wall).
2. Category chips + catalog equip/clear.
3. Stage shows layered placeholder mannequin (tinted panels until real art).
4. Random + Reset work.
5. Portrait and landscape both usable.
6. Template demo cubes / gold-xp HUD not the player fantasy.
7. Settings gear still available.
8. Native build compiles; unit test covers outfit logic.

## Files

| Path | Role |
|------|------|
| `src/features/dress_room/dress_room.h` | Public API |
| `src/features/dress_room/dress_room.c` | Catalog, outfit, UI |
| `src/features/game_features.c` | Wire draw_ui |
| `src/main.c` | Soft clear color; skip demo mesh fantasy |
| `CMakeLists.txt` | Add sources + test |
| `tests/test_dress_room.c` | Equip/random/reset |

## Later (not M0)

M1 theme banner, M2 Fake Show, real layered PNGs, recolor, poses.
