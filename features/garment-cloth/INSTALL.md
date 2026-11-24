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
primitive. `_CLOTH` = 0 marks the piece's pins: they follow skinning (max distance 0) and anchor the tethers; any
other value is free. The seam of a piece is its unlabelled side: those vertices are skinned and hold the border, so
the labelled row next to them may move. A labelled vertex split from an unlabelled one at the same position must be
a pin, or the render seam would open there. Skin weights reference the body's joints only. They are the cloth's
reference, the skinned pose its motion is measured from, and are authored like the weights of any skinned garment;
the feature does not generate them.

Put the feature's `tools/` on the authoring Python path. `garment_paint.baseline_freedom` paints a `_CLOTH` map from
a piece's geometry (the top seam band and copies of unlabelled vertices are pins); only its zeros matter to the cook.
A rig that still lists garment joints after the body's drops them with `model.truncate_skin(len(body_joints))`, which
refuses a vertex still weighted to a dropped joint.

## Cook

`garment_cook.cook_outfit` builds each piece's simulation proxy from the outer sheet of its `source_material`
primitive, carries the pins and the weights onto it, derives the max-distance and backstop maps, and writes the
blob. `bind_outfit` binds the render vertices of the model, or of another model wearing the same garment, to the
proxy, and `stage_model` adds the `_GARMENT` attribute the shader reads:

```python
from garment_cook import bind_outfit, cook_outfit, depfile_text, stage_model

piece = dict(id="coat", piece=1, source_material="cloth", outer_surface=None, budget=240,
             max_distance=dict(rate=0.3, cap_m=0.08),
             material=dict(density=0.24, stretch=1e-5, bend=0.4, drag=2.0, thickness=0.003, iterations=2, friction=0.4),
             linear_inertia=0.45, angular_inertia=0.35)
colliders = [("Hips", [-0.07, 0.9, 0], "Hips", [0.07, 0.9, 0], 0.09, 0.0)]
blob, sheets, report = cook_outfit("coat", Glb(path), [piece], body_joints, colliders=colliders,
                                   body_primitives=["skin", "legwear"])
staged_path.write_bytes(stage_model(Glb(path), bind_outfit(Glb(path), sheets, body_joints)))
depfile_path.write_text(depfile_text(blob_path, inputs))
```

The maps follow from meaning, not from tuning against old frames:

- `r = min(rate x geodesic distance to the pins, cap_m)`: 0 on the pins, growing along the fabric to the hem value
  the garment allows (a pencil skirt a few centimetres, a loose hem more).
- the body direction `b` points from each proxy vertex to the closest point of the `body_primitives` surface at rest,
  smoothed over the proxy; `d` is that distance less the thickness; `R` is the body radius behind the vertex, 1 over
  the largest principal curvature at the closest point, fitted to the body vertices of the smallest geodesic disc
  that determines a quadric, among those whose normals face the same side. A backstop is a local model of the body
  behind the vertex, so the disc does not grow with `r`: one wider than a limb wraps past its side. A flat or concave
  body gives the plane form `R = +inf`; no determining disc, or `d >= r`, turns the backstop off. `backstop=False` on a piece, or no body primitives, turns it off too.
- tethers allow the path length to the nearest pin times `1 + max_edge_strain` of the fabric.

Colliders are the outfit's one set of round cones in rest model space; the cook stores them as spheres on their
joints and the cones that join them, shared by every piece. The report lists, per piece, the proxy vertices that lie
inside a shape at rest and those a shape holds deeper than their `r`. The cook refuses a lost label, a label no recipe
piece names, `_CLOTH` off a piece, a free vertex split from an unlabelled one, and weights outside the body rig;
`garment_blob.encode` checks every value the solver trusts and names the outfit, piece and field it rejects. Add the
blob to the pack with `nt_builder_add_blob`; tools read it back with `garment_blob.decode`.

`cook_mesh` remains the low-level step for one surface: invalid winding, repeated or degenerate triangles,
out-of-bounds indices and capacity overflows fail it, and `wearable=True` requires a pin on every connected piece.
`tether_slack=1.0` limits each reachable free vertex to the rest edge-path length to its nearest pin; `1.03` adds
three percent.

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

