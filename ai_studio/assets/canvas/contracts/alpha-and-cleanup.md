# Alpha and cleanup operations

Use the stable `../ops.mjs` facade or Canvas CLI; pixel tools never overwrite a
source file. `alphaCutout`, `alphaDualPlate`, `alphaDualPlateGenerate`, cleanup
preview/apply, and filter bake mint content-addressed output and journal the
metadata change. Preview is view-only and writes no journal entry.

Choose alpha methods explicitly when the background contract is known:
key matte for flat keyed art, CorridorKey for supported green/magenta soft
edges, ViTMatte for thin detail, BiRefNet for arbitrary backgrounds, and dual
plate only for an aligned light/dark pair. Region-scoped support differs by
method and unsupported combinations refuse loudly before tool invocation.

Machine-local model/tool roots belong only in
`ai_studio/studio.config.local.json`. Tool setup failures are not eligible for
fallback to another method.
