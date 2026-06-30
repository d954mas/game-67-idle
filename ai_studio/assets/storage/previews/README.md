# Preview Pipeline

Local preparation module for reusable asset previews.

## Role

Preview Pipeline creates or regenerates derived preview images before the
browser viewer displays assets. It owns deterministic local preview jobs, not
gallery UI and not asset metadata ownership.

Generated previews are cache files:

```text
tmp/ai_studio/assets/previews/<source-id>/<asset-id>/preview.<ext>
tmp/ai_studio/assets/previews/<source-id>/<asset-id>/preview.json
```

They are rebuilt on demand and must not be committed.

The viewer uses `cache.mjs` for the normal `Refresh previews` action. It
copies missing or stale image previews for template and game sources, renders
missing or stale GLB/GLTF thumbnails through Blender when possible, then
refreshes that source index so cards point at the fresh cache.

For Pack Manifest sources, existing source previews referenced by
`assets.jsonl` are also treated as preview changes. If the current index still
marks those assets as missing or stale, `Refresh previews` rebuilds the index so
the viewer picks up the source preview paths.

`preview.json` stores the source path, size, mtime, preview kind, and preview
size. A preview is stale when that sidecar is missing, from an old preview cache
version, or no longer matches the source file.

The batch command remains for large shared-library model runs:

```powershell
node ai_studio/assets/storage/previews/render_library_previews.mjs --source polypizza
node ai_studio/assets/storage/previews/render_library_previews.mjs --pack survival-kit
node ai_studio/assets/storage/previews/render_library_previews.mjs --all --limit 50
```

The command reads Pack Manifest asset records, finds GLB files, skips existing
library previews unless `--force` is passed, and runs Blender headlessly in
chunks.

## Boundary

- Asset Viewer displays prepared previews and may request explicit refresh or
  regenerate actions.
- Preview Pipeline performs local preparation jobs outside the browser page.
- No filesystem watch mode belongs here. Use page reload, manual refresh/reindex,
  or targeted regenerate actions after local asset changes.
- Source intake, licensing, conversion, and pack building are separate modules.

## Files

- `cache.mjs`: normal source-aware preview cache refresh used by Asset
  Viewer.
- `render_library_previews.mjs`: selects model assets and runs Blender in
  resumable chunks.
- `render_thumbs.py`: Blender script that renders one thumbnail per GLB from a
  fixed isometric camera.
- `make_studio_hdr.py`: rebuilds the shared studio lighting environment.
- `studio_env.hdr`: shared lighting used by both Blender thumbnails and the
  browser `model-viewer`.
