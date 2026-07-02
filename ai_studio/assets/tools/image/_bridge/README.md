# image/_bridge — shared image-tool plumbing

Not a tool. `_bridge` owns the security-sensitive and environment-coupled glue
that every per-tool `api.mjs` bridge needs, so it is written once and never
duplicated:

- **Python interpreter** — resolved from `ai_studio/studio.config.json`
  -> `pythonPath` ONLY (`.venv/Scripts/python.exe`). No PATH search, no candidate
  chain. Missing venv or dependency => loud error naming `setup_python.mjs`.
- **tmp-path confinement** — `safeResolve`, `ensureInsideTmp`, `sessionDirForPath`
  keep tool files under `tmp/ai_studio/assets/<namespace>/` (default namespace
  `raster2d`, kept stable for the frozen viewer's public URLs).
- **session dirs + URL/rel helpers** — `workspaceRel`, `tmpUrl`.
- **HTTP helpers** — `writeJson`, `readJsonBody`.
- **JSON + image-size helpers** — `readJson`, `writeJsonFile`, `readImageSize`,
  `color`, `intOption`, `decodeDataUrl`, `extensionForUpload`, `safeSlug`.

## Files

- `bridge.mjs` — the shared library (import from per-tool bridges).
- `setup_python.mjs` — one-shot: create `.venv/` from `py -3.12`, install
  `../requirements.txt`, verify `import numpy, scipy, PIL`.

## Setup / repair the studio venv

```
node ai_studio/assets/tools/image/_bridge/setup_python.mjs
```
