---
id: QASSET_001
name: Asset Readiness
group: assets
description: Use when an asset is accepted for project use, copied into project-local assets, claimed publishable/restricted, claimed game-use-ready, or used by player-facing content.
---

# QASSET_001 Asset Readiness

## What It Checks

The asset claim is backed by the required record or proof: source/provenance,
origin, committed-asset integrity, license/publish routing, project-local path,
game-use format/path, and load/render evidence when the asset is game-use-ready
or player-facing.

## Use When

Use when an asset is accepted for project use, copied into project-local assets,
claimed publishable or restricted, claimed game-use-ready, or used by
player-facing content.

## Do Not Use For

- player-facing clarity by itself;
- art direction, composition, or style fit;
- runtime/build behavior unrelated to assets;
- game-loop or GDD validation.

## Check

- accepted/project-use assets record source, provenance, and origin;
- every committed asset records integrity/hash or the repository's accepted
  equivalent integrity field;
- publishable/restricted assets have license and routing recorded before
  publishing, redistribution, or git tracking;
- attribution-bearing assets, especially CC-BY, record author/credit and source
  page before being marked ready;
- project code uses project-local asset paths, not shared-library, incoming,
  scratch, or temp paths;
- generated, converted, or packed assets can be traced back to their source and
  builder/manifest record;
- game-use-ready assets have the expected game-use path, format, manifest entry,
  or pack entry;
- player-facing assets have load/render or viewer proof.

## Evidence

Asset record, manifest, source/provenance link, origin, integrity/hash,
license note, publish/restricted routing, project-local path,
generation/conversion record, builder or pack entry, game-use path/format,
load/render proof, or screenshot.

## Not Enough

- A file exists but has no source, provenance, or origin.
- A committed asset has no integrity/hash or accepted integrity field.
- A shared-library, incoming, scratch, or temp path is used as a project asset.
- License exists but publish/restricted routing is unclear.
- CC-BY or attribution-required asset exists without author/credit and source
  page.
- A generated, converted, or packed asset has no source or builder/manifest
  trace.
- Runtime-ready is claimed without game-use path, format, manifest/pack entry, or
  load proof.
- Player-facing use is claimed without load/render proof.
