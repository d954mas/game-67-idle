# 2D Asset-Prep Pipeline Map (canvas design input) — 2026-07-02

Scope: the "Asset Tools" site surface (`/asset_prep/`) + the `raster2d` HTTP bridge
+ Python tools + `cutout` matte tools, as they exist in the **current working tree**
(uncommitted alpha/preview work in flight). Goal is to inform a Figma/Recraft-like
canvas page where every operation is callable from both the browser and an AI agent
through one decomposed tool layer.

Key files:
- HTTP bridge / tool layer: `ai_studio/assets/tools/raster2d/api.mjs`
- Server mount: `ai_studio/studio_shell/server.mjs`
- Python steps: `ai_studio/assets/tools/raster2d/{background,regions,slicing}/*.py`
- Matte algorithms: `ai_studio/assets/tools/cutout/{key_matte,chroma_key_alpha,dual_plate_alpha,route_cutout}.py`
- UI: `ai_studio/assets/viewer/asset_tools.html` + `asset_tools.js` + `asset_tools_*.mjs`
- Atomic IO: `ai_studio/assets/tools/lib/atomic_io.py`

---

## 1. Pipeline steps as the site exposes them today

The left "pipeline-rail" in `asset_tools.html:75-125` renders a 7-step wizard. Step
state is driven by `updatePipeline()` in `asset_tools.js:1055-1086` (purely derived
from client state; steps are **status indicators, not gates**).

| # | Step (HTML `data-step`) | What it does | API route |
|---|---|---|---|
| 1 | `source` | Upload/load an image; canvas gets it locally, server stores a copy and returns a session. | `POST /api/asset-tools/raster2d/upload` |
| 2 | `background` | Border-connected chroma background normalized to exact key color (or skipped for whole-image). Runs **inside detect**, not a separate button. | (folded into `/detect`) |
| 3 | `regions` | Connected-component region detection → crop rects; user then edits rects/polygons on canvas. | `POST /api/asset-tools/raster2d/detect` |
| 4 | `review` (Region Review) | Debounced review-sheet build (grid of matte crops). Auto-runs 350ms after any region change. | `POST /api/asset-tools/raster2d/review` |
| 5 | `alpha` | Per-region alpha policy (`key_matte` default / `generation` override). Selected-region preview calls Python slice. | `POST /api/asset-tools/raster2d/preview` (NEW) |
| 6 | `export` | Slice all regions → PNGs + manifest + optional review sheet, zipped. Single-region export also available. | `POST /api/asset-tools/raster2d/export`, `/export-one` |
| 7 | `promote` | Marked `is-disabled` / "later" in the UI — **not wired** to raster2d. Handled out-of-band by `viewer/promote.mjs` CLI. | (none) |

Note: HTML orders Review (4) before Alpha (5), but `alpha` policy actually feeds
both review and export slicing; the numbering is cosmetic. There are also two
non-numbered workspace modes on the stage toolbar (`asset_tools.html:178-208`):
**Regions** and **Slice 9** (nine-slice preview, client-only, never exported).

UI→route call sites in `asset_tools.js`:
- upload: `loadImageFile()` `asset_tools.js:1206-1233` (line 1210)
- detect: `runDetectRegions()` `asset_tools.js:1235-1256` (line 1238)
- review: `scheduleReview()` `asset_tools.js:1117-1138` (line 1128)
- preview: `requestPythonAlphaPreview()` `asset_tools.js:943-984` (line 954)
- export zip: `downloadZip()` `asset_tools.js:1275-1289` (line 1278)
- single export: `downloadSelectedRegion()` `asset_tools.js:1291-1304` (line 1296)

---

## 2. Operation inventory

All routes are POST JSON, handled by `createRaster2dAssetToolsApi()` /
`handleRaster2dAssetToolsApi` (`api.mjs:446-479`), mounted first in the API chain
in `server.mjs:147-152`. `root` is the repo root (`findRoot`). Python is invoked
via `runPython()` (`api.mjs:110-131`) trying candidates from `pythonCandidates()`
(`api.mjs:86-108`): `AI_STUDIO_PYTHON`/`PYTHON` env → bundled codex python →
`py -3.12` → `C:\Python312|314` → `python`.

