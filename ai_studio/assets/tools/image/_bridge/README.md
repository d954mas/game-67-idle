# image/_bridge — shared image-tool plumbing

Not a tool. `_bridge` owns the security-sensitive and environment-coupled glue
that every per-tool `api.mjs` bridge needs, so it is written once and never
duplicated:

- **Python interpreter** — resolved from `ai_studio/studio.config.json`
  -> `pythonPath` ONLY (`.venv/Scripts/python.exe`). No PATH search, no candidate
  chain. Missing venv or dependency => loud error naming `setup_python.mjs`.
- **Warm Python worker** (T0202) — `runPython` does not cold-spawn a fresh
  interpreter per call. It routes through a long-lived worker (`worker.py`, managed
  by `worker.mjs`) that pays the interpreter-startup + `numpy`/`scipy`/`PIL` import
  floor (~165-278ms) ONCE at boot, then runs each tool script's `__main__` in-process
  (`runpy`) so the second and later detect/slice/render/export calls are near-instant.
  Pure transport: same argv/argparse main, so tool behavior + parity are unchanged.
- **tmp-path confinement** — `safeResolve`, `ensureInsideTmp`, `sessionDirForPath`
  keep tool files under `tmp/ai_studio/assets/<namespace>/` (default namespace
  `raster2d`, kept stable for the frozen viewer's public URLs).
- **session dirs + URL/rel helpers** — `workspaceRel`, `tmpUrl`.
- **HTTP helpers** — `writeJson`, `readJsonBody`.
- **JSON + image-size helpers** — `readJson`, `writeJsonFile`, `readImageSize`,
  `color`, `intOption`, `decodeDataUrl`, `extensionForUpload`, `safeSlug`.

## Files

- `bridge.mjs` — the shared library (import from per-tool bridges). `runPython`
  routes tool spawns through the warm worker.
- `worker.mjs` — the warm-worker manager: lazy spawn, one FIFO-serialized process,
  loud crash → respawn on the next request (no cold-spawn fallback), idle-timeout
  kill, and a process-exit/signal hook so a Python child is never orphaned. Override
  the idle timeout for tests with `AI_STUDIO_IMAGE_WORKER_IDLE_MS`.
- `worker.py` — the long-lived worker process: line-delimited JSON over stdio,
  imports the heavy stack once, runs each requested tool script's `__main__`.
- `setup_python.mjs` — compatibility entry point for the Dev Environment
  root-`.venv` setup, pinned install, and version/import verification.

## Setup / repair the studio venv

```
node ai_studio/assets/tools/image/_bridge/setup_python.mjs
```
