# Asset Tools

Concrete utilities for working on asset source files before they enter storage,
template assets, or a game project.

Asset tools do not own manifest metadata, licenses, indexes, or storage writes.
They produce normalized files, region JSON, sliced images, review sheets, ZIP
exports, audit JSON, Markdown reports, preview images, and local browser exports
that can be referenced by intake, game-owned generation notes, or quality
evidence. Working outputs belong under `tmp/`; accepted assets are promoted
later by the owning game/template/shared storage flow.

Review and tests are step-local verification. They run after a concrete action,
such as slicing a source sheet, removing alpha, extracting a cutout, converting a
model, or checking a texture. They are not a separate top-level asset group.

## Groups

- `lib/`: low-level shared Python helpers for asset tools.
- `image/`: the 2D image pipeline, decomposed per media type over a shared
  `_bridge` (background normalization, region detection, slicing, key-matte and
  dual-plate alpha,
  routing). Used by agents and the `../canvas/` module: source images in `tmp`,
  region JSON, sliced images, review sheets, and ZIP/PNG exports.
- `conversion/`: format conversion helpers such as OBJ/MTL to GLB.
- `model/`: 3D-model preparation and provider integrations. The Meshy client
  under `model/meshy/` is dry-run-first, credit-gated, resumable, and writes
  generated GLB candidates only under `tmp/`. The local Hunyuan3D adapter under
  `model/hunyuan3d/` is plan-first, supports loopback single-image plus offline
  multiview/texture modes, and records GLB provenance. The local Kimodo adapter
  under `model/kimodo/` generates humanoid motion clips from text and retargets
  them onto a character rig with foot lock.
- `textures/`: standalone material texture checks, including 2x2 repeat preview
  generation and seam metrics.
- `blender/`: fail-closed Blender modeling/lookdev workflow with evaluated-mesh
  topology audits, reference/style mismatch review, fresh-hash evidence packets,
  independent verification, and completion gates before rollout or handoff. Also
  holds `vertex_color_bake.py`, the add-on and headless command that bakes a
  sourced model's palette-atlas colour into `COLOR_0` and normalizes it to the
  flat vertex-colour contract games import.
