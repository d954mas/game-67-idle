# sheet/ — stage 4: RGBA frames -> flipbook spritesheet

Packs the matte frames into a near-square grid and writes the sheet + meta. Pure
PIL in the repo `.venv` — this stage has **no dependency on the video
experiment**; given any folder of RGBA frames it packs a sheet anywhere.

- **Entry:** `pack_sheet.py` — pure `choose_columns(count)` / `pack(frames,
  columns, trim)` (unit-tested) + `build_sheet(...)` IO wrapper + a CLI:
  `python pack_sheet.py --run-dir <dir>` (derives `matte/` -> `sheet/`, harvests
  provenance from the sibling `generate/` + `matte/` sidecars), or
  `python pack_sheet.py --frames <dir> --out <dir> --name <n> [--fps 16] [--columns N] [--trim]`.
- **Grid:** deterministic near-square, `columns = ceil(sqrt(count))` unless
  `--columns`. Row-major, one uniform frame box per cell.
- **Trim:** OFF by default — a flipbook wants uniform frame boxes. `--trim` crops
  every frame to the shared union alpha bbox (identical box across frames, so
  alignment is preserved).
- **Output:** `<out>/<name>_sheet.png` + `<name>_sheet.json`
  (`schema: ai_studio.video.spritesheet.v1`; `frame_w/h`, `count`, `columns`,
  `rows`, `fps`, `trim`, `layout`, and the full `source` provenance chain).
- **Deps:** PIL (repo `.venv`). **Tests:** `pack_sheet_test.py` (synthetic PIL
  frames, no GPU) — packing math, placement, meta, trim, loud non-uniform/empty:
  `PYTHONPATH=<repo> .venv/Scripts/python.exe -m unittest ai_studio.assets.tools.video.sheet.pack_sheet_test`.