### 2.1 Upload / new session — `/upload`
- Entry: `uploadRaster2dSource()` `api.mjs:210-226`. Pure JS (no Python).
- Inputs: `{ fileName, dataUrl }` (base64 data URL, ≤ 64 MB `maxBodyBytes` `api.mjs:6`). Accepts png/jpg/jpeg/webp/gif (`extensionForUpload` `api.mjs:201-208`).
- Session id: `<ISO-timestamp>-<uuid8>` (`api.mjs:213`). Slug via `safeSlug` `api.mjs:8-15`.
- Outputs: writes `tmp/ai_studio/assets/raster2d/<session>/sources/<slug><ext>`. Returns `{ sessionId, sourcePath (repo-rel), sourceUrl (/tmp/...), fileName }`.

### 2.2 Detect (normalize background + region detect) — `/detect`
- Entry: `detectRaster2dRegions()` `api.mjs:228-345`.
- Inputs: `{ sourcePath, options }`. Options w/ defaults (`api.mjs:237-245`): `backgroundMode` `"auto"|"whole_image"|"none"|"no_background"`; `keyColor` (optional `#rrggbb`, else auto-estimated); `keyTolerance` 32 (max 255); `minArea` 256; `padding` 8; `mergeDistance` 0; `rowTolerance` 32.
- Two branches:
  - **whole_image** (`api.mjs:251-305`): copies source→`normalized.png`, reads dimensions from header bytes (`readImageSize` `api.mjs:141-169`, PNG/GIF/JPEG only), writes a synthetic `normalize_report.json` (mode `passthrough_no_background`, key `#ff00ff`) and a single full-image region. No Python.
  - **auto** (`api.mjs:307-344`): runs `normalize_background.py` (mode `auto`) → `background/normalized.png` + `background/normalize_report.json`; reads back `key_color`; then runs `detect_regions.py` on the **normalized** image with `--key-tolerance 0` (exact match to the normalized key) → `regions/regions.json` + `regions/overlay.png`.
- Path safety: `ensureInsideTmp()` forces everything under `tmp/ai_studio/assets/raster2d` (`api.mjs:28-37`).
- Outputs returned: `{ sessionId, sourcePath, normalizedPath, normalizedUrl, normalizeReportPath, regionsPath, overlayUrl, normalizeReport, regions }`.

### 2.3 Review sheet — `/review`
- Entry: `reviewRaster2dRegions()` → `runSlice(mode:"review")` `api.mjs:358-421`.
- Inputs: `{ imagePath|normalizedPath|sourcePath, regions[], prefix }`.
- Writes `<session>/review/regions.reviewed.json` (schema `region_review.v1`, `reviewedRegionsPayload` `api.mjs:347-356`) then runs `slice_regions.py` with `--review-sheet`. Key color taken from the session's `normalize_report.json` via `sessionKeyColor()` `api.mjs:176-184`.
- Outputs: `{ sessionId, reviewedRegionsPath, manifestPath, manifest, reviewSheetPath/Url, zipPath:null, zipUrl:null }`.

### 2.4 Selected-region preview — `/preview` (IN-FLIGHT, uncommitted)
- Entry: `previewRaster2dRegion()` `api.mjs:427-432` → `runSlice(mode:"preview")` (new out-dir branch added at `api.mjs:362`).
- Inputs: `{ imagePath, prefix, region }` (single region; wraps to `regions:[region]`, `includeReviewSheet:false`).
- Writes `<session>/preview/slices/<prefix>_<name>.png` + manifest; `directSliceResult()` (`api.mjs:405-417`) flattens the first slice into `{ slice, fileName, slicePath, sliceUrl }`.
- Purpose: give the inspector a **pixel-exact** Python matte preview matching export (previously JS-only approximation). Registered route `api.mjs:465-468`.

