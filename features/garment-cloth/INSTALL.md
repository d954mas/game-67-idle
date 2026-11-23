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

An item can point directly to a garment JSON file. Read it with
`garment_asset.load_asset(path)`. An asset contains a nonempty `pieces` list
with unique piece IDs and can define local body support profiles in `supports`.
The host resolves the mesh, fabric and body profile for the piece. For example,
a coat using one prepared skinned GLB primitive and a painted seam can use

```json
{"pieces":[{"id":"coat","surface":0,"pins":"_PINS","material":"wool","support":"torso","tether_slack":1.03}]}
```

For radii measured from a character mesh, pass rest-space points in meters,
one joint name per point, and the authored body description to
`garment_fit_capsules.fit_body_capsules(points, joints, body, shape)`.
Each shape lists segments in contact order. Each segment names its two
attachment joints, rest endpoints and joints whose vertices supply the fit.
Several garments can reuse a body description and select their own fit.

Put the feature's `tools/` on the authoring Python path. Supply meters,
consistent triangle winding and zero-based indices. A slit has separate
vertices on its two free sides. A sewn region shares vertices. The cooker
uses NumPy during the build. `cook_mesh` returns one piece's mesh arrays, and
`garment_blob.encode` writes an outfit's pieces as one blob:

```python
from garment_blob import encode
from garment_cook import cook_mesh

mesh = cook_mesh("coat", rest_positions, triangles, pinned_indices, wearable=True, tether_slack=1.03)
# bones: (joint name, row-major 3x4 inverse bind matrix) pairs; influences index into them.
piece = dict(id="coat", mesh=mesh, bones=bones,
             vertices=dict(bone=influences, weight=weights, normal=normals, travel=travel_m, backstop=backstop_m),
             material=dict(density=0.24, stretch=1e-5, bend=0.4, drag=2.0, thickness=0.003, iterations=6),
             colliders=[(bone_a, bone_b, point_a, point_b, radius_m, radius_b_m, 0)], render_vertices=count)
blob_path.write_bytes(encode("coat", [piece], owned_joints=["CoatBack"]))
```

Use `wearable=True` for attached items. Every connected piece must have a
pin and the garment needs a movable vertex. Free test sheets can use the
default. Invalid winding, repeated or degenerate triangles, out-of-bounds
indices and capacity overflows fail the cook. `encode` checks every value the
solver trusts and fails with the outfit, piece and field it rejects; the solver
asserts only the blob's structure at load. Write the blob under the build
directory with the input assets, `garment_cook.py` and `garment_blob.py` listed
as build dependencies, and add it to the pack with `nt_builder_add_blob`. Tools
read a blob back with `garment_blob.decode`. For a distinct render mesh, use
`garment_binding.bind_surface(points, faces, render_points, max_distance)`.
When two seam lips coincide in the rest pose, pass `face_candidates` with the
allowed simulation face indices per render vertex to preserve the side
assignment as the seam opens.

When the render and simulation surfaces share source vertex indices, use
`garment_binding.pack_indexed_surface(vertex_count, pins, palette_offset)`
for the packed vertex-frame stream. Pins keep their ordinary skinning and
each free vertex follows its own simulation vertex, including at coincident
slit edges. The offset counts preceding pieces' simulation vertices.

`tether_slack=1.0` limits each reachable free vertex to the rest edge-path
length to its nearest attachment. `1.03` adds three percent allowance.
Omit the argument or pass `None` to disable tethers. Unpinned components
remain free. Body contact runs after tethers and may leave residual extension
when the two constraints conflict.

## Author keyframes

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

The painted
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

## Load a cooked outfit

Request the blob with the outfit's model and view it once its winner is
published. The view points into the payload, so release it, and every actor
bound to it, before the blob's slot publishes another winner or its pack is
unmounted. Check after each `nt_resource_step`:

```c
#include "garment/asset.h"

uint32_t winner = nt_resource_is_ready(blob) ? nt_resource_winner_gen(blob) : 0;
if (winner != outfit->winner) {
    if (outfit->set) {
        garment_actor_clear(&wearer->actor);
        garment_asset_free(outfit->set);
        outfit->set = NULL;
    }
    outfit->winner = winner;
    if (winner) {
        uint32_t size = 0;
        const uint8_t *bytes = nt_resource_get_blob(blob, &size);
        outfit->set = garment_asset_view(bytes, size);
    }
}
```

A blob payload only needs 4-byte alignment; engine blob payloads have at least
the alignment of `malloc`. An outfit cooked without cloth views as NULL, so
track whether its blob is in by the winner, not by the view. A set's
`owned_joints` lists the skin joints the cloth drives, which other secondary
motion should leave alone.

## Simulate

