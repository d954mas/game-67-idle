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

## Author the outfit

The render model labels the vertices each piece moves with the float attribute `_CLOTH_PIECE` (any unique positive
integer; 0 or no attribute is not cloth) and skins them on the body's joints. Everything the cloth needs beyond that
is authored in one Python script per outfit against `tools/garment` (put the feature's `tools/` on the path) and built
into the sidecar `<model>.cloth.glb` beside the model:

```python
from garment.outfit import Outfit, stages

# coat.json beside the script declares {"script": ..., "stage": 1, "sidecar": ..., "colliders": ...}: the one source
# of the body shapes the build, the bakes and the gates use.
o = Outfit(__file__, GAME, "assets/coat.glb", body="assets/coat.body.glb", fabrics="content/fabrics.json",
           clips="assets/clips/animations.manifest.json")
coat = o.piece("coat", 1, "wool")
coat.create("cloth", 240)                                       # the outer sewn sheet, reduced to 240 vertices
coat.pin_bound(0.1)                                             # the seam's render row follows skinning exactly
coat.gradient(rate=0.3, cap=0.08)                               # r: 0.3 m per metre of fabric from the pins, 8 cm cap
coat.weights_transfer(coat.render.surface(), nearest=4)         # the render weights, which the pins keep
coat.weights_follow_body()                                      # derived by the build once pins and r are final
o.write()                                                       # resolves the derived maps, writes sidecar + record
```

The maps follow from meaning, not from tuning against old frames:

- `r` (max distance) is 0 on the pins and grows along the fabric to the hem value the garment allows;
  `gradient(start=, base=)` keeps a piece tight to a line and opens it below.
- `b` points from each sim vertex to the closest point of the declared body at rest, smoothed over the sim mesh; `d`
  is that distance less the thickness; `R` is the body radius behind every free vertex (the plane form `+inf` where
  the body is flat or concave; 0, off, on a pin and where the body has a hole). The build derives them for every
  piece; `paint("R", mask, 0)` turns the backstop off where no body lies behind a piece (a ribbon).
- weights: the render weights (`weights_transfer`), or the body's under the cloth averaged within `r`
  (`weights_follow_body`); `Outfit.layer` makes an outer piece and the piece under it ride one skinning.

A pose that holds the legs far from the rest (sitting) gets a corrective, baked by the build after every map is final,
by the runtime itself through the `garment_host` library (build it with `node ai_studio/feature_testing/run.mjs`); a
committed corrective whose recorded inputs all match is kept without a bake:

```python
o.corrective("sit", clip="sit", t=2.0)   # the drape in that key, on the seat the outfit declares for the clip
```

A seat a bake rests on is declared with the model it is measured on (`seats={"sit": {"planes": rows, "source":
"assets/chair.glb"}}`), so the sidecar's record lists it with the clip; a sidecar derived from a file the game's
`assets/PURCHASED_EXPORTS.md` lists is refused until the project owner lists the sidecar there too.

The game weights it by name: `garment_set_corrective(set, "sit")` once per loaded set, then
`GarmentInput.corrective_weight[index]` 1 while a clip holding that pose plays, ramped over the transitions; the weights
of one update sum to at most 1 (`garment_corrective_weights_valid`), or the update fails. Derive the weights from the
animator's layers by one rule for every path that draws the wearer (play, previews, tools), and treat a placement that
resets the cloth (a seat, a teleport) as a discontinuity of the pose too: restart every per-actor spring that carries
motion across frames (a drawn yaw, secondary chains) on that frame, and on the first frame the cloth updates after it
reset the cloth and let the new clip take full weight at once, so the reset lands on the corrected reference the bake
held. Never snap the clip on a frame whose cloth does not reset.

`python tools/garment/check.py <script>` rebuilds the sidecar in scratch, compares its bytes with the committed one and
runs the gates (C.3 of the scheme); a complete check writes the outfit's receipt (`<script>.check.json`), which the cook
accepts instead of running the per-frame gates. `garment.cards` draws what the script authored.

## Cook

`garment.cook.cook(outfit_id, sidecar_bytes, model, fabrics, colliders, inertia)` returns the blob, the `_GARMENT`
stream of each primitive of the model (the binding grown over the render mesh) and a report; `stage_model` adds the
stream to a model, `stale(sidecar, root, script, colliders)` names every build input that changed since the sidecar
was built (files, imported modules, Python, numpy and scipy, the library, the colliders' body, a corrective's runtime),
and `depfile_text` writes the build's dependencies. The cook runs no per-frame gate: `receipt.accept(GAME, declaration)`
verifies the receipt `check.py` wrote (its digest, exactly the files and modules a check of the declared outfit reads,
each unchanged, the declared script, sidecar and stage, every gate the stage enforces run, no blocking finding by
`gates.blocking`, the rule `check.py` exits by), runs the gates that need no clip again on the committed sidecar and
returns the findings for the report; cook with the colliders the sidecar was built and gated on, and make the receipt
and every file it hashes dependencies of the cook:

```python
from garment import cook as C, model as M, receipt as R, sidecar as S

data = sidecar_path.read_bytes()
sidecar = S.read(data)
changed = C.stale(sidecar, GAME, script, sidecar.check["colliders"])
if changed:
    raise SystemExit(f"rebuild the sidecar ({'; '.join(changed)}): python {script}")
model = M.RenderModel(model_path)
findings = R.accept(GAME, declaration_path, model)  # ValueError names why not
colliders = json.loads((GAME / sidecar.check["colliders"]).read_text())
blob, streams, report = C.cook("coat", data, model, fabrics, colliders, (0.45, 0.35))
staged_path.write_bytes(C.stage_model(model_path, streams))
```

The tethers allow the path length to the nearest pin times `1 + max_edge_strain` of the fabric. `garment_blob.encode`
checks every value the solver trusts and names the outfit, piece and field it rejects. Add the blob to the pack with
`nt_builder_add_blob`; tools read it back with `garment_blob.decode`. `cook_mesh` is the low-level step for one
surface: invalid winding, repeated or degenerate triangles, out-of-bounds indices and capacity overflows fail it.

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

cloth_reset(&state, panel, &fabric, ref, NULL, NULL, &shapes);
cloth_step(&state, panel, &fabric, 1.0F / 180.0F, NULL, ref, NULL, NULL, &shapes);
```

`NULL` max distance uses the mesh's `r` and `NULL` backstop the mesh's `(d, R)`; pass runtime arrays to scale `r` or
to move the backstop with a corrective. `ClothShapes` holds spheres at their current
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
