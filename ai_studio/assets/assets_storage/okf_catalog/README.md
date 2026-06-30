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
node ai_studio/assets/assets_storage/okf_catalog/poly_pizza_audit.mjs --out tmp/ai_studio/poly_pizza_audit.json
```

`tools/assets/source/find_assets.mjs` remains only as a compatibility entrypoint
while legacy asset tools are reviewed.

`poly_pizza_audit.mjs` is a local catalog audit for the legacy Poly Pizza OKF
records. It reports bundle declared-count mismatches, duplicate ids/source
pages, missing source/license fields, and assets that intentionally belong to
more than one pack through secondary membership.
