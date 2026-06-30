# Asset Prep Conversion

Conversion tools normalize source assets into formats the project can inspect
or import.

This group owns file conversion utilities only. It does not decide license,
provenance, storage manifest metadata, or runtime loading policy.

Current tool:

- `obj_to_glb.py`: Blender headless OBJ/MTL to GLB conversion with optional
  split-by-material output for engine-friendly meshes.