### 2.5 Export all — `/export`
- Entry: `exportRaster2dRegions()` → `runSlice(mode:"export")` `api.mjs:423-425`.
- Inputs: `{ imagePath, regions[], prefix, includeReviewSheet }`.
- Writes to `<session>/export/`: `regions.reviewed.json`, `slices/*.png`, `manifest.json`, optional `review_sheet.png`, and `<prefix>_regions.zip` (STORED zip via `write_zip` `slice_regions.py:280-295`).
- Outputs add `{ zipPath, zipUrl }`; UI navigates to `zipUrl` to download.

### 2.6 Export one — `/export-one`
- Entry: `exportRaster2dRegion()` `api.mjs:434-439` → `runSlice(mode:"single")` → `directSliceResult()`. Writes `<session>/single/slices/<prefix>_<name>.png`.

### 2.7 Static tmp serving
- `resolveRaster2dTmpPath()` `api.mjs:441-444`, wired in `server.mjs:112-114`: `/tmp/...` served read-only from repo `tmp/`. This is how `sourceUrl`/`normalizedUrl`/`overlayUrl`/`reviewSheetUrl`/`sliceUrl`/`zipUrl` resolve.

### Session directory layout (all under `tmp/ai_studio/assets/raster2d/<session>/`)
```
sources/<name>.<ext>              # upload
background/normalized.png         # detect
background/normalize_report.json  # detect  (schema background_normalize.v1)
regions/regions.json              # detect  (schema detected_regions.v1)
regions/overlay.png               # detect (auto only)
review/regions.reviewed.json + manifest.json + review_sheet.png
preview/slices/*.png + manifest.json         # NEW
single/slices/*.png + manifest.json
export/slices/*.png + manifest.json + review_sheet.png + <prefix>_regions.zip
```
Each mode gets its own out-dir (`runSlice` `api.mjs:362`), so review/preview/export
do not share slice outputs.

### JSON schemas (write points)
- `ai_studio.raster2d.background_normalize.v1` — `normalize_background.py:108-118`, `api.mjs:254-265`. Fields: mode, key_color, key_tolerance, alpha_threshold, image{w,h}, background_pixels, changed_pixels, source, output.
- `ai_studio.raster2d.detected_regions.v1` — `detect_regions.py:275-295`. Fields: source, image, detection{mode,key_color,key_tolerance,alpha_threshold,min_area,merge_distance,padding,row_tolerance}, region_count, regions[]. (Note: the whole-image JS branch writes a slightly different `options{}` block instead of `detection{}`, `api.mjs:272-282` — schema drift.)
- Region object: `{ id "region_NNN", rect [x,y,w,h], content_bbox [x,y,w,h], area_px, name?, alpha{mode,...}, polygon? [[x,y]...], merged_from?, source? }`.
- `ai_studio.raster2d.region_review.v1` — reviewed payload `api.mjs:347-356`.
- `ai_studio.raster2d.slices.v1` (manifest) — `slice_regions.py:342-365`. Per-slice: `{ id, name?, file, path, rect, polygon?, alpha{mode,status,key_color,warning?,reason?}, width, height }`, plus review_sheet, zip_output.

### Key-color derivation (three places; watch for divergence)
- `normalize_background.estimate_border_key_color` `normalize_background.py:78-87` — numpy, most-common opaque border RGB (max-abs-channel tolerance).
- `slice_regions.estimate_border_key_color` `slice_regions.py:57-71` — PIL getdata fallback if no `--key-color` passed.
- `route_cutout._key_from_border` `route_cutout.py:67-70` — border **median**.
- JS `parseKeyColor` `asset_tools_alpha_preview.mjs:1-10` — parses only; state key color comes from the normalize report. Default everywhere is magenta `#ff00ff`.

---

## 3. Algorithm notes

### Background normalize — `normalize_background.py`
- `normalize_background_numpy` `:90-118`. key-like = `alpha<=threshold` OR max-abs-channel distance to key `<= key_tolerance`. `border_connected` (scipy `label`, iterative numpy fallback `:43-76`) keeps only the flood region touching the image border, then paints it to exact key RGBA. **Interior key-colored art is preserved** (only border-connected background changes).
- Params: key_tolerance (32), alpha_threshold (0). Modes `auto`/`none`.
- Limitation: relies on a contiguous border-touching background; islands of background fully enclosed by art are not normalized. Estimator picks the single most frequent border color — a multi-tone/gradient background degrades.

