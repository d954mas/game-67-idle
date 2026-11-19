# Blender Production Lessons

What six games learned building 3D assets with agents in Blender: what cost the most
iterations, why, and what helped. Each lesson ends with a tip and the shared tool that makes
the tip cheap. Tips are advice from experience, not rules; pick what helps the asset at hand.

Evidence is given as `<game folder>/<path>` (game repositories under `games/` and
`games/private/`) or as the commit in that game's repository. A private game whose id must
not appear in Studio files is named by a short description instead.

## Problem → tool

| You want to… | Tool |
|---|---|
| run a script in headless Blender without version or path trouble | `run_blender.py` |
| see an asset the way the lead will judge it (game camera, ¾, turntable, clay, wire) | `render_views.py` |
| get renders, every applicable check and one card in a single call | `review.py` |
| find holes, flipped or unwelded faces, open shells | `mesh_check.py` |
| check pivot, bounds, normals, textures, skin weights, rig against a reference | `glb_check.py` |
| prove parts touch (door in frame, item in hand, prop on floor) | `contact_check.py` |
| find foot slide, soles under the floor, pops, teleports in clips | `clip_check.py` |
| put many materials in one palette texture without silent overflow | `lib/color.py` `PaletteAtlas` |
| read, pose or write GLB without Blender | `lib/glb.py` |
| scene reset, palette materials, primitives, GLB import/export with pivot | `lib/bpy_scene.py`, `lib/bpy_io.py` |

## Look and style — the most expensive misses

### 1. Technical checks passed, the lead's eye did not
- **What happened:** A scripted facade passed topology, bounding-box and a self-written PASS
  certificate with floating doors, sparse rails and a missing window arch; review renders
  filled 1.4–5.8% of the frame, so nobody could see it. About 35k lines in 219 files were torn
  down. A hotel facade later passed independent visual, material and technical review plus the
  studio gate and was rejected on sight. A generated supporting cast passed critics and was
  rejected as off-style.
- **Why:** The checks proved "renders and is manifold", not "reads right in the game". Reviews
  compared against a written lock or against the batch itself, not against the game.
- **Tip:** Before building, show cheap fake shots from the game camera (blockout render or
  paint-over). Review the asset framed tight, next to two shipped neighbours, at the game
  camera, with the in-game crop on the card. Let someone other than the builder give the verdict.
- **Tools:** `render_views.py` (`game` view), `review.py --game-crop`, `contact_check.py`.
- **Evidence:** tram game (private): design/knowledge/city_square_blender_model_qa_workflow_2026-08-06.md;
  tram game (private): design/knowledge/hotel_runtime_acceptance_2026-10-29.md;
  love-on-4090/design/STATUS.md.

### 2. Detail and sources drifted from the shipped kit
- **What happened:** Detailed diorama props and a paving pattern were rejected as distracting.
  40 CC0 props were rejected as faceted and off-theme; a mix of three city packs was "not in
  style"; a generated relic set was "dirty" and also the largest download (3.7 MB brotli).
  Generated stand-ins for recognisable objects read as glitches.
- **Why:** Detail level and source were chosen per asset, one at a time, not next to the kit
  in the same light.
- **Tip:** Render candidates beside shipped assets at the game camera; prefer one pack family
  per kit; note which style traits a candidate breaks before importing it.
- **Tools:** `render_views.py`, `review.py`.
- **Evidence:** digging game (private) design/dragon/artifacts_v7/; brawler game (private) tools/city_kit/;
  runner game (private) design/knowledge/evidence/.

### 3. Concepts used marketing art as the style reference
- **What happened:** A concept round styled from glossy capsule art was "very far from the
  game". Concepts generated from a render of the real in-game GLB stayed in style.
- **Tip:** Start concept and fake-shot work from a render of the real game asset with game
  shading and palette.
- **Tools:** `render_views.py` (beauty, game view).
- **Evidence:** tram game (private): design/references/steam_capsule/concepts/composite_blender_gen_v005.

### 4. Each round fixed one point and broke its neighbours
- **What happened:** Character gauntlet rounds scaled the head, smoothed, added a decal face,
  borrowed hair; each round looked worse ("the face is scary and huge, the body broken").
- **Why:** Critics compared the model with the icon only, never with the previous round.
- **Tip:** Every round shows the previous round and the last approved look side by side.
  When a base does not fit, swap or recolour a clean base rather than sculpting locally.
