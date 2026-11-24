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
| Author contract in the GLB | `_CLOTH_PIECE` (float, integer values) labels the vertices of each piece; 0 or no attribute is not cloth. `_CLOTH` = 0 marks the pins, which follow skinning and anchor the tethers. A piece's seam is its unlabelled side, which is skinned; a labelled vertex split from an unlabelled one at the same position must be a pin, or it would open a crack. Skin weights reference body joints only and are the cloth's reference. |
| `garment_cook.cook_outfit`, `bind_outfit`, `stage_model` | Cook a model's pieces into one blob (proxy, pins, tethers, skin, max distance, backstop, body direction, the outfit's bones and shapes) and bind the render vertices to the proxy in a `_GARMENT` attribute. The recipe gives each piece's max-distance gradient (`rate`, `cap_m`) and the outfit's body primitives and colliders. [Cook](INSTALL.md#cook). |
| `garment_cook.body_maps` | The body direction `b`, backstop distance `d` and radius `R` of each proxy vertex from a body surface at rest, by the meaning rules of the spec. |
| `garment/asset.h`, `garment_blob.encode`, `decode` | Blob version 3: outfit-wide strings, bones, spheres and cones; per piece the mesh, skin, max distance, backstop and body direction. The writer checks every value the solver trusts; `garment_asset_view` asserts the structure and returns the `GarmentSet` view. [Round trip](tests/test_asset.c). |
| `garment/actor.h` | `garment_actor_update(actor, set, skeleton, &GarmentInput)`: pose, world, dt, explicit `reset`, world convexes and the runtime max-distance scale and bias. Joints are interpolated by local slerp and forward kinematics at every substep, and the reference, body direction, shapes and root follow them. [Actor tests](tests/test_actor.c) · [Integration](INSTALL.md#simulate-a-wearer). |
| `cloth_reset`, `cloth_step`, `cloth_measure` | Reset puts the cloth on its reference with no velocity, a free vertex inside a shape leaving along `-b` and one inside a convex through its nearest face. A substep integrates, then repeats max distance, tethers, fabric, backstop and collision with friction, then strain passes, max distance, backstop and collision; velocity comes from positions. The backstop is a sphere of radius `R`, or with `R = +inf` the half-space behind its plane. `cloth_measure` reports strain, penetration, speed, the excess beyond max distance, the depth inside the backstop and the reference vertices a shape holds deeper than their max distance. |
| `ClothReference`, `ClothShapes`, `ClothConvex` | Per substep: each vertex's reference point and unit body direction; spheres at current and previous centres, the cones that join them and convex plane sets in simulation space. |
| `ClothMaterial` | Surface density in kg/m², anisotropic stretch and bend compliance (`WARP`/`WEFT`/`DIAG` scales), drag, normal `aero_drag`, contact thickness, iterations, the strain limit and its passes, tension-field `compression_limit` and the Coulomb `friction` coefficient. |
| `garment/contact.h` | Exact round-cone SDF between two spheres (`cloth_point_cone_contact`) and triangle-cone contact (`cloth_face_contact`). |
| `garment/debug.h` | Weight, max-distance, tether, reference, backstop, shape and skeleton overlays as line segments. |
| `cloth_vertex_frames_prepare`, `cloth_vertex_quats_eval` | Smooth per-vertex orthonormal transfer (hybrid barycentric-position + unit-quaternion records with `det(R) == 1` on sharp folds). |
| `garment_proxy.py`, `garment_paint.py`, `garment_fit_capsules.py`, `garment_glb.Glb` | Proxy construction from the outer sheet, a `_CLOTH` pin painter, body-capsule fitting and the GLB reader and writer the cook uses. |

## Validation

The feature-test consumer runs one test per constraint: max distance ends on its sphere; `r = 0` equals the reference;
the backstop ends on its sphere along `b`; a shape holding the reference deeper than `r` is reported; tethers hold; a
cone sweeping a pinned panel at 15 m/s leaves no vertex inside after any substep, and a thin one leaves every
vertex outside it and within its max distance; a two-joint arm turning 90° between
samples puts its tip on the arc; reset gives `x = ref` and exits along `-b`; friction holds a vertex on a slope below μ
and lets it slide above; a vertex entering a convex through its top leaves through the top. The blob tests write a
synthetic outfit in Python and compare every field the C view reads bit for bit; the writer's tests reject each value
the solver trusts.

Contact is discrete, as in NvCloth: a vertex leaves a shape by the nearest exit, and the reference constraints,
not a remembered side, decide where the cloth belongs. A shape that moves more than its own thickness in one substep
can therefore pass through cloth without pushing it; max distance and the backstop then place the cloth. Collision
has the last word in a substep, so the excess beyond max distance and the depth inside the backstop are
measured, not forced to zero: a shape that holds a reference deeper than its `r`, or pushes cloth against its
backstop, shows up there. Self-contact and inter-garment contact are outside the feature.

## Compatibility

Version `2.0.0` supports up to 512 proxy vertices and 1536 unique edges per piece, 64 spheres and 64 cones per
outfit and convexes of up to 48 planes. A PATCH preserves public inputs and corrects implementation defects. A MINOR
adds compatible input options or metrics. A MAJOR changes coordinate, ownership, material or data-layout contracts. The
blob carries `GARMENT_BLOB_VERSION`; the view asserts it, so recook every outfit and rebuild all linked targets on
upgrade.

## Extension points

The game owns garment IDs, fabric presets, recipes, body primitives, colliders, convexes, reset events and
presentation. See the [install guide](INSTALL.md).
