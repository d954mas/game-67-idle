# Blender Production Feedback Loop

Canonical AI Studio workflow for building, reviewing, and handing off Blender
scenes. It prevents a successful render from being mistaken for a technically
clean or art-direction-complete result.

The repository skill surface is
`.codex/skills/nt-blender-production/SKILL.md`. This module owns the executable
gates and the detailed contract.

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

Run the loop after each material geometry change, not only at the end. Fix the
first failing modifier or design layer before adding more detail. After three
failed correction cycles on the same cause, stop patching and redesign that
construction.

## Stage 0 — Reference Lock

Before editing Blender, record:

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

## Incident Regression — City Square M02 v002

This pipeline exists because M02 v002 passed a render/count/modifier-order
check while deeper inspection found 208 degenerate faces after bevel, 76 open
boundary edges across volumetric arch pieces, six dirty modifier stages, 13
bevel policies, and a dormer visually overlaid on the roof.

`blender_scene_audit.py` reproduces that failure and returns `block`. Keep this
scene report as a regression example; never weaken the gates merely to make the
old artifact pass.

## Incident Regression — City Square M02 v004 r15

M02 v004 r15 had assigned material families and Noise/Bump nodes, but the Pale
Ochre facade still read as a uniform fill in the master frame. A roof-focused
review packet omitted a fitted facade material view and a dedicated material
review lane, so `QART_001` and `QASSET_002` were incorrectly reported as pass.

Keep the M02 v004 gate regression: it must require explicit material-surface
and material-scale art checks, fitted facade and colored-oblique renders, a
distinct material reviewer with named claims, and evidence-bearing quality
records. Technical shader construction cannot substitute for visual proof.

## Incident Regression — City Square M02 v004 r16

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

## Validation

```powershell
python -m unittest discover ai_studio/assets/tools/blender -p "test_*.py"
node ai_studio/studio.mjs verify --domain assets
node ai_studio/architecture_map/validate_map.mjs --strict
```

Quality reporting must keep dimensions separate. Typical outcomes are
`QART_001`, `QASSET_001`, and `QTECH_001`; one green result does not override a
block in another dimension.
