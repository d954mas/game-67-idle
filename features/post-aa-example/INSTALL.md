# Post AA Example Install

## Choose

Start with native output. Copy an algorithm only after a same-window real-browser
comparison shows a visual benefit worth its measured GPU cost.

## Copy

Copy `example/post_aa_policy.h` into the consumer only when it needs selectable
or DPR-aware modes. Copy the relevant functions from `example/post_aa.glsl`
into the game's existing world composite shader; do not add a second composite
pass for this example.

For SSAA, allocate only the world target at `present_size * world_scale`. Keep
HUD and UI after the composite at native framebuffer resolution. For FXAA,
render the world at present size and call `post_aa_fxaa` from the composite.

## Verify

Compile and run the pure policy test; the script writes its executable to the
system temporary directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File features/post-aa-example/tests/run_test.ps1
```

Then build the consumer's WebGL2 target and compare median, p95, and motion at
the same CSS size and DPR.

## Uninstall

Set the world target to the resolved present size, sample it once in the
composite, remove copied policy/shader code and mode controls, then rebuild the
real browser target. No root feature source is linked in-place.