### Region detection — `detect_regions.py`
- `foreground_mask` `:47-59` = inverse of key-like. `connected_components` `:62-149` is a **custom union-find run-length** labeler (4-connectivity via row-run overlap; no scipy needed). Then `merge_close_components` `:172-211` (greedy nearest-pair merge within `merge_distance`, Chebyshev/Euclidean gap `rect_gap` `:152-159`), `sort_row_major` `:214-234` (row bucketing by `row_tolerance`), `expand_rect` padding `:237-244`.
- Called from `/detect` with `--key-tolerance 0` because it runs on the already-normalized image (exact key).
- Limitation: 4-connected only (diagonally-touching sprites split); merge is O(n²) greedy; no per-region alpha analysis (does not decide key_matte vs dual_plate).

### Key matte cutout — `cutout/key_matte.py` (path 1, the default export matte)
- `key_matte_cutout` `:74-166`. Trimap from key-distance: sure-bg `<= exact_tolerance` (12), sure-fg `>= foreground_tolerance` (80), smoothstep alpha across the band `:109-113`. Downscales work copy to `max_dim` 512 then composites at original res, keeping crisp original RGB where alpha>0.8 `:143`. `_limit_despill` `:42-71` is a Vlahos-style per-key-family limit (green/magenta/red/blue/cyan) to kill key halos without blur. Then conditional `decontaminate_source_key_spill_image`, `bleed_transparent_rgb(passes=4)`, `repair_transparent_edge_rgb`, `zero_fully_transparent_rgb`.
- Invoked by `slice_regions.apply_alpha` `:217-242` (imports `key_matte_cutout`); on any exception falls back to `simple_key_matte_cutout` `:204-214` (pure-PIL per-pixel exact key removal) and marks `status:"applied_fallback"` + `warning`.
- Limitation: per-crop only (not full sheet); tuned for flat single key; soft fractional alpha is explicitly out of scope (routed to dual_plate).

