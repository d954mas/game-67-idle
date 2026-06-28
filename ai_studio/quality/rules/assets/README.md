# Asset Rules

Use this group when changed work adds, moves, generates, converts, sources, or
claims assets as ready.

## Order

1. Start with [COMMON.md](COMMON.md).
2. Add numbered checks only when their "Use When" section matches the task.

## Checks

### [QASSET_COMMON - Assets Common](COMMON.md)
Checks obvious asset blockers: missing source/generation path, missing
license/provenance, unclear runtime path/format, restricted asset misuse, or no
load/render proof for visible assets.

Use first when assets are added, moved, generated, converted, sourced, or
claimed as ready.

### [QASSET_001 - Asset Readiness](checks/QASSET_001_asset_readiness.md)
Checks whether an asset is legally/procedurally usable and technically ready for
runtime use.

Use when assets, provenance, licenses, publishability, integrity, runtime-ready
format, or visible material proof changed.

Record applied checks in the task log as `Quality: QASSET_001=pass` or
`Quality: QASSET_001=block`.
