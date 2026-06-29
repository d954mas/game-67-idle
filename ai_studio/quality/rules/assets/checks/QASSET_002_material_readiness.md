---
id: QASSET_002
name: Material Readiness
group: assets
description: Use when material-dependent assets can lose textures, UVs, colors, material maps, or material assignment during source intake, prep, conversion, or runtime-ready use.
---

# QASSET_002 Material Readiness

## What It Checks

Material-dependent assets keep intentional material data for their current
stage: source intake, preparation/conversion, or runtime-ready use.

## Use When

Use when adding, accepting, preparing, converting, generating, or claiming
runtime-ready material-dependent models, textures, materials, props, surfaces,
or material map sets where texture, UV, color, or material assignment affects
player-facing output.

## Do Not Use For

- source/provenance, license, publishability, or restricted routing by itself;
- runtime path/format readiness without material-dependent visuals;
- art direction, composition, or style fit;
- player-facing clarity or layout;
- runtime/build behavior unrelated to asset material data.

## Check

- source/accepted stage has material information: textures, UVs,
  per-primitive colors, vertex colors, material maps, shader/material
  assignment, or an explicitly accepted flat material;
- prepared/converted stage preserves material files, UVs, and material
  assignment in the manifest or runtime-ready asset;
- runtime-ready stage has viewer, runtime, or screenshot proof that material
  data is present and not accidentally flat;
- intentionally flat materials are accepted by the task or lead.

## Evidence

Source material record, material/texture files, UV/material metadata, manifest
entries, conversion/prep log, model/material viewer screenshot, runtime
screenshot, or lead/task note accepting an intentionally flat material.

## Not Enough

- A visible material-dependent asset renders as flat color by accident.
- Source asset has no material information when material output is expected.
- Texture files exist but are not referenced by the prepared/runtime-ready asset.
- Conversion/prep claims success but material assignment is missing.
- A flat material is claimed intentional without task or lead acceptance.
