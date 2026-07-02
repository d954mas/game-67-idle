# Canvas

Multi-image canvas projects (a Figma/Recraft-like workspace) whose capabilities
are all callable two equal ways: by an agent (CLI or direct import) and by the
thin browser page. The tools are the product; the page is only a local interface.

## Owner and boundary

This module owns canvas project persistence, the shared operation layer, its HTTP
adapter, the agent CLI, and the thin page. It does not own the 2D image pipeline:
`detect_regions` bridges to the existing `../tools/raster2d/` ops unmodified so the
browser and an agent get identical pixels/regions.

## Layout

- `store.mjs` — project persistence. One project is a folder under the configured
  canvas projects root: `project.json` (schema `ai_studio.canvas.project.v1`) plus
  an immutable, content-addressed `files/`. All writes are atomic; all ids and
  file names are path-confined.
- `ops.mjs` — the one operation layer both clients call. Thin wrappers over the
  store plus the bridged `detectRegions`.
- `api.mjs` — HTTP adapter (`createCanvasApi`) mounted by Studio Shell on
  `/api/canvas/`.
- `cli.mjs` — agent client over the same ops.
- `site/` — the thin page (`canvas.html` / `canvas.js` / `canvas.css`), served via
  the existing `/ai_studio/` static route. It reuses the Asset Tools viewport
  module for pan/zoom/fit and holds no logic beyond rendering/input.
- `tests/` — `node:test` suites for the store, ops, API, and studio config.

## Projects root

The on-disk projects root is resolved from studio config
(`ai_studio/studio.config.json`, `canvasProjectsRoot`) via
`../../core_harness/tool_lib/studio_config.mjs`. It is created lazily on first
project create, never at load time. The `CANVAS_PROJECTS_ROOT` env var overrides
config so tests and one-off runs never touch the configured location.

## Operations

Every capability is one op in `ops.mjs`:

- `listProjects` / `createProject` / `getProject` / `updateProject`
- `addImage` (parses real PNG/JPEG/GIF dimensions, writes an immutable file)
- `patchElement` (move/resize/rename) / `removeElement` (element only; file stays)
- `detectRegions` — reads the element image, runs it through the raster2d
  upload + detect pipeline, stores `element.regions`, and records a `tool_runs`
  entry. Requires Python (numpy + Pillow), as the rest of the raster2d pipeline.

## CLI

```powershell
node ai_studio/assets/canvas/cli.mjs list
node ai_studio/assets/canvas/cli.mjs create --title "My canvas"
node ai_studio/assets/canvas/cli.mjs show <id>
node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
```

## Validation

```powershell
node --test ai_studio/assets/canvas/tests/*.test.mjs
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/core_harness/validation/doc_reference_check.mjs
```
