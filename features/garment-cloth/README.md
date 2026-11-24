# Garment cloth

## Purpose

Simulate separately authored garment surfaces on an animated character, on the scheme NvCloth and Chaos Cloth use.
Each sim vertex has a skinned reference; the cloth moves around it inside a max-distance sphere and stays outside a
backstop sphere behind it, collides with the body's spheres and cones and with the game's convexes, and the reference
is never repaired by the runtime. Each actor owns a `ClothState`; actors share the immutable mesh and material. The
host supplies the animated pose in one Y-up coordinate space measured in meters.

## Public surface

| Input or operation | Meaning |
|---|---|
| Render model contract | `_CLOTH_PIECE` (float, integer values) labels the render vertices that follow each piece; 0 or no attribute is not cloth. A piece's seam is its unlabelled side, which is skinned. Skin weights reference the body's joints. Everything else the cloth needs is authored in the outfit's script and built into its sidecar. |
| `garment/asset.h`, `garment_blob.encode`, `decode` | Blob version 3: outfit-wide strings, bones, spheres, cones and corrective names (`CNAM`, optional); per piece the mesh, skin, max distance, backstop, body direction and corrective shapes (`CORR`, optional: rest-space position and body direction deltas, absent where all are zero). The writer checks every value the solver trusts; `garment_asset_view` asserts the structure and returns the `GarmentSet` view. [Round trip](tests/test_asset.c). |
| `garment/actor.h` | `garment_actor_update(actor, set, skeleton, &GarmentInput)`: pose, world, dt, explicit `reset`, world convexes, the runtime max-distance scale and bias, and `corrective_weight` (one weight in [0, 1] per corrective, its index from `garment_set_corrective(set, name)`; NULL is all 0). The reference is `skin(rest + Σ weight·delta)` and the body direction is turned the same way; a weight change between frames blends across the substeps as the joints do, and weight 0 skips the shape. Joints are interpolated by local slerp and forward kinematics at every substep, and the reference, body direction, shapes and root follow them. `garment_actor_piece` reads a piece's cloth and its frame reference, for tools. [Actor tests](tests/test_actor.c) · [Integration](INSTALL.md#simulate-a-wearer). |
| `cloth_reset`, `cloth_step`, `cloth_measure` | Reset puts the cloth on its reference with no velocity, a free vertex inside a shape leaving along `-b` and one inside a convex through its nearest face. A substep integrates, then repeats max distance, tethers, fabric, backstop and collision with friction, then runs max distance, the strain passes, backstop and collision; velocity comes from positions. The strain limit follows max distance, so where the constraints cannot all hold the fabric keeps its limit and the defect shows as excess beyond max distance, not as a torn triangle. The backstop is a sphere of radius `R`, or with `R = +inf` the half-space behind its plane. `cloth_measure` reports strain, penetration, speed, the excess beyond max distance, the depth inside the backstop and the reference vertices a shape holds deeper than their max distance. |
| `ClothReference`, `ClothShapes`, `ClothConvex` | Per substep: each vertex's reference point and unit body direction; spheres at current and previous centres, the cones that join them and convex plane sets in simulation space. |
| `ClothMaterial` | Surface density in kg/m², anisotropic stretch and bend compliance (`WARP`/`WEFT`/`DIAG` scales), drag, normal `aero_drag`, contact thickness, iterations, the strain limit and its passes, tension-field `compression_limit` and the Coulomb `friction` coefficient. |
| `garment/contact.h` | Exact round-cone SDF between two spheres (`cloth_point_cone_contact`) and triangle-cone contact (`cloth_face_contact`). |
| `garment/debug.h` | Weight, max-distance, tether, reference, backstop, shape and skeleton overlays as line segments. |
| `cloth_vertex_frames_prepare`, `cloth_vertex_quats_eval` | Smooth per-vertex orthonormal transfer (hybrid barycentric-position + unit-quaternion records with `det(R) == 1` on sharp folds). |
| `garment_host` library, `garment.host` | The runtime offline (`tools/host/garment_host.c`, built from the same sources at `-O2`; Windows): load a blob on a skeleton, step it with model-space poses and world convexes, read positions, references, render records and metrics, and store a corrective shape (`gh_corrective`) that the runtime then evaluates by its weight (`gh_step`). `Skeleton.from_glb`, `rest_pose`, `clip_pose` and `ramp` (local slerp, as the actor interpolates) give the poses; `bake` drapes into a key pose (rest, ramp, convexes raised from below, settle) and `deltas` turns the drape into rest-space corrective deltas. The consumer build makes the library (`node ai_studio/feature_testing/run.mjs`); `GARMENT_HOST` names another. [Host tests](tests/test_host.c) |
| `garment.outfit`, `garment.sim`, `garment.body`, `garment.model` | The authoring library an agent writes one script per outfit against (spec C.1): `Outfit(script, root, model, body, fabrics)` (body: the declared complete body file, or the model's primitives as a fallback with holes) and its `Piece`s, each a sim mesh held as numpy arrays and edited by plain functions selected by boolean masks over `piece.f` fields. Sim mesh: `create` (the outer sewn sheet of the labelled render region, reduced to a vertex count), `set_mesh`, `cut`, `density`, `relax`, `offset`, `delete`. Maps: `pin`, `pin_bound` (pins every sim vertex that carries a render vertex which must follow skinning, so the seam stays exact), `unpin`, `gradient` (also continued from a depth on: tight to a line, opening below it), `paint`, `smooth` (of `r` at once; of the derived `d`, `R` an override the build applies after deriving them). Weights: `weights_follow_body` (declared; the default for a piece with a body behind it: the body's weights at the closest point, averaged within each vertex's own `r`, the final pins keeping theirs; later weight writes are refused), built from `weights_from`, `weights_transfer` (cut-aware closest point, or the k nearest), `weights_average` (mean within a per-vertex geodesic radius), `weights_set`, `weights_blend`, `weights_smooth`. `Outfit.over` declares a layer order, `Outfit.separate` declares that the build offsets the outer mesh where the gap is thinner than the two fabrics, `Outfit.layer` declares the B.6 rule (both layers ride one skinning where they overlap, and the gap it splits is the narrowest over the outfit's clips); `stages(script)` is the content stage a build includes: `GARMENT_STAGES`, else the `stage` the outfit's declaration ships. Every write records its script line (`piece.why`). `Outfit.corrective(name, clip, t | pose, convexes)` declares a named corrective the build bakes last through the host (`garment.corrective`): the spec's bake, then reset onto the corrected reference, hold and re-take the drape until it stays within the fabric's thickness of the reference (the residual drift, the passes and the key of everything it read, the runtime sources included, are recorded; a bake that does not converge stops the build; a seated clip with no declared seat is refused unless `convexes=[]`; a committed corrective whose key matches is kept without a bake, `GARMENT_REBAKE=1` bakes again); `Outfit.input` declares a file the script reads into the cloth a build input. `Outfit.write` resolves the derived maps in the build order below, writes the sidecar and its asset record (`<model>.cloth.provenance.json`). A declared body file must pass `body.prove` (closed parts, consistent outward orientation, no crossing triangles: what the winding number needs). [Tests](tests/test_authoring.py), [bake tests](tests/test_host.py) |
| `garment.sidecar` | The sidecar `<model>.cloth.glb` (spec B.1): the skin's skeleton nodes and inverse binds, a skinned mesh `piece:<id>` per piece with `_MAX_DISTANCE`, `_BACKSTOP_DISTANCE`, `_BACKSTOP_RADIUS`, `_BODY_DIRECTION` and corrective morph targets, `extras.build` (the hash of every input: script, files read, imported modules, numpy and scipy versions, library, body; `garment.build`), `extras.correctives` (each bake's key, convexes, passes and residual drift), `extras.check` (what the gates read besides the sidecar) and `extras.findings` (what a derivation could not do as its rule says). `write(read(data)) == data`. |
| `garment.cook` | The cook from the sidecar (B.7, B.8): `cook(outfit_id, sidecar bytes, model, fabrics, colliders, inertia)` returns the blob v3 (edges, rest angles, warp/weft kinds and tethers derived from the sim mesh and its pins; the outfit's bones and body shapes) and the `_GARMENT` stream of every primitive from the binding grown over the render mesh; `stage_model` adds the stream to a model, `stale` names every build input that changed (and, given the colliders, a body they were not fitted on; per corrective, a changed runtime or bake input). It depends on the authoring library and `garment_blob` only. |
| `garment.bind`, `garment.gates`, `garment/check.py` | The render binding grown over the render mesh (B.7) and the gates of C.3 on a built sidecar: G1 maps and the cook's structural mesh checks, G2 seam (the seam and the split copies of unlabelled vertices, across primitives), G3 cut, bind (a render vertex farther from its bound sim triangle than that triangle's longest edge), G4 reach against the body (not decided on the fallback body), G5 forced strain, G6 side (a reference turned to the far side of a limb it can reach), G7 reach against the shapes (samples failing G4 are G4's where G4 decides), G8 layer order, G9 looks, G10 the game's seat (where a garment covers the posed body in a seated clip, the body above the seat top by twice that fabric's thickness: the runtime keeps the cloth one thickness outside the body shapes and one above a convex), G4-G8 again at each corrective's key on the reference the game evaluates there, each bake's residual drift (`bake`), the build's findings (`derive`: free vertices with no fitted body radius, inner vertices the layer rule made kinematic, outer vertices with no room; `sheet`: render triangles the sim sheet leaves out), plus reports of free vertices over holes of the body and clips whose manifest entry names a corrective the outfit lacks (`correctives`). A gate that measured nothing is `not_run` and fails where enforced. One table (`gates.ENFORCED_FROM`) decides for `check.py` and the cook: stage 1 enforces the rebuild, G1, G3, G9 and `bake`; from stage 2 every gate. `python tools/garment/check.py <script>` reruns the script into scratch, compares the bytes with the committed sidecar (the rebuild check), prints one JSON finding per gate and piece naming the worst clip, time and vertices, writes a card of each failure and one per corrective (the reference at its key without and with it, and the cloth the runtime holds on it) with `--cards`, and exits 1 on an enforced failure, the cook's decision for that stage; an unknown `--clips` name is refused, and a stale rebuild names each changed input (a committed corrective whose key matches is kept, so a check needs no host unless a bake must run again). |
| `garment.colliders` | The body shapes of one body model, fitted by one rule (`fit`): the smallest round cone over the skin of each rig segment, given way where it would hold a reference deeper than its max distance; `read`, `write` (`garment.colliders.v1`), the reach measures `g7` and `g4_failing` (G4 through `body.reach_violation`) and `report`. `python tools/garment/fit_colliders.py <script> <segments.json> <out.json>` fits an outfit's set on the complete body its script declares (no fallback) and the sidecar it builds, and writes the set's asset record; a segment whose joints share a rest position and a non-finite value are refused. |
| `garment.cards` | Static cards of authored data (`raster` draws them z-buffered; `bind_card` is the render binding layer): the sim wire over the posed body, each map as heat with a numeric legend, weights per joint, the body direction, the reference in a clip pose, body shapes, and marked vertices with their script lines. |
| `garment.transfer`, `garment.view` | The shader's render transfer in numpy, and PNG cards of shaded triangle layers from a yaw (drawn mesh beside the sim mesh with its wire, marked vertices, measure lines), for agents without a GUI. |

## Build order

A script authors the sim mesh, the pins, `r` and its own weights, which change at once, and declares the rules that
derive maps from other maps. `Outfit.built` (and so `write` and `check.py`) resolves them on copies of the pieces in
one order fixed by their dependencies, whatever order the script declared them in:

1. `weights_follow_body`: needs the final pins and `r`;
2. `separate`: moves the outer sim mesh, so it comes before anything measured on the mesh;
3. the backstop `b`, `d`, `R` of every piece from the body: needs the final mesh and pins; `R` is the body radius
   behind every free vertex whatever its authored `r` (the runtime may grow `r` by its max-distance scale and bias,
   and a backstop with `d >= r` never acts anyway), 0 on a pin and at a hole;
4. `layer`: needs the weights, `b`, `d` and `r`; shares the weights, sets the outer `d` and limits the inner `r`;
5. the script's `paint` and `smooth` of `d` and `R`, in the order it wrote them (an override, carried through later
   mesh edits like the pins);
6. the correctives, in the order declared: each needs every map final.

## Validation

The feature-test consumer runs one test per constraint: max distance ends on its sphere; `r = 0` equals the reference;
the backstop ends on its sphere along `b`, and a plane backstop on its plane; a shape holding the reference deeper than `r` is reported; tethers hold; a
cone sweeping a pinned panel at 15 m/s leaves no vertex inside after any substep, and a thin one that lands on it
pushes the vertices out and leaves every vertex within its max distance; a two-joint arm turning 90° between
samples puts its tip on the arc; reset gives `x = ref` and exits along `-b`; friction holds a vertex on a slope below μ
and lets it slide above; a vertex entering a convex through its top leaves through the top. The host steps exactly what the
solver gives when stepped directly, a ramp ends on its key pose, a stored corrective is the reference the solver uses,
a drape stored as a corrective is the reference and the reset cloth at its key, and a bake repeats until its drape
holds within the fabric's thickness. The actor's corrective is the skinned corrected rest at weight 1 and the plain
reference bit for bit at 0, blends linearly, puts the cloth on the corrected reference at a reset, and refuses a weight
outside [0, 1]. The blob tests write a
synthetic outfit in Python and compare every field the C view reads bit for bit; the writer's tests reject each value
the solver trusts.

Contact is discrete, as in NvCloth: a vertex leaves a shape by the nearest exit, and the reference constraints,
not a remembered side, decide where the cloth belongs. A shape that moves more than its radius plus the fabric
thickness in one substep can therefore pass through cloth without pushing it (about 9.7 m/s for a 5 cm hand at
180 Hz); max distance and the backstop then place the cloth. Content that needs more calls for continuous collision
of spheres and cones, as in NvCloth, not a memory of the side. Collision
has the last word in a substep, so the excess beyond max distance and the depth inside the backstop are
measured, not forced to zero: a shape that holds a reference deeper than its `r`, or pushes cloth against its
backstop, shows up there. Self-contact and inter-garment contact are outside the feature.

## Compatibility

Version `4.0.0` supports up to 512 proxy vertices and 1536 unique edges per piece, 64 spheres and 64 cones per
outfit, 16 correctives per outfit and convexes of up to 48 planes. A PATCH preserves public inputs and corrects implementation defects. A MINOR
adds compatible input options or metrics. A MAJOR changes coordinate, ownership, material or data-layout contracts. The
blob carries `GARMENT_BLOB_VERSION`; the view asserts it, so recook every outfit and rebuild all linked targets on
upgrade.

## Extension points

The game owns garment IDs, outfit scripts and their sidecars, fabrics, body files, collider sets, convexes, reset
events and presentation. See the [install guide](INSTALL.md).