- **Tools:** `review.py` (one card per round), `mesh_check.py`.
- **Evidence:** digging game (private) design/dragon/worker_k4/; digging game (private) design/dragon/WORKER.md.

## Characters

### 5. Start from a rigged, professionally made base
- **What happened:** A hero built from scratch by script went through body v71, clothing v86,
  wardrobe v96 and still had stepped torso and detached shoulders. A first-person hand went
  through seven revisions and four watertight passes before a licensed rigged source replaced
  it. A worker built on an existing rigged body shipped with 55 poses and zero holes.
- **Why:** Anatomy, deformation and style were invented together; every fix moved a joint
  another part depended on.
- **Tip:** Search owned, purchased and CC0 rigged bases first, especially for hands, faces and
  animals. Spend iterations on silhouette and palette; use Blender to adapt, not to invent.
- **Tools:** `glb_check.py` (skin, reference rig), `review.py`.
- **Evidence:** voxel-hero game (private) design/art/modular-hero/; tram game (private): tools/blender_player_hand_g0/;
  digging game (private) commit f082794.

### 6. A generated mesh is a measurement; a good generated shape can be polished
- **What happened:** A single multi-view generation (one mesh, one JPEG atlas, painted-on
  clothes) was rejected whole: the face smeared, nothing could be swapped. An auto-decimated,
  reposed NPC passed a critic and was rejected for crooked limbs. A boss rebuilt only from a
  generated mesh's measurements lost its character; the same generated shape polished in
  Blender was approved.
- **Why:** Shard UVs and baked light smear features; decimation keeps silhouette but breaks
  deformation topology.
- **Tip:** Use generated meshes for proportions, or polish a strong generated shape for
  secondary characters; build hero parts (head, hair, body, garments) as clean separate shells.
  Ask critics to check limbs and mesh artifacts at the game camera and in motion first.
- **Tools:** `lib/glb.py` (measure), `mesh_check.py`, `clip_check.py`, `review.py`.
- **Evidence:** love-on-4090/design/knowledge/character-modeling-guide.md; love-on-4090/design/knowledge/cast-v2-spec.md.

### 7. Clothing as pushed-out body islands opened holes
- **What happened:** Six unwelded islands with 167 coincident pairs opened gaps up to 0.26
  units when pushed; concave areas folded (520 of 2562 head faces inverted); a single-sided
  hood showed the scene through it; decimation at 0.4 cut layers through each other.
- **Why:** Normals were recomputed per island and the push exceeded the local concave radius.
- **Tip:** Weld before offsetting, offset along averaged normals, give shells an inner wall,
  decimate before layering. What worked elsewhere: derive garments from the body's rings
  (6–10 mm offset) and transfer weights from the body by nearest face.
- **Tools:** `mesh_check.py` (welded topology, miswound, inward faces), `contact_check.py`.
- **Evidence:** brawler game (private) design/knowledge/evidence/thug-costume-defects-2026-09-25/README.md;
  love-on-4090/design/knowledge/character-outfit-pipeline-research.md; love-on-4090/tools/lin3/garment_lib.py.

### 8. One rig standard, one skinned rig instead of pose meshes
- **What worked:** Characters from different generators were reposed onto one fixed rig by
  landmarks, skinned with geodesic weights, and checked against the standard on every export.
  A creature built as per-beat pose meshes "read as different cats"; one 39-bone rig with
  seven clips and crossfades fixed it.
- **Tip:** Keep the rig standard as data and compare every export with it.
- **Tools:** `glb_check.py --reference-rig`, `clip_check.py`.
- **Evidence:** love-on-4090/tools/npc_v2/; love-on-4090/tools/lin3/check_rig.py;
  tram game (private) commit 656cfafc.

### 9. Weights cannot fix a deep crease under linear-blend skinning
- **What happened:** A tight garment pierced the body at about 98° hip flexion; four rounds of
  weight blends, de-roll and helper bones failed.
- **Why:** Under LBS the belly moves with the hips and the thigh top with the leg; no weight
  mix lifts one over the other, and garments copying body weights inherit it.
- **Tip:** Measure joint angles per clip first; fix the one extreme shot's pose, or use
  corrective shapes, instead of tuning weights rig-wide.
