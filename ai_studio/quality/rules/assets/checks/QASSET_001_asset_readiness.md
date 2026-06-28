---
id: QASSET_001
name: Asset Readiness
group: assets
description: Use when assets, provenance, licenses, publishability, integrity, runtime-ready format, or visible material proof changed.
---

# QASSET_001 Asset Readiness

## What It Checks

Checks whether an asset is legally/procedurally usable and technically ready for
runtime use.

## Use When

Adding, moving, generating, converting, sourcing, or claiming assets as ready.

## Do Not Use For

- player-facing clarity by itself;
- art direction, composition, or style fit;
- runtime/build behavior unrelated to assets;
- game-loop or GDD validation.

## Check

- license;
- provenance and source path/page;
- publishability or restricted routing;
- integrity where available;
- runtime-ready path and format;
- visual/material proof when the asset is visible in-game.

## Evidence

Use asset manifest, source/provenance link, license note, source path,
generation record, integrity hash where available, runtime path, load/render
proof, or screenshot for visible assets.

## 3D Material Floor

For active 3D games that claim sourced or ready GLB/GLTF models, prove source
materials, textures, UVs, per-primitive colors, or equivalent material data.
Flat color-only rendering is not a ready-asset proof.

If the floor fails, stop content expansion and create a material/texture pass.

## Not Enough

- A file exists but has no source/provenance.
- License/provenance exists but runtime path or format is unclear.
- Visible assets are claimed ready without load/render or material proof.
