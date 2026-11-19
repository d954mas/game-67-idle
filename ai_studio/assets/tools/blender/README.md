# Blender Tools

Shared Blender and GLB tooling for every game: a headless runner, a `bpy`/GLB
library, review renders and cards, and advisory checks. The tools exist to make
iteration faster; use what helps and write game code only for the shapes
themselves. What six games learned, and which tool answers each lesson:
[production_lessons.md](production_lessons.md).

The repository skill surface is `.codex/skills/nt-blender-production/SKILL.md`.
Commands below run from the Studio root with the Studio venv
(`node ai_studio/dev_environment/python_run.mjs <script> ...`; plain `python`
works too when numpy and Pillow are installed). Anything that starts Blender must
run outside the agent sandbox, which blocks `.exe` launches.

## Quick Path

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/assets/tools/blender/review.py `
  <game>/assets/models/crate.glb <game>/assets/models/lamp.glb --out tmp/review
```

From a game repo the same command works with paths relative to that repo (folders and
`*.glb` globs expand). In the agent sandbox, which blocks Blender, add `--no-render`.

One call renders front, right and ¾ views (beauty, clay, wire), runs every
applicable check and writes `tmp/review/<stem>/card.png` per model: views,
triangle/vertex/material/texture counts, size, and the check findings; the terminal
prints each finding with a fix hint. All
models render in one Blender start (about 4 s for the first, about 1–2 s for each
next one); checks take milliseconds.

Useful flags: `--game-camera '{"height":1.8,"behind":2,"pitch":16,"fov_y":55}'`
adds the game's view; `--game-crop shot.png` puts the in-game screenshot on the
card; `--turntable 8`; `--animation Walk --time 0.4` poses the model;
`--pivot floor`, `--contacts contract.json`, `--floating`; `--no-render` for
checks only; `--strict` to exit 1 on high findings.

## Tools

| Tool | What it does |
|---|---|
| `run_blender.py <script.py> -- <args>` | Finds Blender (`--blender`, `$BLENDER`, PATH, installed versions with 5.2 first), runs the script headless with `--factory-startup`, the Studio root on `sys.path`, exit code on Python errors, timeout and optional `--log`. `--which` prints the executable. |
| `render_views.py` (inside Blender) | Views `front back left right top three_quarter game turntable` × passes `beauty clay wire silhouette`, fitted framing, shared studio HDR or flat light, `views.json` with model stats; several `--model` files in one start. |
| `review_card.py` | Composes the card from `views.json` and check reports (Pillow, no Blender). |
| `review.py` | Checks + one batched render + cards, as above. |
| `mesh_check.py` | Welded topology: open edges, non-manifold, miswound and degenerate faces, duplicates, components, inside-out shells, optional `--tri-budget`. Profiles `auto`, `volume`, `surface`. |
| `glb_check.py` | Export contract: finite data, unit normals, triangles, URIs, JPEG, texture policy, COLOR_0 encoding, pivot rule with bounds and bounds centre, joint limit, skin weights (≤ 4 influences, sums, unweighted vertices, inverse binds), `--reference-rig`. |
| `contact_check.py` | Contract pairs (`moving`, `receiver`, `direction`, `window`) with an automatic negative control per pair, ground supports, or `--floating` for parts hanging in the air. |
| `clip_check.py` | Per clip: non-finite or non-positive keys, pops, teleports, planted-foot slide, soles under the floor, optional loop seam; limits scale with model height. `--model` plays clip-only files on a model. |
| `lib/glb.py` | Read, pose, skin and write GLB with numpy, no Blender: `Glb.load`, `parts`, `posed_world`, `skinned_positions`, `GlbBuilder`. |
| `lib/color.py` | sRGB/linear/hex conversion; `PaletteAtlas` rows that raise when full and keep alpha above the engine refill threshold; PNG writer for Blender. |
| `lib/bpy_scene.py`, `lib/bpy_io.py` | Inside Blender: scene reset, palette and vertex-colour materials, closed bmesh primitives, axis helpers, bounds, 4.x/5.x compatibility; GLB import and export presets (`static`, `skinned`, `animated`) with pivot rules and an export report. |
| `lib/motion.py` | Foot contacts, foot lock, slide, twist removal, two-bone IK (shared with Kimodo). |
| `lib/studio_root.py` | Finds the Studio checkout from any game repo or feature workspace (`NT_STUDIO_ROOT` overrides). |

