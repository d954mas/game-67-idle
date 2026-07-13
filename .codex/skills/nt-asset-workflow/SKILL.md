---
name: nt-asset-workflow
description: "Use when sourcing, adding, moving, preparing, recording, promoting, pulling, reviewing, licensing, or validating game assets in this repository: add an asset to a game, reuse an asset from the global library, add a new asset or pack to the global library, handle paid/non-redistributable assets, refresh previews, inspect unregistered files, or update asset storage/viewer/index/manifest workflows. Source first, keep license/provenance, and route through ai_studio/assets."
---

# NT Asset Workflow

Use this as the main asset skill. Keep the source, prepared files, library,
game-local copy, generated preview/index, and engine integration as separate
decisions.

## Pick The Path

- **Find/reuse an existing asset**: search storage first, browse viewer if needed,
  then pull the selected asset into the game.
- **Add an asset to the global library**: intake or prepare it, record license and
  provenance, add/promote it into storage, then refresh the index/preview.
- **Add an asset only to the current game**: place it under the game asset root,
  keep manifest/source metadata, and let storage show unregistered files until
  the metadata is added.
- **3D models or meshes**: load `references/3d-models.md` for source-first model
  reuse, conversion, texture dedup, and engine pack/load integration.
- **Standalone textures or materials**: load `references/textures-and-materials.md`
  for tiling, material maps, downloaded texture provenance, and seam checks.
- **Paid or non-redistributable asset**: load
  `references/restricted-assets.md` before moving binaries.
- **Generated art, source sheets, cutouts, slice9, or atlas crops**: load
  `references/generated-assets-and-cutouts.md`.
- **Need to create raster source art after source search fails**: use
  `nt-asset-image-generation`, then return here for prep, storage, license,
  and project/library handoff.
- **Library search, license gate, pull/promote commands**: load
  `references/source-library-and-license.md`.

## Default Workflow

1. Identify the target: global library, template, or a specific game source.
2. Search before creating:
   `node ai_studio/assets/catalog/search.mjs --query "<need>" --kind <kind> --json`.
3. If reusing, pull from the library into the game:
   `node ai_studio/assets/gallery/pull.mjs --ids <asset-id> --to <game>/assets --apply`.
4. If adding/promoting, record `origin`, license, source page/path, author/vendor
   when known, and enough description/tags for future search.
5. Refresh the selected source from Asset Viewer or CLI so new/unregistered files
   are visible.
6. Verify the asset where it matters: viewer preview, prepared files, game
   runtime, or license guard depending on the task.

## Hard Rules

- Do not load game code from the global library path. Games use local copies.
- Do not commit paid, unknown-license, or non-redistributable binaries.
- Every accepted asset needs license, provenance, `origin`, and searchable
  metadata.
- Custom/unknown licenses default to non-publishable. Use the license registry
  decision before setting `publish=true`.
- Keep Markdown notes human-readable; generated indexes/previews live under
  `tmp/ai_studio/assets/`.
- Do not generate new art until the shared library and practical free sources
  have been checked or the user explicitly asks for generation.

## Useful Commands

```powershell
node ai_studio/assets/catalog/search.mjs --query "wood crate" --kind model --json
node ai_studio/assets/gallery/pull.mjs --ids <asset-id> --to <game>/assets --apply
node ai_studio/assets/gallery/promote.mjs --manifest <review-manifest.json> --ids <ids> --source <source> --license <license> --origin <mine|ai|sourced> --apply
node ai_studio/assets/previews/render_library_previews.mjs --pack <pack>
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Add `--refresh` to search only after files or manifests changed and the query
must include those changes.