## Simulate a wearer

For a NeoTolis character, include `garment/actor.h`. Keep a zero-initialized `GarmentActor` for each wearer and share
the outfit's `GarmentSet` view. Once per animation update pass the final model-space pose, the world transform, the
elapsed seconds and the scene's convexes:

```c
static const ClothConvex floor = {.plane_count = 1, .planes = {{0, 1, 0, 0}}}; /* world y <= 0 is inside */
const GarmentInput in = {.pose = pose, .world = world, .dt = dt, .reset = placed_anew, .render = true,
                         .convexes = &floor, .convex_count = 1, .max_distance_scale = 1.0F};
if (garment_actor_update(&actor, outfit, rig, &in)) draw_garment_surface(actor.palette, actor.palette_count);
```

Set `measure` where the stats' strain and constraint measures are read (a lab, a probe, a test); it costs a pass
per frame. Set `reset` on the first frame and whenever the wearer is placed anew: staged, seated, cut or teleported. The cloth
then starts on its reference with no velocity; there is no warm-up and no teleport guess, and an actor without a
previous update resets regardless. Between pose samples the actor slerps the joints' local rotations and runs forward
kinematics at every substep, so a fast turn sweeps the arc. `max_distance_scale` and `max_distance_bias` blend the
cloth toward the animation: scale 0 with bias 0 is fully skinned and skips the solve; a pin (cooked `r = 0`) stays
kinematic for any scale and bias.

A convex is a world-space plane set (`n . x + d <= 0` inside, up to `CLOTH_CONVEX_PLANES` planes): one plane for a
floor, a box plus its hull planes for a seat. The actor moves them into its own simulation space at every substep. A
vertex inside leaves through the face it entered by (at a reset, through the nearest face); friction is the fabric's
Coulomb `friction`. Contact with body shapes is discrete, to the nearest exit: a limb that moves more than its own
thickness in one substep can pass through cloth without pushing it, and the reference constraints then place it.

## Simulate a surface directly

```c
#include "garment/cloth.h"

static ClothState state;
static ClothReference ref[CLOTH_VERTICES]; /* skinned position and unit body direction per vertex */

cloth_reset(&state, panel, &fabric, ref, NULL, &shapes);
cloth_step(&state, panel, &fabric, 1.0F / 180.0F, NULL, ref, NULL, &shapes);
```

`NULL` max distance uses the mesh's `r`; pass a runtime array to scale it. `ClothShapes` holds spheres at their current
and previous substep centres, the cones joining them and the convexes in simulation space. Keep the view and its blob
alive and immutable through every use of `state`. Reset succeeds when `state.mesh == panel`. An invalid input clears
`state.frame_valid` and leaves the positions.

## Inspect a posed garment

```c
garment_actor_debug_lines(&actor, rig, pose, layers, selected_joint, draw_line, view);
```

`draw_line` receives endpoints in model space and their RGB color. Layers: `CLOTH_DEBUG_WEIGHTS` (a skeleton joint's
skin weights, or `-1` for the max-distance map), `TETHERS`, `SHAPES` (spheres and cones at the last substep),
`SKELETON`, `REFERENCE` (the reference wire), `MAX_DISTANCE` (each vertex's offset from its reference, red beyond `r`)
and `BACKSTOP` (the body direction from the reference to the backstop surface). Call `garment_actor_clear(&actor)`
when releasing the wearer. A host with its own posed vertices uses `cloth_debug_mesh`, `cloth_debug_reference`,
`cloth_debug_cone` and `cloth_debug_bone` directly.

## Verify

From the Studio root, run the feature owner checks. The native fixture also
exposes the focused CTests after the first build.

```powershell
node ai_studio/feature_testing/run.mjs
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_garment_" --output-on-failure
```

Then run the consuming game's animation and body-fit fixtures. To uninstall,
remove its cook rules, target link, frame calls and dependency record.
