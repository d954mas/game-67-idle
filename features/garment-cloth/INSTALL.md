# Install garment cloth

## Link

After adding the engine and defining the consuming target, link the module in
place. The target exports its C headers and `FEATURE_GARMENT_CLOTH=1`.

```cmake
set(GARMENT_CLOTH_DIR "${GAME_REPO_ROOT}/features/garment-cloth")
add_subdirectory("${GARMENT_CLOTH_DIR}" "${CMAKE_BINARY_DIR}/garment-cloth")
target_link_libraries(game PRIVATE garment_cloth)
```

Record the feature source and version from `feature.json` in the consuming
game's `dependencies.json`.

## Cook

Put the feature's `tools/` on the authoring Python path. Supply meters,
consistent triangle winding and zero-based indices. A slit has separate
vertices on its two free sides. A sewn region shares vertices. The cooker
uses NumPy during the build and writes immutable C arrays to a text stream.

```python
from garment_cook import cook_mesh

with output_header.open("w", encoding="ascii") as stream:
    cook_mesh(stream, "panel", rest_positions, triangles, pinned_indices,
              wearable=True, tether_slack=1.03)
```

Use `wearable=True` for attached items. Every connected piece must have a
pin and the garment needs a movable vertex. Free test sheets can use the
default. Invalid winding, repeated or degenerate triangles, out-of-bounds
indices and capacity overflows fail the cook. Include the generated header
in a single C translation unit after `garment/cloth.h`. Generate it under
the build directory with the input assets and `garment_cook.py` listed as
build dependencies. For a distinct render mesh, use
`garment_binding.bind_surface(points, faces, render_points, max_distance)`
and evaluate its binding through `garment/render_binding.h`. When two
seam lips coincide in the rest pose, pass `face_candidates` with the
allowed simulation face indices per render vertex to preserve the side
assignment as the seam opens.

For a garment with a thin render shell, a sharply folded proxy triangle
can turn its signed normal offset toward the body. To fade the local shell
height as that happens, cook one reference value per render binding with
`garment_binding.fold_rest_agreements(points, faces, bindings)`. On each new
dressed proxy pose, call `cloth_render_normals(mesh, positions, &field)` once.
After success, call `cloth_render_position_folded(binding, mesh->face,
positions, &field, rest_agreement, output)` for each render vertex. A cache
is valid for the geometry and pose used to fill it. Consumers can obtain the
same rest reference from a rest cache via `cloth_render_normal_agreement`.

`ClothRenderNormals` occupies 18 KiB at the current mesh limits. The builder
uses another 16 KiB of temporary stack space for double-precision area sums.
Hosts with small thread or WebAssembly stacks can store the persistent cache
in caller-owned render memory. A host drawing garments sequentially can reuse
one cache after it has transferred all vertices of the previous garment.

The factor is `clamp(1 + posed_dot - rest_dot, 0, 1)`. The dot product compares
the supporting face normal with the interpolated area-weighted mean normals
of its corners. Opposing neighboring faces cancel smoothly at a tight bend.
Position can extrapolate across an edge. Normal weights are clamped
to that face, so separately indexed seam lips and garment layers stay separate.
The bind surface and rigidly transformed shapes keep their authored height.
Near-collinear triangles (minimum altitude at most one ten-millionth of
the longest edge) contribute no normal. Their shell vertices reduce to
the barycentric surface point, including through a normal reversal. Use
`cloth_render_position` for items requiring constant shell height.

For pose-driven silhouettes, prepare cross sections in descending order
from the attachment to the hem. The rows together assign every simulation
vertex exactly once. Open or irregular rows use explicit angular column
indices. A slit can assign two separate vertices to the same column.
The optional `SectionGuide` fitter requires SciPy in the authoring Python
environment. NumPy suffices for mesh cooking and keyframe sampling.

```python
from garment_shape import SectionGuide

sections = SectionGuide(rest_positions, rows, sides=24, clearance_m=0.008,
                        columns=vertex_columns)
target_pose = sections.fit(posed_targets, body_points, support_faces, frame=frame)
```

For each sample, supply triangles in the support area, garment and body
points in one space, and the rigid bind-to-pose frame when needed. The
returned positions use the input pose space and can become runtime targets.

`tether_slack=1.0` limits each reachable free vertex to the rest edge-path
length to its nearest attachment. `1.03` adds three percent allowance.
Omit the argument or pass `None` to disable tethers. Unpinned components
remain free. Body contact runs after tethers and may leave residual extension
when the two constraints conflict.

## Author keyframes

### Drive a mask from live animation

Cook the mask once with `garment_keyframes.normal_weight_map(patch, rest_positions)`.
The patch takes a scalar or per-vertex `weights` in [0,1], or `center_m` and
`radii_m` for an ellipsoid. The mask follows the proxy's vertex order across poses.
Store it with a signed displacement in meters and the garment's maximum
combined displacement. Positive displacement follows the outward winding normal.

