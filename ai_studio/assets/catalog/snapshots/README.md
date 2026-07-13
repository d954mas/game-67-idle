# Source Snapshots

Generated change-detection layer for asset sources.

## Role

Source Snapshots records the tracked files for each asset source so refresh can
compare the current filesystem state with the last indexed state.

It owns file-level added/changed/deleted detection. It does not own manifest
parsing, SQLite query data, preview generation, or browser UI.

Generated snapshots live outside git:

```text
tmp/ai_studio/assets/snapshots/<source-id>.json
```

## Tracked Files

Asset sources track:

- asset files under the source root;
- `pack.json` and `assets.jsonl` manifest files;
- license files.

Preview folders are not tracked here. Preview changes belong to
`../../previews/` and the Asset Viewer `Refresh previews` action.

## Consumers

`../index.mjs` uses this module during `refreshAssetIndex()`:

1. build the current source snapshot;
2. compare its compact signature with the indexed signature;
3. skip rebuild when unchanged;
4. rebuild and store the new snapshot when changed.

The first implemented slice still performs a full index rebuild after a changed
snapshot. The important behavior is that refresh now has a durable file diff and
tracks pack manifest metadata changes such as `pack.json` and `assets.jsonl`.
