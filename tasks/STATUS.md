# Project Status

## Night Run (branch night/little-lives-polish, 2026-06-23)

Autonomous overnight polish: debug-art monolith → polished stylized low-poly.
Plan: `gamedesign/projects/little-lives/night_plan.md`. Backlog + decisions:
`gamedesign/projects/little-lives/night_backlog.md`. master is safe at
`little-lives-m3-snapshot-2026-06-23` (never touched).

Shipped this run:
- **A0** `src/ll_art.h` — frozen art-direction contract (sun dir, ambient, AO,
  fog, sky bands, warm/cool grade). One coherent lighting source for all draws.
- **A1** render rework #1 — directional surface shading (walls now sun-warm vs
  cool-shadow), banded gradient sky, distance fog, warm/cool grade. Gameplay
  unchanged (smoke green). Evidence: `tasks/evidence/night-A1/`.

In progress: A2 (faceted furniture + AO contact shadows) → A3 HUD → A4 juice →
A5 camera → A6 depth → A7 compose → A8 harden → A9 handoff.

## Current Goal

Build `Little Lives` (little-lives): a 3D Sims-like, iteratively toward a full
game (city, multiple Sims, interactions, build, work, needs). Milestone plan:
M1 house sandbox (now) → M2 city/lots → M3 careers/relationships/skills.

## Current State

- Playable 3D Sims-like, BUILT + DevAPI-verified. Runtime: `src/clean_seed_main.c`.
  - M1 sandbox (T0106 ✅), polish (✅).
  - M2 city: 4-lot neighborhood, roads, travel, overview map (T0109 ✅ review).
  - M3 careers/relationships/skills/families (T0110 ✅ review).
  - HUD text via engine font pack (T0107 ✅ done).
  - Furniture = real CC0 Kenney meshes with authentic per-material colours
    (bed/fridge/shower/toilet/sofa/desk) via the shape-renderer mesh path (T0111).
    Sims = animated blocky humanoids (real character mesh needs a rigged pipeline).

## Blocking Work

- None. Core game proven across M1–M3 + readable text HUD.

## Non-blocking Debt

- Sim character mesh = a rigged/textured/animated pipeline (FBX->glTF + skin +
  ozz skeletal anim); large milestone, only if photoreal Sims wanted (T0111).
- City: only the active lot fully simulated for player; cross-lot visiting is minimal.

## Build / Run / Validate

```powershell
cmake --build build/_cmake/native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 1280x720
python tmp/ll_smoke.py            # DevAPI acceptance smoke
node tools/taskboard/cli.mjs validate
node tools/ai.mjs validate        # pipeline + visual-invariant guard (run this!)
```

## Last Known Good Evidence

- `gamedesign/projects/little-lives/reviews/city_meshes_overview.png` — neighborhood w/ mesh furniture.
- `gamedesign/projects/little-lives/reviews/furniture_meshes.png` — multi-colour CC0 furniture.
- `gamedesign/projects/little-lives/reviews/hud_text.png` — readable font HUD.
- `gamedesign/projects/little-lives/reviews/sims_people.png` — humanoid Sims.

## Next Priorities

1. T0111 — Sim character mesh + optional textured/lit (PBR) mesh pipeline.
2. Lead review of M2/M3 (T0109/T0110) → close or extend.
3. Deeper city: cross-lot visiting, neighborhood sim life.
