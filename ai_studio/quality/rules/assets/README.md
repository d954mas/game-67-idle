# Asset Rules

Use this group when asset files or records are added, changed, converted, moved
into project use, claimed publishable, claimed runtime-ready, or used as asset
dependencies in player-facing output.

## Not For

- player-facing clarity or layout: use
  [Player Clarity](../player_clarity/README.md);
- art direction, composition, or style fit: use [Art](../art/README.md);
- runtime/build behavior unrelated to assets: use
  [Technical](../technical/README.md).

## Checks

### [QASSET_001 - Asset Readiness](checks/QASSET_001_asset_readiness.md)

Checks: accepted source/provenance, license/publish routing, project-local
asset path, runtime format/path, and load/render proof for the claim being made.

Use when: an asset is accepted for project use, copied into project-local
assets, claimed publishable/restricted, claimed runtime-ready, or used by
player-facing content.

### [QASSET_002 - Material Readiness](checks/QASSET_002_material_readiness.md)

Checks: material-dependent assets keep intentional textures, UVs, colors,
material maps, or material assignment for their current stage.

Use when: a material-dependent model, texture, material, prop, surface, or
material map set can lose material data during source intake, prep, conversion,
or runtime-ready use.

Record applied checks in the task log using the outcome format from the Quality
README.