### chroma_key_alpha.py (shared spill/hygiene library) — **uncommitted WIP**
- Big bank of heuristic spill masks (`is_*_spill_like`, vectorized `*_mask_rgb`, `source_key_spill_mask` keyed per key family `:119-140`), integral-image box-sum neighbor repaint (`decontaminate_source_key_spill_numpy` `:187-221`), `bleed_transparent_rgb` `:241-270`, `repair_transparent_edge_rgb` `:281-299`, `resize_rgba_premultiplied` `:302-319`.
- **In-flight change (git diff):** `from scipy import ndimage` → optional import with numpy fallback in `dilate_bool_mask` (`:156-172`). This makes the whole cutout stack run in environments without scipy (e.g. the studio-server's bundled python). Test `test_dilate_bool_mask_has_numpy_fallback_without_scipy` added.
- Limitation: many hardcoded magic-number thresholds per key color; magenta/green/red/blue/cyan special-cased; unusual keys fall back to plain distance.

### dual_plate_alpha.py (path 2, NOT wired to the site)
- `extract_dual_plate_alpha` `:45-70+`: needs light+black plate pair; Smith&Blinn joint-channel projection (`proj`) for true fractional alpha. This is the intended target of `alpha.mode:"generation"` regions but there is **no runtime path** from the site to it yet.

### route_cutout.py (path chooser, NOT wired to the site)
- Docstring `:1-26`: decides key_matte vs dual_plate from a single flat-key crop via `soft_score` (partial-keyness mass) and `depth90` (transition depth). Thresholds `SCORE_THRESHOLD=0.11`, `DEPTH90_THRESHOLD=14`. Currently CLI/agent-only; the site's alpha mode is a manual dropdown, not auto-routed.

### JS matte preview — `asset_tools_alpha_preview.mjs`
- `applyAlphaPreviewMatte` `:139-181`: client-side approximation — connected border flood (`buildConnectedBackgroundMask` `:48-80`), exact-key zeroing, magenta-only despill + band alpha, polygon mask. `applyGenerationAlphaDiagnostic` `:197-226` tints edge pixels for the "generation" diagnostic view. This is a **second, independent matte implementation** in JS that only approximates the Python result — the WIP `/preview` route exists specifically to replace it with the real Python matte for `key_matte`.

---

## 4. Canvas-readiness assessment

### Already pure / agent-callable (good tool-layer primitives)
The Python scripts are all argparse CLIs writing atomic files, and the `api.mjs`
exports are plain `async (root, body) => result`:
- `uploadRaster2dSource`, `detectRaster2dRegions`, `reviewRaster2dRegions`,
  `previewRaster2dRegion`, `exportRaster2dRegions`, `exportRaster2dRegion` — all
  exported and independently callable (already imported directly by
  `raster2d/api.test.mjs`). An agent can drive the whole pipeline via HTTP or by
  importing these functions.
- Python: `normalize_background.py`, `detect_regions.py`, `slice_regions.py`,
  `key_matte.py`, `dual_plate_alpha.py`, `route_cutout.py` are all standalone CLIs.
- Pure client geometry libs with unit tests (fully reusable in a canvas page,
  framework-agnostic, no DOM): `asset_tools_viewport.mjs` (fit/zoom/pan, rect
  clamp/resize/handles/hit-test), `asset_tools_history.mjs` (undo/redo stack),
  `asset_tools_slice9.mjs` (nine-slice draw math), `asset_tools_region_label.mjs`
  (label fitting), `asset_tools_stage_view.mjs` (view-mode resolver),
  `asset_tools_alpha_preview.mjs` (canvas ImageData matte, though it duplicates
  Python — see risks).

### Coupled to the step-wizard UI (needs decomposition for a canvas)
- `asset_tools.js` is a single 1849-line module with one global `state` object
  (`:30-55`) and a monolithic `renderAll()` (`:1176-1190`). All routing calls,
  DOM ids, and the pipeline-step status live here. There is no separation between
  "operation" and "presentation."
- Session model is **stateless per operation**: every call re-derives the session
  from a tmp file path (`sessionDirForPath` `api.mjs:39-45`). There is no project
  object, no server-side session registry, no list/reopen. The client holds the
  only cross-step state (regions array, key color, selectedId) in memory; a reload
  loses everything not on disk.
- Alpha "generation" mode is a dead-end placeholder: it only writes
  `status:"needs_generation"` in the manifest (`slice_regions.py:219-225`) and
  shows a JS diagnostic tint. No dual_plate/route_cutout execution is wired.
- Region edits are pushed to `/review` on a 350 ms debounce (`scheduleReview`),
  re-slicing **all** regions every change — fine for a wizard, wasteful for a live
  canvas with many regions.

### What a persistent "canvas project" would need that sessions don't have
1. **Persistence beyond tmp**: sessions live under `tmp/ai_studio/assets/raster2d/`
   with timestamp+uuid ids and no manifest/registry; nothing lists or reopens them,
   and `tmp` is disposable. A canvas needs a durable project record (id, name,
   source refs, regions, layers, alpha decisions, history) + a list/open API.
2. **Multiple images / layers**: the whole stack assumes one source image per
   session (`state.sourcePath`/`imagePath`, single `sources/` file). No concept of
   layers, multiple plates (needed for dual_plate), or reference/overlay images.
3. **Masks as first-class**: masks exist only as per-region `polygon` arrays baked
   into the slice; there is no editable mask channel, brush, or per-region matte
   parameter store. Key color/tolerance are global-per-session, not per-region.
4. **Per-region candidates / history**: `asset_tools_history.mjs` is a single
   in-memory regions-array undo stack (depth 80, `:1`), not persisted and not
   per-region. No notion of alternate matte candidates to compare/accept.
5. **Auto-routing + real generation**: to make "each op agent-callable" meaningful,
   `route_cutout` (auto key_matte/dual_plate) and `dual_plate_alpha` need HTTP
   routes; today they are Python-CLI islands.
6. **One matte implementation**: the JS `applyAlphaPreviewMatte` should be dropped
   in favor of the Python `/preview` result (the WIP direction) so browser and
   agent see identical pixels.

### Reusable-as-is JS modules for a canvas page (and gaps)
- `asset_tools_viewport.mjs` — reusable; lacks rotation, multi-select, snapping.
- `asset_tools_history.mjs` — reusable pattern; generic over snapshot shape but
  currently only stores `{regions, selectedId}`; not persisted; not branchable.
- `asset_tools_stage_view.mjs` — trivial, tied to review/edit/slice9 modes only.
- `asset_tools_region_label.mjs` — reusable text-fit helper.
- `asset_tools_slice9.mjs` — reusable nine-slice math; note nine-slice is
  **preview-only** today (no export route).
- `asset_tools_alpha_preview.mjs` — reusable ImageData ops but semantically a
  duplicate of Python matte; keep only as an instant-feedback layer, not truth.

---

## 5. Risks / unknowns

- **Uncommitted work in flight** (`git status` working tree; the task's opening
  snapshot was stale — the `asset_tools_*.mjs` are now committed, these are the
  live diffs):
  - `raster2d/api.mjs` + `api.test.mjs`: adds the `/preview` route +
    `previewRaster2dRegion` + a `preview` out-dir; broadens `pythonCandidates`
    (bundled codex python, `py -3.12`). Direction: real Python matte preview.
  - `cutout/chroma_key_alpha.py` + test: scipy made optional with a numpy
    `dilate_bool_mask` fallback — enables running the matte in a scipy-less env.
  - `raster2d/alpha/README.md`: doc now says key_matte preview goes through the
    Python slicing path (matches export).
  - `viewer/asset_tools.html` + `asset_tools.js`: the `selectedAlphaPreview` state
    machine (`asset_tools.js:943-1046`) wiring the inspector to `/preview`.
  - `viewer/README.md`: modified (doc).
- **`alpha/` module is a README only** — no Python yet (`raster2d/alpha/` contains
  just `README.md`). The "alpha step" is implemented inside `slice_regions.apply_alpha`,
  not as its own tool. Standalone alpha ops named in the README (flatten, threshold,
  transparent-RGB hygiene) do not exist yet.
- **Duplicate matte implementations**: JS `applyAlphaPreviewMatte`
  (`asset_tools_alpha_preview.mjs`) vs Python `key_matte_cutout` vs the pure-PIL
  `simple_key_matte_cutout` fallback (`slice_regions.py:204-214`). Three code paths,
  drift risk; the WIP is converging browser onto Python but the JS path remains.
- **Schema drift**: the whole-image JS branch emits `detected_regions.v1` with an
  `options{}` block (`api.mjs:272-282`) while Python emits a `detection{}` block
  (`detect_regions.py:283-292`); both claim the same schema string.
- **`route_cutout` / `dual_plate_alpha` are unreachable from the site** — the
  "generation" alpha mode never triggers them; it only tags the manifest. Auto path
  selection (soft_score/depth90, threshold 0.11) exists but is unused by the UI.
- **Hardcoded paths / env**: bundled python path
  `%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe`
  and `C:\Python312|314` (`api.mjs:96-105`); server binds `127.0.0.1` only
  (`server.mjs:158`); tmp root is fixed at `tmp/ai_studio/assets/raster2d`
  (`api.mjs:24-26`). Windows-first (matches Avast/Windows env).
- **Whole-image dimension read** supports only PNG/GIF/JPEG headers
  (`readImageSize` throws on webp, `api.mjs:168`), even though upload accepts webp.
- **No server-side session GC / registry**: sessions accumulate in `tmp` forever;
  no listing, no reopen, no cleanup. A canvas project store would replace this.
- **Promote is fully decoupled**: `viewer/promote.mjs` is a separate CLI operating
  on **asset-review manifests** (`review-manifest.json` → library pack.json/
  assets.jsonl + license .md + intake-log, integrity sha256), not on raster2d
  export manifests. There is no code path from a raster2d `slices.v1` export/zip
  into promote; the "Promote" step is disabled in the UI. Bridging export→promote
  (map slice manifest → review manifest entries with license/provenance/origin)
  is unbuilt and would be required for a canvas "export to library" action.
