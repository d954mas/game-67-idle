# Evidence — A0 (art contract) + A1 (surface lighting / sky / fog)

## Sprint contract
Deliver the frozen art-direction contract `src/ll_art.h` and its first consumer:
rework the world renderer so the scene reads as ONE coherent lit place — baked
directional sun shading on surfaces, a banded gradient sky, distance fog, and a
warm/cool temperature grade — instead of flat daylight-scaled colours.

## Named acceptance checks
- [x] `ll_art.h` compiles and is the only source of palette/lighting tokens used
      by the new render paths (sun dir, ambient, sky bands, fog, grade).
- [x] Build of `game_seed` is green (clang, native-debug).
- [x] Gradient sky renders (zenith→horizon bands) replacing the flat blue clear.
- [x] The two cutaway walls differ in shade by light direction (one sun-warm,
      one cool-shadow) — directional lighting visible, not flat.
- [x] Distant lots/edges haze toward the horizon (fog) — depth, not floating.
- [x] Gameplay unchanged: DevAPI smoke passes decay/eat/work/build checks.
- [x] `node tools/ai.mjs validate` green (pipeline + visual invariant guard:
      no banned debug text in product view).
- [x] `node tools/taskboard/cli.mjs validate` green.

## Evidence
- Before: `tasks/evidence/night-baseline/00-baseline-live.png` (flat blue sky,
  evenly-lit flat walls, no fog).
- After (noon): `tasks/evidence/night-A1/02-A1-noon.png` (gradient sky, cool
  left wall / warm back wall, warm-graded floor + sofa, edge haze).
- After (morning): `tasks/evidence/night-A1/01-A1-surfaces-sky-fog.png`.

## Verdict
PASS — coherent lit room vs flat cutouts. Faceted furniture + AO contact shadows
(to ground objects/sims) deferred to A2.
