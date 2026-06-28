---
id: QASSET_COMMON
name: Assets Common
group: assets
description: Use first when assets are added, moved, generated, converted, sourced, or claimed as ready and you need a cheap pass for obvious source, license, provenance, runtime-path, restricted-use, or load/render blockers.
---

# Assets Common

Use this first when changed work adds, moves, generates, converts, sources, or
claims assets as ready.

## What It Checks

Catches obvious asset blockers before spending time on numbered asset checks.

## Use When

Assets are added, moved, generated, converted, sourced, or claimed as ready.

## Do Not Use For

- player-facing clarity by itself;
- art direction, composition, or style fit;
- runtime/build behavior unrelated to assets;
- game-loop or GDD validation.

## Check

- source or generation path is recorded;
- license/provenance is not missing;
- runtime path and format are clear;
- restricted assets are not treated as redistributable;
- visible assets have some proof that they load/render.

If any item fails, fix it before using numbered asset checks.

## Evidence

Use asset manifest, source/provenance link, license note, source path,
generation record, runtime path, load/render proof, or screenshot for visible
assets.

## Not Enough

- A file exists but has no source/provenance.
- A visible asset is claimed ready without load/render proof.
- A restricted or paid asset treated as redistributable.

## Record As

```text
Quality: QASSET_COMMON=pass; evidence: <manifest/source/runtime proof>
```
