# Project Status

## Current Goal

Little Lives (little-lives): a 3D Sims-like. The overnight run polished the
verified M1–M3 game from a flat debug-art monolith into a coherent, stylized
low-poly "miniature diorama" — one authored art direction, depth, juice.

## Night Run — branch `night/little-lives-polish` (2026-06-23)

Plan: `gamedesign/projects/little-lives/night_plan.md`
Backlog + decisions + telemetry: `gamedesign/projects/little-lives/night_backlog.md`
master is safe at tag `little-lives-m3-snapshot-2026-06-23` (never touched).

Shipped — one commit + pre-snapshot tag (`night/<id>-pre`) per slice:
- A0 `src/ll_art.h` — frozen art contract (sun/ambient/AO/fog/sky/grade/camera).
- A1 lit surfaces: directional walls, banded gradient sky, distance fog, grade.
- A2 faceted furniture (per-face sun) + soft AO contact shadows.
- A3 cohesive HUD palette + consistent buttons (engine font only).
- A4 juice: `src/ll_fx.{c,h}` (subagent-built, lead-integrated) — particles,
  screen shake, 0.5s squash-stretch; wired to place/need/work/promotion events.
- A5 camera life: authored diorama framing + gentle breathing drift.
- A7 composition: back-wall window (tracks time of day) + wall poster; grade
  verified consistent noon vs evening.

A6 (gameplay depth) folded into A7 — core systems (careers/skills/relationships)
already built + verified in M1–M3; the night's goal was the art/polish/juice
transformation, not new mechanics. Parked: none.

Evidence (before/after + per-slice contracts): `tasks/evidence/night-baseline/`
and `tasks/evidence/night-A1..A7/`. Headline: `night-baseline/00-baseline-live.png`
vs `night-A7/16-A7-noon.png`.

## Build / Run / Validate

```
cmake --preset native-debug
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 1280x720
python tmp/ll_smoke.py                                   # gameplay acceptance
python tools/little-lives/ll_capture.py <out> [wait] [mode] [minutes]
node tools/ai.mjs validate --full ; node tools/taskboard/cli.mjs validate
```

## Gates (this run, all green)

build green; smoke decay/eat/work/build green; `ai.mjs validate --full` +
visual invariant guard green; taskboard validate green; skills_eval green.
