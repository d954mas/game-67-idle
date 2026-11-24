# Garment cloth

## Purpose

Simulate separately authored garment surfaces on an animated character. Each
actor owns a `ClothState`; actors can share the immutable mesh and material.
The CPU solver uses XPBD length and signed dihedral constraints. Capsule
contacts inspect triangle interiors, edges and vertices. The host supplies
the animated pose in one Y-up coordinate space measured in meters.

## Public surface

| Input or operation | Meaning |
|---|---|
| Author contract in the GLB | `_CLOTH_PIECE` (float, integer values) labels the vertices of each piece; 0 or no attribute is not cloth. `_CLOTH` (float, 0..1) is the freedom map: 0 is a pin that follows skinning and anchors the tethers, a free vertex is driven toward its target at `drive_hz` × (1 − `_CLOTH`). A piece's seam is its unlabelled side, which is skinned; a labelled vertex split from an unlabelled one at the same position must be a pin, or it would open a crack. Skin weights reference body joints only and are the cloth's target: they are authored like any skinned garment's, and the feature ships no weight tool. |
| `garment_paint.baseline_freedom`, `garment_paint.coincident` | Paints `_CLOTH` from a piece's geometry: the top seam band and every vertex split from an unlabelled one (`coincident`) are pins, and the drive falls with surface distance from them. |
| `garment_glb.Glb` | Reads a one-buffer GLB and rewrites it in place: same-size attributes are overwritten in their own views, new ones are appended, a body-only rig drops the trailing joints, so a repeated identical write reproduces the bytes. |
| `garment_cook.cook_outfit`, `bind_outfit`, `stage_model` | Cook a model's pieces into one blob (proxy, pins, drive, travel, tethers, colliders) and bind the render vertices to the proxy in a `_GARMENT` attribute. A render pin is skinned; a free vertex follows the cloth by the bound proxy triangle's interpolated "not a pin" indicator. The cook refuses a lost label, a label no recipe piece names, `_CLOTH` off a piece, a free vertex split from an unlabelled one, weights outside the body rig, and a render edge whose two ends bind to proxy places that lie far apart along the proxy (`garment_binding.torn_edges`): such an edge spans an opening of the sheet and would stretch across it. |
| `garment_fit_capsules.fit_body_capsules` | Fits the segments declared in a character's body asset to rest-space vertices and joint names. |
| `garment/asset.h` | The cooked-outfit blob: header, section table and record layouts. `garment_asset_view` asserts the blob's structure once and returns the `GarmentSet` the actor binds, pointing into the blob; `garment_asset_free` releases that view. [Round trip](tests/test_asset.c) · [Loading](INSTALL.md#load-a-cooked-outfit). |
| `garment_blob.encode`, `garment_blob.decode` | Write one outfit's pieces as a blob and read it back. The writer checks every value the solver trusts (finite numbers, ranges, capacities, indices) and names the outfit, piece and field it rejects. `garment_blob.single` is its single-precision rounding. |
| `garment/actor.h` | Binds a cooked outfit to a NeoTolis skeleton, advances the cloth at fixed intervals, and supplies a posed palette and inspection overlays. Each wearer owns its state. [Small consumer](tests/test_actor.c) · [Integration](INSTALL.md#inspect-a-posed-garment). |
| `ClothSupport` | A finite, one-sided scene surface supplied per wearer. Its center and two half-edges describe a cushion or ledge in its actual pose. [Scene contact](INSTALL.md#scene-contact). |
| `garment/debug.h` | Draws weight and tether overlays from a posed cloth mesh and wire outlines for current body capsules and skeleton links. Send its line segments to the host's debug renderer. [Integration example](INSTALL.md#inspect-a-posed-garment). |
| `garment_cook.cook_mesh` | Produces the mesh arrays of one piece from ordered triangle indices, rest positions and pins. The cooker derives lumped masses, edge lengths and signed hinge angles. |
| `ClothMaterial` | Surface density in kg/m², length and angular compliance, exponential velocity drag per second, contact thickness in meters and solver passes per substep. Zero compliance gives a rigid constraint at convergence. |
| `ClothMaterial.max_edge_strain`, `strain_passes` | Optional tensile strain limit. A value of 0.12 begins shortening edges above 112% of rest length. Set 1–32 extra edge sweeps per substep to enable it. Zero sweeps use the ordinary solver. |
| `ClothMaterial.reset_carry_range_m` | Positive distance in meters outside a body collider over which a free vertex inherits less displacement from its neighbors at spawn or teleport. At reset, the solver also evaluates the fully propagated pose and keeps the localized pose only if it lowers peak edge strain without increasing peak capsule penetration beyond numerical tolerance. Zero uses full propagation. |
| `ClothGuide` | A per-vertex motion sphere around the target and an inward backstop plane through it, along the skinned normal that is also the contact fallback. Supply a unit outward normal, a travel radius and an inward allowance in meters. A null guide disables both limits. The backstop holds cloth where the body capsules do not cover the skin. |
| `ClothTether` | Optional one-sided distance limit to the closest pin by edge-path length. It bounds accumulated extension under a small solver budget and permits shorter, folded paths. |
| `cloth_reset`, `cloth_reset_supported`, `cloth_step` | Bind a cooked mesh and initial pose (optionally resolving initial body penetration outward from pinned seams for mid-pose spawns), then advance one fixed substep with explicit velocity, reference-frame fictitious inertia and ground plane. `cloth_push_targets` pushes a free vertex's skinned anchor out of the body capsules and then the support, in two passes with the contact margin, each exit on the side the target came from; the actor calls it once per pose sample and the steps interpolate the pushed targets. |
| `ClothMaterial` (v1.0 extensions) | Anisotropic `WARP`/`WEFT`/`DIAG` stretch and bend scales, Grinspun Discrete Shells area-scaled bending with sliver-triangle regularization, tension-field soft compression (`compression_limit`), normal-only `aero_drag`, a per-vertex XPBD drive toward the pushed target (`ClothMesh.drive`, squared angular frequency in s⁻²), `contact_friction` and `max_speed_mps`. |
| `garment/contact.h` | Exact closed-form tapered round-cone SDF (`cloth_point_capsule_contact`), temporal previous-side recovery (`cloth_point_capsule_contact_swept`), and face interior contact (`cloth_face_contact`). |
| `cloth_vertex_frames_prepare`, `cloth_vertex_quats_eval` | Smooth per-vertex orthonormal transfer (non-degenerate hybrid barycentric-position + unit-quaternion records with `det(R) == 1` on sharp folds). |
| `garment_proxy.py`, `garment_fit_capsules.py` | The outer sewn sheet of a render piece and its reduction to a vertex budget, checkerboard quad triangulation, geodesic depth, warp/weft edge classification, bind-pose validation, long-edge subdivision, quality-driven isotropic remeshing (`improve_mesh_quality`) and tapered body-capsule fitting (`fit_tapered_capsule`). |
| `cloth_frame_valid` | Checks one frame's inputs: time step, anchors, guide contract and capsules. The mesh and material are cooked data and are only asserted for capacity. Reset also checks the pose. An invalid frame preserves positions and velocities, and clears `state.frame_valid` so the host can report it. |
| `cloth_measure` | Returns maximum positive edge strain, body-proxy penetration and pin drift in meters, maximum speed in m/s and state finiteness. Strain is a ratio to the rest length. |
| `garment/contact.h` | Queries a triangle against a capsule including its effective thickness. For a positive penetration the outputs identify the nearest triangle point and correction direction. |

## Validation

The independent feature-test consumer cooks a folded surface and a pinned
panel, then runs them through the public C target. Its core checks cover
oriented hinges, triangle-interior contact, bounded corrections near a sewn
edge, bad frame inputs, winding and disconnected pieces. The blob tests write a
synthetic two-piece outfit in Python and compare every field the C view reads
with the writer's values bit for bit, including a blob that is only 4-byte
aligned; the writer's tests reject each value the solver no longer checks.

The optional strain sweeps follow the ordinary constraints. They preserve
pins and can move free vertices outside a motion guide or a tether. Capsule
contacts are projected again after each pair of sweeps and after an odd
final sweep. Both contact and a limited iteration budget can leave residual
strain. Measure residual strain, contact and solve time together when tuning an item.

The contact metric is discrete and relative to the supplied capsules. A
consumer qualifies garments on its actual skinned body and visible render
mesh. Record release-device cost, large pose changes, layering and the worst
animation-frame residuals. This version supplies one-way body contacts and
velocity-level tangential friction relative to moving colliders. Self-contact,
inter-garment contact and continuous collision detection are outside its coverage.

## Compatibility

Version `1.1.0` supports up to 512 proxy vertices and 1536 unique edges per
instance. It uses explicit velocity integration in Y-up meters, anisotropic
`WARP`/`WEFT`/`DIAG` XPBD constraints with Grinspun Discrete Shells bending,
tapered round-cone contacts, collision-aware reset and smooth per-vertex
quaternion/frame surface transfer. A PATCH preserves public inputs and corrects
implementation defects. A MINOR adds compatible input options or metrics. A
MAJOR changes coordinate, ownership, material or data-layout contracts. The blob
carries `GARMENT_BLOB_VERSION`; a change to its records bumps it, and the view
asserts the version, so recook every outfit and rebuild all linked targets on
upgrade.

## Extension points

The game owns garment IDs, material presets, attachment maps, topology
seams, skeleton sampling, colliders, frame accumulation and presentation.
Use the same garment instance across clip transitions and reset or rebase
both position buffers at a known teleport. Author alternate meshes, motion
limits and collision sets through data. See the [install guide](INSTALL.md).
