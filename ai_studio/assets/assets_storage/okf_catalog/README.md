# OKF Catalog

Markdown/OKF catalog access for the shared local asset library.

## Role

OKF Catalog reads `catalog/**/*.md` asset records, joins pack metadata, exposes
normalized asset records, and provides the source-first search command.

The catalog describes reusable source assets. Binary files and derived previews
live in the asset storage folders referenced by catalog records.

## Command

```powershell
node ai_studio/assets/assets_storage/okf_catalog/find_assets.mjs --query sofa --kind model
```

`tools/assets/source/find_assets.mjs` remains only as a compatibility entrypoint
while legacy asset tools are reviewed.
