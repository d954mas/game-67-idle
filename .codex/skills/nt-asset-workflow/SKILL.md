---
name: nt-asset-workflow
description: "Source, import, prepare, promote, or validate game assets and licenses through the Studio asset library. Use for asset manifests and viewer workflows."
---

# NT Asset Workflow

Keep source art, prepared outputs, library storage, game-local copies, previews,
and engine integration distinct. Choose the work needed for the requested target.

## Workflow

- For reuse, search the library, then practical free CC0/OFL sources before
  generation. Use `node ai_studio/assets/catalog/search.mjs --query "<need>"
  --kind <kind> --json`; add `--refresh` only when changed files must be included.
- For library intake or promotion, preserve source metadata and refresh affected
  indexes/previews after changes. Existing valid metadata can be reused.
- For game-only intake, use the game's asset root and manifests. Games consume
  local copies, never runtime dependencies on global library paths.
- Verify the changed claim: license/provenance for intake, prepared output for
  conversion, viewer preview for discovery, and runtime evidence for game use.

## Conditional References

- 3D reuse, conversion and engine loading: `references/3d-models.md`.
- Character clips and retargeting: `references/animation.md`.
- Textures, materials and seam checks: `references/textures-and-materials.md`.
- Paid, account-gated or unknown licenses: `references/restricted-assets.md`
  before moving binaries.
- Generated art, alpha, source sheets, slice9 and crops:
  `references/generated-assets-and-cutouts.md`.
- Search, license gates, pull/promote commands:
  `references/source-library-and-license.md`.
- Raster creation after source search, or when explicitly requested:
  `nt-asset-image-generation`; return here for preparation and acceptance.

## Asset Contract

- Accepted assets need license, provenance, integrity/hash, `origin`, and useful
  searchable metadata. Custom/unknown licenses remain non-publishable until
  the license registry supports a publishable decision.
- Restricted binaries stay out of git except the explicit per-game grant in a
  private game's `assets/PURCHASED_EXPORTS.md`. That grant covers only shipped
  game-ready purchased exports, never source archives or unused pack content.
- Heavy non-shipping authoring files belong in synchronized
  `<YandexDisk>/gamedev/games/<game-id>/`; follow
  `ai_studio/assets/external_game_workfiles.md` and retain logical paths/hashes.
- Generated indexes/previews belong in `tmp/ai_studio/assets/`.
- Source-first and the Studio restriction on procedural final art still apply.

## Handoff

Report the accepted files, provenance/license status, verification evidence,
and remaining preparation or integration gaps. Canvas-to-game promotion uses
Canvas's acceptance and promotion operations; a preview/export alone is not
asset acceptance.
