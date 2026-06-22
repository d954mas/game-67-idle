# Evidence — A2 (faceted furniture + AO contact shadows)

## Sprint contract
Make furniture read as crisp low-poly solids and ground every prop/sim: shade
each furniture triangle by its own world face-normal against the baked sun
(faceting), and add round contact shadows under objects and sims.

## Named acceptance checks
- [x] Furniture shaded per-face: each triangle uses its face normal → visible
      facets (sofa top vs sides differ in light), not one flat colour per part.
- [x] Round contact shadows ground objects + sims (sofa/desk/bed/sims sit on a
      soft warm-dark shadow, no longer floating).
- [x] Shadows are opaque-but-soft (renderer has no alpha blend — confirmed in
      engine pipelines) via 3 concentric tone rings, dimmed by daylight.
- [x] Build green; per-face triangle batching auto-flushes (no overflow).
- [x] Gameplay unchanged (smoke decay/eat/work/build green).
- [x] `node tools/ai.mjs validate` green (fail 0, visual invariants clean).

## Evidence
- Before A2: `tasks/evidence/night-A1/02-A1-noon.png` (flat-shaded furniture,
  objects floating).
- After A2: `tasks/evidence/night-A2/04-A2-ringshadows.png` (faceted furniture +
  grounded soft contact shadows).
- Intermediate (hard-blob shadows, rejected): `03-A2-faceted-ao.png`.

## Verdict
PASS — furniture is dimensional and everything is grounded. Note: discovered the
shape renderer never alpha-blends; all transparency is faked with opaque tones.