- **Tools:** `clip_check.py`.
- **Evidence:** love-on-4090/design/knowledge/hip-crease-findings-2026-11-18.md.

## Animation

### 10. Retargeting is measurable — measure it
- **What happened:** Retargeted clips had 9 glove triangle crossings, soles 22 mm under the
  floor on Run, a grip axis parallel to the forearm, and root-motion clips that lost travel.
  Three games wrote three separate retargeters.
- **Tip:** Check floor extremes, planted-foot slide, pops and hand-to-item contact on every
  retargeted clip; keep skeleton maps as data.
- **Tools:** `clip_check.py`, `contact_check.py`, `lib/motion.py`.
- **Evidence:** voxel-hero game (private) design/art/modular-hero/ready-animation-handoff.md;
  brawler game (private) tools/visual_slice/retarget_fight_clips.py; voxel-hero game (private) tools/actors/retarget_ready_hero.py.

### 11. Numbers ended opinion rounds; eyeballed takes reached the lead broken
- **What worked:** A foot-slip metric and a stride per leg length set playback rate; a glTF
  decode-back matched Blender within 8.5e-7 m. No opinion rounds were needed.
- **What failed:** Seated trailer takes reached the lead with floating pelvises and legs
  through chairs; the review was a glance at frames.
- **Tip:** Run contact and clip checks plus an independent critic before anything is shown.
- **Tools:** `clip_check.py`, `contact_check.py`, `review.py`.
- **Evidence:** brawler game (private) design/knowledge/fight-anims-2026-09-23.md;
  voxel-hero game (private) design/art/modular-hero/animated-export-diagnostic.md; love-on-4090/tools/qa/take_qa.py.

## Geometry and export

### 12. Pivot, bounds centre and collider centre got mixed up
- **What happened:** Carts and boxes hovered 0.5–0.8 m above the floor because a level
  generator read the bounds/collider centre as the model origin. A held item skinned to the
  hand sat 0.23 units off it.
- **Tip:** Report pivot, bounds bottom and bounds centre separately on every export; check
  attachment pairs (item to hand, prop to floor) in the same pass as weights.
- **Tools:** `glb_check.py` (pivot rule, bounds centre), `contact_check.py`, `lib/bpy_io.py`.
- **Evidence:** love-on-4090/design/knowledge/floating-props-audit.md;
  tram game (private) commit 62eea9c0.

### 13. Palette atlases: great default, silent overflow
- **What worked:** One shared palette replaced five baked textures and a weapons atlas; a pack
  went from 3.67 MB to 0.56 MB.
- **What failed:** The 33rd row of a 32-row palette wrapped onto row 0; texels with alpha ≤ 8
  were refilled by the engine as holes.
- **Tip:** Allocate rows through an allocator that raises when full and keeps alpha above 8.
- **Tools:** `lib/color.py` `PaletteAtlas`.
- **Evidence:** digging game (private) commits e1e5bec, 9beaaf3, b8680a3.

### 14. Colour problems that looked like art problems
- **What happened:** A runtime frame looked grey; the Blender palette was fine. About 60% of
  the colour was lost to gamma-space shading and ACES on a clamped buffer.
- **Tip:** Compare palette values in Blender with pixels in the runtime before repainting.
  Keep sRGB/linear conversion in one place.
- **Tools:** `lib/color.py`, `review.py --game-crop`.
- **Evidence:** tram game (private) commit 41bb27a9.

### 15. Exports were not deterministic; LODs need measurement first
- **What happened:** GLB bytes changed on every run because triangle order varied; fixed by
  canonicalising order. Far crowds spent 72% of a 17.8 ms frame on sub-pixel triangles; a
  far LOD under 140 triangles and ~300-triangle accessory LODs met 60 fps.
- **Tip:** Canonicalise and hash exports so a diff means a real change; profile per pass,
  then give LODs explicit triangle budgets.
- **Tools:** `mesh_check.py --tri-budget`, `review.py` (counts on the card).
- **Evidence:** tram game (private) commits c276d6f6, 41961f7d, 03e01af4.

## Tooling

### 16. Every game rewrote the same plumbing
- **What happened:** Six games hold about 900 `bpy` scripts: ~320 own render setups, ~115 GLB
  parsers, ~90 contact-sheet builders, camera setup in 68 files of one game alone, four
  retargeters. Twenty-plus scripts were byte-copied between two games and then drifted.
