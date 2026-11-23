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

## Author the model

The cook reads its contract from the garment GLB. Each piece is the set of vertices whose float attribute
`_CLOTH_PIECE` holds the piece's number (any unique positive integer; 0 or no attribute is not cloth), across every
primitive. `_CLOTH` is the piece's freedom: 0 is a pin that follows skinning and anchors the tethers, and a free
vertex is driven toward its target at `drive_hz` x (1 - `_CLOTH`). A piece's border with any other part must be a pin,
so the render seam is closed by construction. Skin weights reference the body's joints only. They are the cloth's
target, the pose each free vertex is driven toward, and are authored like the weights of any skinned garment; the
feature does not generate them.

Put the feature's `tools/` on the authoring Python path. To paint `_CLOTH` and drop the garment joints:

```python
from garment_glb import Glb
from garment_paint import baseline_freedom

freedom = baseline_freedom(piece_points, piece_faces, seam_fraction=0.05, curve=(0.85, 0.42, 0.8), sewn=border)
model = Glb(path)
model.set_attribute(primitive, "_CLOTH", freedom)            # appended once, rewritten in place afterwards
model.truncate_skin(len(body_joints))                         # garment joints must be the trailing nodes
path.write_bytes(model.to_bytes())
```

## Cook

`garment_cook.cook_outfit` builds each piece's simulation proxy from the outer sheet of its `source_material`
primitive, interpolates `_CLOTH` and the weights onto it, and writes the blob. `bind_outfit` binds the render
vertices of the model, or of another model wearing the same garment, to the proxy, and `stage_model` adds the
`_GARMENT` attribute the shader reads:

```python
from garment_cook import bind_outfit, cook_outfit, depfile_text, stage_model

piece = dict(id="coat", piece=1, source_material="cloth", outer_surface=None, budget=240, drive_hz=1.9, swing=0.33,
             material=dict(density=0.24, stretch=1e-5, bend=0.4, drag=2.0, thickness=0.003, iterations=2),
             colliders=[("Hips", [-0.07, 0.9, 0], "Hips", [0.07, 0.9, 0], 0.09, 0.0, 0)],
             linear_inertia=0.45, angular_inertia=0.35, downward_follow=0.85)
blob, sheets, report = cook_outfit("coat", Glb(path), [piece], body_joints)
staged_path.write_bytes(stage_model(Glb(path), bind_outfit(Glb(path), sheets, body_joints)))
depfile_path.write_text(depfile_text(blob_path, inputs))
```

`swing` is the travel radius per metre of fabric to the nearest pin and `backstop_per_m` how far below its target, along the skinned normal, a free vertex may sink per metre. Colliders are round cones in rest model space,
the margin is the fabric thickness, and flag 1 marks an outside obstacle. The report lists, per piece, the free
proxy vertices that already lie inside their own colliders at rest. The cook refuses a lost label, a label no
recipe piece names, `_CLOTH` off a piece or on a free border, and weights outside the body rig; `garment_blob.encode`
checks every value the solver trusts and names the outfit, piece and field it rejects. Add the blob to the pack with
`nt_builder_add_blob`; tools read it back with `garment_blob.decode`.

For radii measured from a character mesh, pass rest-space points in meters, one joint name per point, and the
authored body description to `garment_fit_capsules.fit_body_capsules(points, joints, body, shape)`.

`cook_mesh` remains the low-level step for one surface: invalid winding, repeated or degenerate triangles,
out-of-bounds indices and capacity overflows fail it, and `wearable=True` requires a pin on every connected piece.
`tether_slack=1.0` limits each reachable free vertex to the rest edge-path length to its nearest pin; `1.03` adds
three percent. Body contact runs after tethers and may leave residual extension when the two conflict.

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
track whether its blob is in by the winner, not by the view.

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
It also checks mesh edges that cross the object between two endpoints. A free
vertex's target inside the object leaves it through the face it entered by, after
the body capsules, so the drive never pulls fabric into the seat. Fabric can
slide off the edge and swing back underneath. Zero depth represents a one-sided
sheet.
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
