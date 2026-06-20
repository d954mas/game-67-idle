# Shared Free Asset Library

Load this reference when downloading, organizing, reusing, or importing free
internet assets such as models, rigs, textures, materials, audio, and source UI
art.

## Location

Default local shared source library:

```text
C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets
```

This library is an OKF-style Markdown source-asset catalog, not a runtime pack.
Asset knowledge lives in `catalog/**/*.md`; binary files live in `files/**`.
Game code must not load files directly from this path. A project imports an
accepted asset by copying or converting required files into project-local
`assets/source/...` paths and keeping provenance.

## Search First

Always search Markdown records before opening binary folders:

```powershell
rg -n "tags:.*vehicle|has_animation: true|license: CC0" C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets\catalog
rg -n "tileable: true|kind: texture|metal|normal|roughness" C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets\catalog
```

Open the matching `catalog/<kind>/<asset-id>.md` record, then follow its
`resource:` path to `files/<kind>/<asset-id>/`.

## Folder Contract

Bootstrap the folder contract before the first download on a machine:

```powershell
node tools/assets/intake/bootstrap_shared_asset_library.mjs
```

- `_incoming/`: unreviewed downloads. Do not integrate into a game from here.
- `_quarantine/`: unclear license, partial download, broken extraction,
  suspicious provenance, or reference-only assets.
- `_templates/`: reusable asset/model/texture/material and download templates.
- `catalog/<kind>/<asset-id>.md`: OKF concept records with tags, license,
  formats, intended use, and `resource:` links.
- `files/<kind>/<asset-id>/`: original, extracted, normalized, or converted
  source files.
- `licenses/<asset-id>/`: license text, copied license files, or screenshots.
- `previews/<asset-id>/`: preview renders, thumbnails, contact sheets, 2x2
  texture previews, or visual review captures.
- `tools-notes/`: reusable download/conversion notes.

Use asset ids shaped as:

```text
<source>__<asset-slug>__<license>
```

Example: `quaternius__animated-robot-pack__cc0`.

## Download Intake

1. Search for legal assets before building major visual content from debug
   shapes or procedural placeholders.
2. Download into `_incoming/<source>/<asset-slug>/`.
3. Record the source page URL, direct download URL, author/vendor, downloaded
   date, local original path, and local extracted path.
4. Check the license before accepting the asset. If the license is unclear, put
   the asset in `_quarantine/` and use it only as a non-shipping reference.
5. Check file size/hash and extraction result. A partial or corrupt download
   stays in `_quarantine/`.
6. Create a Markdown record from `_templates/asset-record.md`,
   `model-record.md`, `texture-record.md`, or `material-record.md` before
   moving the asset into `catalog/`.

Use the helper when a direct URL or local downloaded file is available:

```powershell
node tools/assets/intake/download_source_asset.mjs --url <url-or-path> --source <source> --slug <asset-slug> --license-name <license-or-unknown>
```

The helper records size, SHA256, `download-log.md`, and `intake.json`. It does
not approve the license; the agent still has to perform the license gate before
moving anything into `catalog/` or a project.

After the license gate passes, accept the incoming asset with the catalog helper:

```powershell
node tools/assets/intake/accept_incoming_asset.mjs --source <source> --slug <asset-slug> --asset-id <source>__<asset-slug>__<license> --kind <model|texture|material|audio|ui> --title "<title>" --description "<searchable sentence>" --license-name <license> --license-url <url> --tags "<comma,tags>" --source-page-url <asset-page-url> --author-vendor "<name>"
```

For texture assets, store the tiling decision and proof paths in the catalog
record when applicable:

```powershell
node tools/assets/intake/accept_incoming_asset.mjs --source <source> --slug <asset-slug> --asset-id <asset-id> --kind texture --title "<title>" --description "<text>" --license-name <license> --license-url <url> --tags "texture,tileable,ground" --tileable true --wrap-mode repeat --preview-2x2 previews/<asset-id>/tile_2x2.png --seam-audit previews/<asset-id>/tile_audit.json
```

Acceptance copies the downloaded file, `download-log.md`, and `intake.json` into
`files/<kind>/<asset-id>/`, writes `catalog/<kind>/<asset-id>.md`, and creates a
license note under `licenses/<asset-id>/`. The catalog entry is the source of
truth for search; do not rely on `_incoming/` paths after acceptance.

## License Gate

An asset can move into `catalog/` only when the Markdown record states:

- license name and license URL or copied license file;
- whether attribution is required;
- whether commercial use, modification, and redistribution are allowed;
- shipping decision: `allowed`, `reference-only`, or `blocked`;
- enough source/provenance to audit the decision later.

Every catalog record should keep OKF-compatible frontmatter:

```yaml
---
type: Game Asset
title: Short Human Title
description: One sentence for search and selection.
resource: files/models/<asset-id>/
tags: [model, vehicle, cc0, stylized]
timestamp: 2026-06-20T00:00:00Z
asset_id: <source>__<asset-slug>__<license>
kind: model
status: accepted
license: CC0
---
```

Additional fields such as `formats`, `has_animation`, `tileable`, `wrap_mode`,
`preview_2x2`, or `uv_assumption` are allowed and encouraged. Unknown fields
must not break consumers.

Do not use protected IP, ripped game assets, unclear marketplace files, or
assets whose attribution/redistribution terms cannot be satisfied.

## Project Import Boundary

When using a catalog asset in a game:

1. Copy only the chosen files into the project.
2. Preserve the source asset id in the project evidence or source note.
3. Convert units, axis, materials, textures, and formats project-locally.
4. Record whether the imported copy is original, extracted, normalized,
   optimized, or runtime-packed.
5. Verify the asset in the runtime or a model/material viewer before claiming it
   solves visual quality.

The shared library helps agents avoid rediscovering downloads. It does not
replace project-specific runtime manifests, pack builders, or visual proof.
