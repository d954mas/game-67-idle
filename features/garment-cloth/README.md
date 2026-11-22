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
| `garment/actor.h` | Binds a cooked outfit to a NeoTolis skeleton, advances the cloth at fixed intervals, and supplies a posed palette and inspection overlays. Each wearer owns its state. [Small consumer](tests/test_actor.c) · [Integration](INSTALL.md#inspect-a-posed-garment). |
| `garment/debug.h` | Draws weight and tether overlays from a posed cloth mesh and wire outlines for current body capsules and skeleton links. Send its line segments to the host's debug renderer. [Integration example](INSTALL.md#inspect-a-posed-garment). |
| `garment_cook.cook_mesh` | Produces a C mesh from ordered triangle indices, rest positions and pins. The cooker derives lumped masses, edge lengths and signed hinge angles. |
| `ClothMaterial` | Surface density in kg/m², length and angular compliance, exponential velocity drag per second, contact thickness in meters and solver passes per substep. Zero compliance gives a rigid constraint at convergence. |
| `ClothMaterial.max_edge_strain`, `strain_passes` | Optional tensile strain limit. A value of 0.12 begins shortening edges above 112% of rest length. Set 1–32 extra edge sweeps per substep to enable it. Zero sweeps use the ordinary solver. |
| `ClothMaterial.reset_carry_range_m` | Positive distance in meters outside a body collider over which a free vertex inherits less displacement from its neighbors at spawn or teleport. At reset, the solver also evaluates the fully propagated pose and keeps the localized pose only if it lowers peak edge strain without increasing peak capsule penetration beyond numerical tolerance. Zero uses full propagation. |
| `ClothGuide` | A per-vertex motion sphere around the animated anchor and an inward backstop plane. Supply a unit outward normal, travel radius and inward allowance in meters. A null guide disables these limits. |
| `ClothTether` | Optional one-sided distance limit to the closest pin by edge-path length. It bounds accumulated extension under a small solver budget and permits shorter, folded paths. |
| `cloth_reset`, `cloth_reset_over_body`, `cloth_step`, `cloth_step_framed` | Bind a cooked mesh and initial pose (optionally resolving initial body penetration outward from pinned seams for mid-pose spawns), then advance one fixed substep with explicit velocity, reference-frame fictitious inertia, optional floor plane and anim-drive suppression. |
| `ClothMaterial` (v1.0 extensions) | Anisotropic `WARP`/`WEFT`/`DIAG` stretch and bend scales, Grinspun Discrete Shells area-scaled bending with sliver-triangle regularization, tension-field soft compression (`compression_limit`), normal-only `aero_drag`, `gravity_scale`, XPBD `anim_drive` with collider occlusion guard, `contact_friction`, `max_speed_mps` and `collider_mask`. |
| `garment/contact.h` | Exact closed-form tapered round-cone SDF (`cloth_point_capsule_contact`), temporal previous-side recovery (`cloth_point_capsule_contact_swept`), face interior contact (`cloth_face_contact`) and bone-free bilateral seated lap-bridge builder (`cloth_build_lap_bridge`). |
| `cloth_vertex_frames_prepare`, `cloth_vertex_frames_eval`, `cloth_vertex_quats_eval` | Smooth per-vertex orthonormal transfer (`3x4` rigid frames or non-degenerate hybrid barycentric-position + unit-quaternion records with `det(R) == 1` on sharp folds). |
| `garment_proxy.py`, `garment_fit_capsules.py` | Checkerboard quad triangulation, geodesic depth, warp/weft edge classification, bind-pose validation, long-edge subdivision, quality-driven isotropic remeshing (`improve_mesh_quality`) and tapered body-capsule fitting (`fit_tapered_capsule`). |
| `cloth_step_shaped` | Advance the same simulation, then enforce each free vertex's travel sphere and backstop around its current animated target. Pins follow their anchors. A null guide gives the same step as `cloth_step`. |
| `cloth_offset_surface` | Apply signed offsets in meters along area-weighted normals of the posed proxy to a separate output buffer. Pins and vertices with zero offset retain their exact input positions. |
| `cloth_pose_distance`, `cloth_pose_angle` | Evaluate a smooth 0–1 activation at the current pose. Inputs are two points in meters or three points defining an unsigned angle. Either threshold order is supported. |
| `cloth_pose_offsets` | Combine the signed displacements and per-vertex masks of active pose rules, then cap the total at the garment's displacement limit. |
| `garment_keyframes.normal_weight_map` | Cook painted weights or an ellipsoid described in the rest proxy's local coordinates into a static mask. |
| `garment_shape.SectionGuide` | Fit ordered garment cross sections to a selected support surface in the animated pose during authoring. The consuming game provides the section membership and support triangles. |
| `garment_keyframes.vertex_map` | Expand a scalar radius or validate a nonnegative, finite value per proxy vertex. |
| `garment_keyframes.authored_track` | Linearly sample full-proxy position keys onto integer ticks. Keys supply vertices in meters or an OBJ name resolved by the caller's mesh loader. |
| `garment_keyframes.normal_offsets` | Bake signed meter offsets per tick and vertex from overlapping painted or ellipsoidal patches with smooth activation keys. |
| `cloth_mesh_valid`, `cloth_frame_valid` | Check the index ranges, storage capacities, numeric inputs and guide contract. Reset also checks the mesh and pose. An invalid frame preserves positions and velocities, and clears `state.frame_valid` so the host can report it. |
| `cloth_measure` | Returns maximum positive edge strain, body-proxy penetration and pin drift in meters, maximum speed in m/s and state finiteness. Strain is a ratio to the rest length. |
| `garment/contact.h` | Queries a triangle against a capsule including its effective thickness. For a positive penetration the outputs identify the nearest triangle point and correction direction. |
| `garment/render_binding.h` | Evaluates an offline binding from a render surface to a deformed simulation triangle. |
| `garment_binding.fold_rest_agreements`, `cloth_render_normals`, `cloth_render_position_folded` | Optional render transfer. A fold that turns the support face away from its neighbors attenuates the signed shell height relative to its authored reference. |

## Validation

The independent feature-test consumer cooks a folded surface and a pinned
panel, then runs them through the public C target. Its core checks cover
oriented hinges, triangle-interior contact, bounded corrections near a sewn
edge, bad inputs, winding, disconnected pieces and strict pose bounds when a
capsule intersects the authored pose.

The optional strain sweeps follow the ordinary constraints. They preserve
pins and can move free vertices outside a motion guide or a tether. Capsule
contacts are projected again after each pair of sweeps and after an odd
final sweep. Both contact and a limited iteration budget can leave residual
strain. The hard envelope in `cloth_step_shaped` still has final priority.
Measure residual strain, contact and solve time together when tuning an item.

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
MAJOR changes coordinate, ownership, material or data-layout contracts. Rebuild
C cooks and all linked targets on upgrade.

## Extension points

The game owns garment IDs, material presets, attachment maps, topology
seams, skeleton sampling, colliders, frame accumulation and presentation.
The section fitter expects a surface described by ordered rows from its
attachment to its hem. Garments with branching geometry can supply their
own per-vertex animation targets directly to `cloth_step_shaped`.
Use the same garment instance across clip transitions and reset or rebase
both position buffers at a known teleport. Author alternate meshes, motion
limits and collision sets through data. See the [install guide](INSTALL.md).