Checks print findings with a fix hint and exit 0; `--strict --fail-on
high|medium|low` turns them into a gate. Every threshold is a flag or a
`--config` JSON key (flat, or nested under the tool name).

A game script that runs inside Blender imports the library directly:

```python
from ai_studio.assets.tools.blender.lib import bpy_io, bpy_scene as S

S.reset_scene()
crate = S.box("Crate", size=(1, 1, 1), anchor="bottom", material=S.palette_material("Wood", "#8a5a33"))
print(bpy_io.export_glb("tmp/crate.glb", [crate], preset="static"))
```

## Full Gate

The stages below are a fail-closed review loop for hero scenes, named-reference
work, propagation and handoff, where fresh-hash evidence and independent review
lanes matter. Use it when that level of proof is wanted; it is not a step every
asset has to pass.

## Outcome

A Blender iteration is review-ready only when four claims are independently
supported:

1. **Technical integrity:** evaluated meshes and modifier stages are clean.
2. **Art-direction fidelity:** the result still matches the approved references,
   style rules, composition, architecture, palette, and detail hierarchy.
3. **Completion:** every requested deliverable exists and no known high-severity
   defect remains open.
4. **Evidence freshness:** audits and renders identify the exact scene hash they
   reviewed.

`The render completed` proves none of those claims by itself.

## Feedback Loop

```text
reference lock
    ↓
thin Blender change
    ↓
technical audit ── block ──→ fix the first failing stage ──┐
    ↓ review/pass                                           │
clay + color + ¾ + master-frame evidence                    │
    ↓                                                       │
art-direction mismatch audit ── block ──→ revise brief ─────┤
    ↓ pass                                                  │
independent visual/technical review ── block ──→ fix ───────┘
    ↓ pass
fresh-hash final gate
    ↓
user review / propagation / handoff
```

During construction, run the technical and visual checks affected by each
material geometry change. Fix the failing modifier or design layer before
adding detail; change the construction when repeated corrections miss the cause.
Final review, propagation and handoff still require the complete applicable
profile and fresh evidence for the exact saved scene.

## Stage 0 — Reference Lock

Before scene production, establish or reuse a valid reference lock. Update the
affected fields when scope or accepted design changes:

- source `.blend` and immutable hash;
- separate output scene path and version;
- approved spec and reference paths;
- locked camera, resolution, color-management, scale and units;
- requested scope and explicit non-goals;
- palette/material invariants;
- architecture or form genealogy when a named style drives the result;
- required proof renders;
- what the next user decision actually is;
- measurable done condition.

If a named reference controls art direction, use the design-knowledge workflow
and create the mismatch audit before final modeling. Do not reduce a style to a
feature checklist such as `pastel + symmetry + arch`.

## Stage 1 — Thin Build

Work in a versioned copy. Change the smallest coherent slice that can prove the
decision. Keep source scenes immutable. Give review-owned objects stable names
or a common prefix/collection.

For generated geometry, set `audit_topology` when automatic classification is
wrong:

- `volume`: must be watertight;
- `surface`: intentional open sheet/plane; boundary edges are allowed;
- `skip`: helper that must not enter the final render or handoff.

Do not use `skip` to silence a production defect.

## Stage 2 — Technical Integrity Gate

Run Blender headlessly against the saved scene:

```powershell
& $blender --background $scene `
  --python ai_studio/assets/tools/blender/blender_scene_audit.py -- `
  --output tmp/blender-audit.json `
  --prefix Hero_ `
  --camera-name CAM_Master `
  --focal-mm 160 `
  --shift-y 0.025
```

The audit evaluates final meshes and every modifier stage. It blocks:

