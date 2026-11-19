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
- `audio/`: local zero-shot SFX tagging (`tag_audio.py`, CLAP) that writes
  tag proposals for a pack's audio records; applying them is a storage step.
  Also `import_audio_pack.py`: converts source audio to 48 kHz 16-bit WAV and
  writes a Pack Manifest pack from a plan.
- `textures/`: standalone material texture checks, including 2x2 repeat preview
  generation and seam metrics.
- `blender/`: shared Blender and GLB tooling for every game. `review.py` renders
  views and runs every check into one review card per model; `run_blender.py`
  runs headless Blender with the Studio library importable; `lib/` reads, poses
  and writes GLB without Blender and holds `bpy` scene, export and palette
  helpers; `mesh_check.py`, `glb_check.py`, `contact_check.py` and
  `clip_check.py` are advisory checks with an opt-in `--strict` gate. Also holds
  `vertex_color_bake.py` (palette-atlas colour into `COLOR_0`), the optional
  fail-closed gate for hero scenes, and `production_lessons.md`.