```c
#include "garment/cloth.h"

static ClothState state;
static const ClothMaterial fabric = {
    .density = 0.24F, .stretch = 0.00001F, .bend = 0.4F,
    .drag = 2.0F, .thickness = 0.003F, .iterations = 6
};

static const ClothMesh *panel; /* outfit->pieces[0].mesh from the view */

void equip_panel(const float pose[][3]) {
    cloth_reset(&state, panel, pose);
}

void step_panel(float seconds, const float pose[][3], const ClothGuide *limits,
                const ClothCapsule *capsules, int capsule_count) {
    cloth_step(&state, panel, &fabric, seconds, NULL, pose, limits, capsules, capsule_count);
}
```

Keep the view and its blob alive and immutable through every use of
`state`. Reset succeeds when `state.mesh == panel`. Each pose and guide
array must cover every simulation vertex. Pins follow their current anchor.
The capsule endpoints define the centerline. Sample the rig and colliders
at physics frequency, then present `state.x`. A known translation can be
applied to both `state.x` and `state.previous` to carry inertia through a
world-origin shift.

## Scene contact

Pass the upper face of a supporting scene object to `garment_actor_update`.
Transform the center as a point and each half-edge as a vector using the object's
current world transform. Their cross product points toward the cloth. For a level
Y-up surface, use the positive X and negative Z half-edges in that order.
Set `depth` to the measured distance in world meters from the top to the underside.
The actor converts the face to its own simulation space for the duration of the call.
Pass `NULL` when the character has no supporting object.

For a shaped cushion, supply its closed outward planes in `ClothSupport.planes`
and set `plane_count`. A plane in model space satisfies `dot(n, x - center) + d <= 0`
inside the cushion. Transform its normal by the inverse transpose of the object's
world basis and keep the offset relative to the transformed center. Leave the
count at zero for a rectangular support.

At spawn or teleport, the solver resolves overlaps toward the nearest face.
It also checks mesh edges that cross the object between two endpoints.
Leave the material's `support_drape` at zero when the ordinary contact already
lays the piece over the object. For long fabric whose animated guide would pull
it through the object, use a weight from zero to one. The blocked guide moves
toward the rim on its animated side and recovers its hanging length from its
sewn attachment. A reachable guide above the object keeps its animated pose.
Fabric can slide off the edge and swing back underneath. Zero depth represents
a one-sided sheet.
The low-level solver accepts the same face in simulation coordinates through
`cloth_reset_supported` and `ClothFrame.support`.

## Inspect a posed garment

For a NeoTolis character, include `garment/actor.h`. Keep a zero-initialized `GarmentActor` for each wearer and share the outfit's `GarmentSet` view. Pass the character's final model-space skeletal pose, its world transform and elapsed seconds once per animation update. A successful update with `render=true` exposes `actor.palette` and `actor.palette_count` for drawing. In the workbench, inspect that same actor and pose:

```c
if (garment_actor_update(&actor, outfit, rig, pose, world, dt, true, NULL)) {
    draw_garment_surface(actor.palette, actor.palette_count);
    garment_actor_debug_lines(&actor, rig, pose, layers, selected_joint, draw_line, view);
}
```

`draw_garment_surface` and `draw_line` are host drawing functions. The line callback receives endpoints in model space and their RGB color. Apply the garment's model transform and camera. Set `selected_joint` to a skeleton index for skinning weights or `-1` for animation-drive weights. On a frozen pose, call only the debug function to change layers. Call `garment_actor_clear(&actor)` when releasing the wearer. The [isolated actor test](tests/test_actor.c) exercises different wearers of one asset with a two-joint rig.

For a host supplying its own posed vertices, include `garment/debug.h` and use the geometry helpers directly:

```c
cloth_debug_mesh(mesh, display_positions, weights, layers, draw_line, view);
if (layers & CLOTH_DEBUG_CAPSULES) {
    for (int i = 0; i < contact_count; ++i)
        cloth_debug_capsule(&contacts[i], draw_line, view);
}
if (layers & CLOTH_DEBUG_SKELETON) {
    for (int i = 0; i < joint_count; ++i)
        if (parent[i] >= 0)
            cloth_debug_bone(joint_position[parent[i]], joint_position[i], draw_line, view);
}
```

Call the mesh helper for each simulated piece. Pass `NULL` for its cooked animation-drive weights, or one float per proxy vertex for a selected joint's anchor-skinning weights. When the host interpolates the displayed pose, call the helper separately with `CLOTH_DEBUG_WEIGHTS` on the displayed positions and `CLOTH_DEBUG_TETHERS` on the physical positions. The tether color then measures actual distance against the cooked limit. Use the capsule endpoints and both radii from that same physical step and the skeleton from the displayed pose. Toggling layers changes only drawing.

## Verify

From the Studio root, run the feature owner checks. The native fixture also
exposes the focused CTests after the first build.

```powershell
node ai_studio/feature_testing/run.mjs
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_garment_" --output-on-failure
```

Then run the consuming game's animation and body-fit fixtures. To uninstall,
remove its cook rules, target link, frame calls and dependency record.