- zero-area/degenerate faces;
- unintended boundary edges on volumes;
- non-manifold or duplicate faces;
- a dirty intermediate Boolean/modifier stage hidden by later modifiers;
- camera/unit mismatch;
- uncontrolled bevel-policy proliferation.

It leaves generic intersections at `review` because intentional assembly
overlaps cannot be approved safely from AABBs alone. Resolve that check with a
three-quarter depth/wireframe/internal proof and a written list of allowed
overlaps. Never turn a broad overlap detector into an automatic pass.

### Component interfaces and architectural joinery

Topology alone cannot prove that separately valid meshes form a credible
building. Every component-first architectural packet must also declare and
verify its interfaces:

- use separate `contact` and `clearance` contracts; a positive gap is not a
  universal success condition;
- tag legal cross-component mates explicitly and keep undeclared intersections
  blocking;
- verify structural/load-path chains such as threshold -> jamb -> impost ->
  archivolt and console -> slab -> post -> rail;
- sample the final visible opening and backing, not an earlier proxy hidden
  behind the component under review;
- bound repeated-element pitch and require intentional end conditions such as
  rail returns or lower rails;
- test profile seating around curved openings with distance/ray/BVH samples;
- treat coplanar/tangent faces as suspect until an evaluated-mesh check and a
  depth or section view resolve them.

Blender's mesh validation, BMesh adjacency, ray casting and BVH overlap checks
support these rules, but cannot decide whether a door reads as a door or a
balcony is architecturally plausible. Those remain evidence-backed visual
review claims.

For Boolean architecture:

- prefer one watertight profile cutter over tangent rectangle-plus-ellipse
  pairs;
- make cutters pass fully through the shell;
- audit after each Boolean and after bevel;
- reject coplanar/tangent joins that create degenerate faces;
- close arch-ring caps and all volumetric decorative meshes;
- construct dormers/openings through roofs rather than hiding collisions from
  one camera.

## Stage 3 — Visual Evidence Packet

Use the real target camera as well as diagnostic views. A lookdev packet must
contain:

- frontal clay;
- frontal color;
- colored three-quarter whenever glass, transparency, clearcoat, anisotropy,
  layered shading, or another angle-dependent material is under review;
- three-quarter depth/intersection check;
- complete approved master frame;
- grayscale value check;
- solid-black silhouette;
- lighting comparison on the complete scene when lighting is under review.

Add wireframe/internal views for handoff or whenever modifiers, roofs, nested
openings, or intersections are risky. Include a scale figure for architecture.

Component evidence must be fitted to evaluated component bounds rather than
reusing a whole-assembly camera. Require, at minimum, component `clay_front`,
`color_front`, `depth_three_quarter`, `wireframe`, and `silhouette` views. Check
that the silhouette has useful occupancy and nonzero frame margins. A tiny
component somewhere inside a valid image is not review evidence.

Clay does not validate materials. If a defect appears only under an oblique
view, the exact colored oblique role must also appear in the contact sheet and
blind comparison. Review transparent/transmissive surfaces for opaque-wedge,
world/black-void, sorting-split and detached-edge artifacts; a plausible shader
parameter is not evidence that the rendered result is plausible.

### Material-response evidence

Shader nodes, material slots, swatches and parameter ranges prove construction,
not visible material quality. When a lookdev task claims finished plaster,
stone, brick, wood, metal, glass or roofing, require all of:

- a fitted neutral-light surface view where macro color, roughness and relief
  response can actually be seen;
- a colored oblique view for angle-dependent response and edge separation;
- the approved master frame and reduced gameplay-size frame to reject both
  accidental flatness and high-frequency noise;
- a dedicated material reviewer who explicitly judges surface visibility,
  family separation and texture scale against the reference contract.

A material lane cannot pass because named shader nodes exist. If the claimed
surface reads as a uniform color at the target camera and flat output was not
explicitly approved, keep art and material readiness blocked.

Do not infer a hole from one isolated view. A background-colored edge can be an
object silhouette, while a dark patch can be a recess, missing backing, or
lighting. Resolve the claim with at least two of: assembly color/ID context,
three-quarter depth, section/ray evidence, or explicit backing-hit results.

