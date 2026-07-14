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

`auto` routes only between key matte and a dual-plate refusal; it never silently
selects a neural model. CorridorKey is green-native and uses the recorded
hue-180 shim for magenta; other keys refuse. CorridorKey region requests run
one whole-frame matte and composite only the requested regions. ViTMatte and
BiRefNet are whole-element only.

Machine-local model/tool roots belong only in
`ai_studio/studio.config.local.json`. Tool setup failures are not eligible for
fallback to another method.

The cleanup order is alpha/keying, quantization, optional denoise, then export.
