# Pack Builder Rules

Load this reference when editing pack/material builders, generated headers,
atlas packs, or asset audit/report tools.

## Builder Discipline

- Read the existing pack/build script before adding new asset logic.
- Do not assume the pack/material path is too slow. If the engine or project has
  pack builders and caches, inspect the builder and run or measure the smallest
  pack build before choosing a direct PNG/runtime shortcut.
- Add the smallest asset path that proves runtime integration.
- Regenerate packs with the project task or preset.
- Verify both generated files and runtime loading behavior when possible.

## Pack Builder Changes

When editing a pack builder:

- Add resource ids with stable names.
- Keep cache directories and output paths project-relative.
- Fail loudly on missing required source assets.
- Write generated PNG/JSON/Markdown outputs atomically through a temp file in
  the same directory followed by replace/rename. Audits, previews, or users may
  read files while a build is running; they must see either the old complete
  file or the new complete file, never a truncated image or partial manifest.
- Reuse `tools/assets/atomic_io.py` for Python asset tools instead of copying
  local temp-file helpers into each script.
- Apply the same atomic write rule to audit/report tools, not only pack
  builders.
- Print enough output for a user or agent to know what was generated.

## Review Atlases

Final review atlas audits must reject hidden RGB under alpha 0 in the clean
atlas; this catches key-color ghosts that are invisible in PNG viewers but can
leak back through filtering or premultiplied conversion.

They must also reject visible pixels outside declared packed `padded_rect`s.
Clean atlas free space is not a place for labels, stains, or untracked art
fragments. Labeled previews must be pixel-identical to clean atlases outside
label rects, without debug outlines over assets. The overlay-only label policy
must be declared in pack JSON, not carried only in prose or chat context.