Hash the `.blend` after saving. Every render manifest and audit report must
carry that same hash. Any later scene save makes the prior evidence stale.

## Stage 4 — Art-Direction Fidelity Gate

Actually open the current images and the approved references at useful
resolution. Write observed mismatches before judging preference.

Check every applicable dimension:

1. composition and locked camera;
2. silhouette and mass hierarchy;
3. architecture/form genealogy and structural plausibility;
4. scale and proportions;
5. repeated rhythm versus hero exceptions;
6. material and palette relationships;
7. light on the full composition;
8. detail hierarchy at target and thumbnail size;
9. originality/copy risk;
10. completion against the user's exact deliverables.

For each dimension record:

- evidence paths;
- what is good;
- what is wrong or missing;
- `observed` versus `inferred` claims;
- severity: `critical`, `high`, `medium`, or `low`;
- remediation and proof required to close it.

An approved palette does not approve architecture. A clean topology report does
not approve style. A close-up does not approve the master frame. Do not average
these gates into one vague score.

For named-reference lookdev and handoff, the bundle must also carry one complete
reference-comparison matrix. Each standard dimension needs the criterion taken
from the approved spec, an observed reference fact, an observed candidate fact,
the exact render roles used, and a hashed spec citation. Missing dimensions or
unpaired observations fail closed. Known user rejections remain separate named
records and require exact-candidate closure evidence; a later generic PASS does
not erase them.

## Stage 5 — Independent Verification

Builder self-review runs every loop. Add independent read-only reviewers when:

- the user previously rejected the result;
- a named reference controls final art direction;
- Boolean/Geometry Nodes/complex modifier topology is involved;
- the style will be propagated to many assets;
- the scene is entering handoff or visual lock.

Use at least:

- a **visual verifier** receiving approved refs, spec, and native renders;
- a **technical verifier** receiving the `.blend`, audit contract, and scripts.

Keep reviewer context independent enough to challenge the builder. Reviewers
return evidence, severity, and `pass/block/review`, not only taste commentary.
The lead integrates disagreements and reruns gates after fixes.

Store each independent lane as a separate immutable JSON artifact bound to the
exact scene hash and the hashes of the images it actually inspected. The
certificate step may aggregate those artifacts, but must never author a verdict,
invent reviewer identities, or replace missing reports with hard-coded PASS
rows. Distinct required lanes need distinct reviewer identities.

If independent agents are unavailable, perform a fresh-context review pass and
record that reduced independence explicitly.

## Stage 6 — Final Gate

Copy `review_bundle.template.json` to the game/project evidence directory and
fill it with current evidence. Validate it:

```powershell
python ai_studio/assets/tools/blender/gate.py path/to/review_bundle.json
```

Profiles:

- `blockout`: composition, scale, silhouette and basic topology;
- `lookdev`: topology, architecture/style, palette, lighting and full evidence;
- `handoff`: lookdev plus naming, transforms, dependencies and wireframe proof.

The gate fails closed when references are missing, technical/art checks are not
`pass`, evidence hashes differ, required renders are absent, independent review
is missing, or a critical/high finding remains open.

Baseline or reserve promotion is a post-gate operation. The branch ledger must
receive a passing gate result for that exact scene hash; status text or a
selection rationale is not sufficient authority.

## Done And Stop Rules

Do not show a packet as finished when:

- a known fixable technical defect is hidden by the camera;
- the result drifted from the approved style or reference contract;
- only some requested components are finished;
- evidence belongs to an older scene hash;
- the builder is the only reviewer where independent review is required;
- `critical` or `high` findings remain open;
- the next step would propagate an unapproved module.

User review should contain genuine choices of taste or direction, not defects
the pipeline already knows how to find and fix.