Resolve the garment's named sample points on the skeleton at substep end.
Pass the resulting points in a common space to `garment/pose.h`.
For a proximity rule, set the distance at zero weight higher than the
distance at full weight. An angle rule uses the angle between the two
segments meeting at the pivot, with thresholds in [0,180] degrees.
Both functions saturate outside the thresholds and interpolate smoothly inside.
They return false on invalid thresholds, nonfinite inputs or an undefined angle.

```c
float activation;
if (cloth_pose_distance(point_a, point_b, inactive_m, full_m, &activation)) {
    ClothPoseOffset rule = {mask, offset_m, activation};
    if (cloth_pose_offsets(mesh->vertices, &rule, 1, garment_limit_m, offsets)) {
        cloth_offset_surface(mesh, simulation->x, offsets, dressed_positions);
    }
}
```

For several rules pass an array and its length. The signed contributions
add before the symmetric cap. A zero-length rule list yields zero offsets.
Invalid inputs preserve the destination so the caller can choose its
fallback. Re-evaluate at the blended pose during a transition. The feature
uses only the sample points, so playback rate and clip names are irrelevant.

### Drive a mask from a clip

This four-vertex panel keeps its top edge fixed. The local correction moves
the lower edge outward by 2 cm at tick 4 and fades to zero at tick 8.

```python
import numpy as np
from garment_keyframes import normal_offsets, vertex_map

panel = np.array([[0, 1, 0], [1, 1, 0], [0, 0, 0], [1, 0, 0]], dtype=float)
travel_m = vertex_map([0, 0, 0.04, 0.04], len(panel))
offsets_m = normal_offsets([{
    "weights": [0, 0, 1, 1], "offset_m": 0.02,
    "keys": [[0, 0], [4, 1], [8, 0]],
}], panel, count=9)
```

At tick `t` pass `offsets_m[t]` to `cloth_offset_surface`. The painted
weights and activation values range from 0 to 1. For a spatial patch,
supply `center_m` and three positive `radii_m`. Patch contributions add,
and a negative `offset_m` reverses the displacement direction. Each
activation key and patch amplitude uses a finite real scalar. A patch can
start or end active at the corresponding clip boundary. For an
authored full-surface pose, call `authored_track(keys, count, panel, faces,
load_obj, directory)`. Its first and last keys cover ticks 0 and
`count - 1`. Each key contains `tick` and either `vertices_m` or `obj`.
For OBJ keys the loader takes a path and returns positions and ordered
triangles matching the proxy. The result has shape `(count, vertices, 3)`.

## Simulate

```c
#include "garment/cloth.h"
#include "panel.gen.h"

static ClothState state;
static const ClothMaterial fabric = {
    .density = 0.24F, .stretch = 0.00001F, .bend = 0.4F,
    .drag = 2.0F, .thickness = 0.003F, .iterations = 6
};

void equip_panel(const float pose[][3]) {
    cloth_reset(&state, &panel, pose);
}

void step_panel(float seconds, const float pose[][3], const ClothGuide *limits,
                const ClothCapsule *capsules, int capsule_count) {
    cloth_step(&state, &panel, &fabric, seconds, pose, limits, capsules, capsule_count);
}
```

Keep `panel` and its arrays alive and immutable through every use of
`state`. Reset succeeds when `state.mesh == &panel`. Each pose and guide
array must cover every simulation vertex. Pins follow their current anchor.
The capsule endpoints define the centerline. Sample the rig and colliders
at physics frequency, then present `state.x`. A known translation can be
applied to both `state.x` and `state.previous` to carry inertia through a
world-origin shift.

Call `cloth_step_shaped` with the same arguments to make the authored pose
limits binding at the end of the substep. Supply externally owned end-pose
positions and guide normals for each sample. Set a free vertex's travel to
zero to place it exactly at its animated target. A positive travel permits
residual motion in meters, and the backstop caps inward motion along the
unit outward normal. A null guide uses ordinary simulation. A nonfinite or
invalid guide rejects the entire step and clears `state.frame_valid` while
preserving both position buffers. When a capsule overlaps the allowed
region, `cloth_measure` reports the penetration left by the final projection.

An authored per-vertex offset map can correct a local silhouette after
simulation. Pass the final proxy positions, an array of signed meters and a
separate output buffer to `cloth_offset_surface`. For example, pass
`cloth_offset_surface(&panel, state.x, offsets_m, display_pose)` and evaluate
render bindings against `display_pose`. Positive offsets follow the face
winding and negative offsets go inward on an outward-wound surface. Keep
the solver history in `state`. Measure body clearance and stretch on the
surface you present.

The function validates the mesh, pose and offset map. Invalid inputs or an
output buffer overlapping any input return `false` before writing. A valid
zero map copies the input pose exactly. Collapsed triangles contribute no
normal. If an offset vertex has a missing or cancelling normal, or an offset
exceeds the coordinate range, the function copies the entire input pose to
the output and returns `false`.

## Verify

From the Studio root, run the feature owner checks. The native fixture also
exposes the focused CTests after the first build.

```powershell
node ai_studio/feature_testing/run.mjs
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_garment_(cloth|cook|binding|shape|keyframes)$" --output-on-failure
```

Then run the consuming game's animation and body-fit fixtures. To uninstall,
remove its cook rules, target link, frame calls and dependency record.