- **Tip:** Import the shared library; keep only shape recipes in the game; bump versions with
  parameters, not file copies.
- **Tools:** `lib/`, `render_views.py`, `review_card.py`.
- **Evidence:** tram game (private): tools/blender_city_square/; digging game (private) tools/visual_slice/;
  brawler game (private) tools/visual_slice/.

### 17. Blender version and launch friction
- **What happened:** Receipts cite 4.3.2, a 4.5 portable and 5.2.3; one install had no
  executable; the sandbox blocks `.exe`; a shim rewrote `BLENDER_EEVEE_NEXT` in script text
  and `exec`ed it; one kit needed OpenVDB bindings at import time.
- **Tip:** Launch through one runner that resolves and records the version, and keep 4.x/5.x
  differences in one module. Import heavy optional dependencies lazily.
- **Tools:** `run_blender.py`, `lib/bpy_scene.py`.
- **Evidence:** tram game (private): tools/blender_player_hand_g0/run_blender52.py;
  voxel-hero game (private) tools/actors/build_actors.py; digging game (private) design/dragon/WORKER.md.

### 18. Fast optional tools get used; a heavy gate gets bypassed
- **What happened:** The six-stage fail-closed gate was used by four games until late October;
  the two newest Blender-heavy games never called it and wrote their own fast checks. The
  vertex-colour bake, with a one-line CLI and an add-on, stayed in steady use.
- **Tip:** Reach for the one-command `review.py`; keep the full gate (below in the README) for
  hero or named-reference scenes where fresh-hash evidence matters.
- **Tools:** `review.py`, `gate.py`.

### 19. What worked for speed
- One script per prop (250–350 lines, flat-matte, low-poly) on shared helpers shipped fast.
- A one-page target sheet (facet scale, normals, colour source, acceptance) before modelling
  a hero, then parameterised build scripts that re-run in seconds.
- One review card per asset: render, wireframe with triangle count, textures, in-game crop.
- **Evidence:** tram game (private): tools/blender_district_barrier/;
  love-on-4090/design/knowledge/character-modeling-guide.md; runner game (private) design/knowledge/evidence/gem-model-v002/README.md.

## Full-gate incidents

The full gate in the README (Stages 0–6) grew out of these architecture-scene
incidents. They stay as regression examples for that gate.

### Incident Regression — City Square M02 v002

This pipeline exists because M02 v002 passed a render/count/modifier-order
check while deeper inspection found 208 degenerate faces after bevel, 76 open
boundary edges across volumetric arch pieces, six dirty modifier stages, 13
bevel policies, and a dormer visually overlaid on the roof.

`blender_scene_audit.py` reproduces that failure and returns `block`. Keep this
scene report as a regression example; never weaken the gates merely to make the
old artifact pass.

### Incident Regression — City Square M02 v004 r15

M02 v004 r15 had assigned material families and Noise/Bump nodes, but the Pale
Ochre facade still read as a uniform fill in the master frame. A roof-focused
review packet omitted a fitted facade material view and a dedicated material
review lane, so `QART_001` and `QASSET_002` were incorrectly reported as pass.

Keep the M02 v004 gate regression: it must require explicit material-surface
and material-scale art checks, fitted facade and colored-oblique renders, a
distinct material reviewer with named claims, and evidence-bearing quality
records. Technical shader construction cannot substitute for visual proof.

### Incident Regression — City Square M02 v004 r16

M02 v004 r16 passed topology and three independent report lanes, but the lead
still found two visible failures: the side roof read as disconnected planes and
the plaster/stone response disappeared in the approved master frame. The full-
depth roof had preserved a shallow front-only ridge position, leaving one rear
plane visually dominant from three-quarter views. The material reviewer judged
close-up construction without separately proving master, 640-pixel and
front-versus-side consistency.

Keep this regression fail-closed. A completed architectural roof needs dedicated
left/right colored side evidence and explicit reviewer claims for continuous
side planes plus intact eave/ridge read. Finished facade materials need fitted,
oblique, master-crop and 640-crop evidence, with explicit claims for target-
camera visibility and front/side consistency. A close-up PASS cannot override a
flat target frame, and a watertight roof cannot override a broken silhouette.