Incident history behind these gates lives in
[production_lessons.md](production_lessons.md#full-gate-incidents).

## Vertex Colour Bake

`vertex_color_bake.py` moves a sourced model's colour out of its texture and
into `COLOR_0`. Free CC0 models normally carry colour in one shared palette
atlas; a game that draws vertex colour with no textures imports such a model
grey, which is why an otherwise usable model gets rejected and rebuilt by hand.

The tool is one file with three entry points over one pipeline, so the panel and
the command produce a byte-identical GLB from the same parameters.

### Install the add-on

Edit > Preferences > Add-ons > the dropdown > Install from Disk, pick
`ai_studio/assets/tools/blender/vertex_color_bake.py`, enable
`AI Studio Vertex Colour Bake`. The panel is View3D > sidebar (`N`) > AI Studio.
Blender copies the file, so re-install after the repository copy changes.

### Bake headlessly

```powershell
& $blender --background --factory-startup `
  --python ai_studio/assets/tools/blender/vertex_color_bake.py -- `
  --input <sourced.glb> --output <game>/assets/models/environment/park_bench.glb `
  --height 1.55 --pivot floor `
  --report tmp/vertex_color_bake/park_bench.json `
  --proof tmp/vertex_color_bake/park_bench_proof.png
```

Accepted sources are `.glb`, `.gltf`, `.fbx` and `.obj`. Omitting `--input`
bakes the open scene, which is how a `.blend` opened by Blender's own arguments
is converted.

Parameters:

- `--height` metres, measured on the game's Y axis; `0` keeps the source size.
  The scale is always uniform, because a non-uniform one leaves the exported
  normals off unit length.
- `--pivot floor|center|top|keep` plus `--pivot-offset X,Y,Z` in game Y-up
  metres. The pivot is the model point that lands on the origin, so a grip point
  is `--pivot floor --pivot-offset 0,<grip height>,0`.
- `--yaw` degrees around the game's up axis.
- `--sample face|corner` and `--filter nearest|bilinear`. `face` samples the UV
  centroid of each polygon and paints the whole face with it, which is what a
  flat palette fill needs. `corner` samples every loop, which follows a texture
  that actually varies across a face.
- `--report` writes what came in, which material became which fills, the
  resulting bounds and counts, and the contract audit.
- `--proof` renders the same geometry twice from one camera, textured and then
  vertex-coloured, and reports the pixel difference between them. Both passes
  use flat Workbench shading and the Standard view transform, so a correct sRGB
  decode makes the halves identical and a wrong one shows as a colour shift.

### Check a shipped file

The audit half needs no Blender:

```powershell
python ai_studio/assets/tools/blender/vertex_color_bake.py --audit <shipped.glb>
```

It requires one node with an identity transform, one mesh with one primitive,
`POSITION`/`NORMAL`/`COLOR_0` with colour as float RGBA in `0..1`, unit normals,
and no images, textures or external URIs.

### Limits

- Colour follows Principled/Emission base colour through image textures, RGB
  constants, existing colour attributes and constant multiply mixes. Any other
  node graph falls back to the material's viewport colour and says so in the
  report, as `"source": "fallback"` with a reason.
- Alpha is always `1`. Cutout foliage that relies on texture alpha loses its
  holes and needs real geometry.
- One output object. Split a kit into separate GLBs by running the tool once per
  source file.
- The grip pivot is a number the caller supplies; nothing infers it from
  geometry.

## Validation

```powershell
node ai_studio/studio.mjs verify --domain assets
node ai_studio/architecture_map/validate_map.mjs --strict
```

`verify --domain assets` runs every `test_*.py` here and in `lib/`. A single file:
`node ai_studio/dev_environment/python_run.mjs -m unittest ai_studio.assets.tools.blender.test_glb_check`.
The library, check and card tests need no Blender. `test_bpy_tools.py` and the bake
tests start Blender and skip when none is found.

The bake tests build their own palette-atlas model, so they need Blender: set
`BLENDER` when it is not in `C:\Program Files\Blender Foundation` or on `PATH`.
They assert that each flat fill comes back as the source colour, that the export
passes the contract, and that the panel and the command line write the same
bytes.

Quality reporting must keep dimensions separate. Typical outcomes are
`QART_001`, `QASSET_001`, and `QTECH_001`; one green result does not override a
block in another dimension.
