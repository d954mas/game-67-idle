#!/usr/bin/env python3
"""Phase-2 mapper glue for the pack expander (T0330): slice generated sheets
back into axis-named assets.

expand_jobs.py turns a config into sheet-jobs with a cell manifest (which
cell holds which axis values); gen_batch.py/generate_image.py turn each job
into a sheet PNG. Nothing maps the two together -- this script is that
mapper. It reproduces the REAL production chain used by the image-tool
bridges (ai_studio/assets/tools/image/regions/api.mjs,
ai_studio/assets/tools/image/slice/api.mjs), just driven from Python instead
of the Node bridge:

  bg_fix/normalize_background.py (auto border-key estimate + exact-key clamp)
    -> regions/detect_regions.py (chroma connected-components, row-major)
    -> slice/slice_regions.py    (crop + per-region key_matte alpha)

Those three tools need numpy/scipy/Pillow, which live in the Studio venv.
Invoke this script through ai_studio/dev_environment/python_run.mjs; child
tools reuse that owner-selected interpreter through sys.executable.

HARD GATE (the reason this script exists): detect_regions's region_count
MUST equal len(job["cells"]). A merged/split/empty region would otherwise
silently shift the row-major zip and hand an asset the WRONG axes -- the
worst failure mode (quiet mislabeling, not a crash). On mismatch the sheet is
rejected whole and printed loudly; nothing is sliced for it.

Paths inside jobs.json (job["out"], job["cells"]) are the ones expand_jobs.py
built: repo-root-relative. This script resolves them against the repo root
regardless of its own CWD. --outdir, like a normal CLI path argument, is
resolved against the current working directory.

Usage:
  node ai_studio/dev_environment/python_run.mjs .codex/skills/nt-asset-image-generation/scripts/slice_pack.py --jobs tmp/packs/g67-gen-icons/jobs.json
  node ai_studio/dev_environment/python_run.mjs .codex/skills/nt-asset-image-generation/scripts/slice_pack.py --jobs jobs.json --only grade-rusty__material-copper
  node ai_studio/dev_environment/python_run.mjs .codex/skills/nt-asset-image-generation/scripts/slice_pack.py --jobs jobs.json --outdir tmp/packs/g67-gen-icons/cut
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from expand_jobs import slugify

REPO_ROOT = Path(__file__).resolve().parents[4]
TOOLS_ROOT = REPO_ROOT / "ai_studio" / "assets" / "tools" / "image"
NORMALIZE_SCRIPT = TOOLS_ROOT / "bg_fix" / "normalize_background.py"
DETECT_SCRIPT = TOOLS_ROOT / "regions" / "detect_regions.py"
SLICE_SCRIPT = TOOLS_ROOT / "slice" / "slice_regions.py"

# Mirrors the defaults ai_studio/assets/tools/image/regions/api.mjs passes to
# the same two tools in the production (Node bridge) path.
DEFAULT_NORMALIZE_TOLERANCE = 32
DEFAULT_MIN_AREA = 256
DEFAULT_PADDING = 8
DEFAULT_MERGE_DISTANCE = 0
DEFAULT_ROW_TOLERANCE = 32


def resolve_repo_path(repo_root: Path, value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else (repo_root / value).resolve()


def run_tool(python: Path, script: Path, args: list[str]) -> subprocess.CompletedProcess:
    cmd = [str(python), str(script), *args]
    return subprocess.run(cmd, capture_output=True, text=True)


def _fail_detail(label: str, result: subprocess.CompletedProcess, cmd: list[str]) -> str:
    tail = (result.stdout + result.stderr).strip()
    return f"{label} exit {result.returncode}: {tail[-500:]}\n  cmd: {' '.join(cmd)}"


@dataclass
class SheetResult:
    sheet: str
    status: str  # OK | REJECT | ERROR | MISSING
    cells_cut: int = 0
    paths: list[str] = field(default_factory=list)
    detail: str = ""


def process_job(python: Path, job: dict, *, outdir_override: Path | None,
                 key_tolerance: int, min_area: int, padding: int,
                 merge_distance: int, row_tolerance: int) -> SheetResult:
    out = job.get("out")
    cells = job.get("cells") or []
    if not out:
        return SheetResult(sheet=str(job.get("name") or "?"), status="ERROR", detail="job has no 'out'")

    sheet_path = resolve_repo_path(REPO_ROOT, out)
    sheet_name = sheet_path.name
    if not sheet_path.exists():
        return SheetResult(sheet=sheet_name, status="MISSING", detail=f"sheet not generated yet: {sheet_path}")
    if not cells:
        return SheetResult(sheet=sheet_name, status="ERROR", detail="job has no 'cells' manifest")

    outdir = outdir_override or (sheet_path.parent / "cut")
    outdir.mkdir(parents=True, exist_ok=True)
    stem = sheet_path.stem

    normalized_png = outdir / f"{stem}.normalized.png"
    normalize_report = outdir / f"{stem}.normalize_report.json"
    normalize_cmd = [
        "--source", str(sheet_path), "--output", str(normalized_png), "--mode", "auto",
        "--key-tolerance", str(key_tolerance), "--json-output", str(normalize_report),
    ]
    result = run_tool(python, NORMALIZE_SCRIPT, normalize_cmd)
    if result.returncode != 0:
        return SheetResult(sheet=sheet_name, status="ERROR",
                            detail=_fail_detail("normalize_background", result, [str(NORMALIZE_SCRIPT), *normalize_cmd]))
    key_color = json.loads(normalize_report.read_text(encoding="utf-8"))["key_color"]

    regions_json = outdir / f"{stem}.regions.json"
    overlay_png = outdir / f"{stem}.overlay.png"
    detect_cmd = [
        "--source", str(normalized_png), "--key-color", key_color, "--key-tolerance", "0",
        "--min-area", str(min_area), "--padding", str(padding), "--merge-distance", str(merge_distance),
        "--row-tolerance", str(row_tolerance), "--json-output", str(regions_json), "--overlay-output", str(overlay_png),
    ]
    result = run_tool(python, DETECT_SCRIPT, detect_cmd)
    if result.returncode != 0:
        return SheetResult(sheet=sheet_name, status="ERROR",
                            detail=_fail_detail("detect_regions", result, [str(DETECT_SCRIPT), *detect_cmd]))
    detected = json.loads(regions_json.read_text(encoding="utf-8"))
    region_count = detected["region_count"]

    if region_count != len(cells):
        print(f"REJECT {sheet_name}: expected {len(cells)} cell(s), found {region_count} region(s) "
              f"(count mismatch would silently mis-map axes -- skipping slice)")
        return SheetResult(sheet=sheet_name, status="REJECT",
                            detail=f"expected {len(cells)} cell(s), found {region_count} region(s)")

    # detect_regions already sorts row-major (sort_row_major); expand_jobs also
    # lists cells row-major -> a plain zip is the deterministic cell<->region mapping.
    reviewed_regions = []
    for region, cell in zip(detected["regions"], cells):
        name = "__".join(f"{axis}-{slugify(value)}" for axis, value in cell["axes"].items())
        reviewed_regions.append({"id": region["id"], "rect": region["rect"], "name": name})
    reviewed = {
        "schema": "ai_studio.raster2d.region_review.v1",
        "source": str(normalized_png),
        "region_count": len(reviewed_regions),
        "regions": reviewed_regions,
    }
    reviewed_json = outdir / f"{stem}.reviewed.json"
    reviewed_json.write_text(json.dumps(reviewed, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")

    prefix = slugify(job.get("pack") or stem)
    manifest_path = outdir / f"{stem}.manifest.json"
    # job["pack"] is the same for every sheet/candidate in a pack, and candidates of one
    # sheet share identical cell axes -> identical region names; --prefix alone does not
    # disambiguate them. Namespace the actual sliced PNGs per sheet stem (stem already
    # carries __cN for candidates) so candidate c2 cannot silently overwrite c1's cuts.
    slice_outdir = outdir / "slices" / stem
    slice_outdir.mkdir(parents=True, exist_ok=True)
    slice_cmd = [
        "--source", str(normalized_png), "--regions", str(reviewed_json), "--output-dir", str(slice_outdir),
        "--prefix", prefix, "--key-color", key_color, "--manifest-output", str(manifest_path),
    ]
    result = run_tool(python, SLICE_SCRIPT, slice_cmd)
    if result.returncode != 0:
        return SheetResult(sheet=sheet_name, status="ERROR",
                            detail=_fail_detail("slice_regions", result, [str(SLICE_SCRIPT), *slice_cmd]))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    paths = [item["path"] for item in manifest["slices"]]
    return SheetResult(sheet=sheet_name, status="OK", cells_cut=len(paths), paths=paths)


def main() -> int:
    ap = argparse.ArgumentParser(description="Slice generated pack sheets into axis-named assets (phase 2 of expand_jobs).")
    ap.add_argument("--jobs", required=True, help="jobs.json written by expand_jobs.py")
    ap.add_argument("--only", help="process only the sheet whose out basename (with or without extension) matches this")
    ap.add_argument("--outdir", help="shared output dir for all sheets (default: <sheet_dir>/cut per sheet)")
    ap.add_argument("--key-tolerance", type=int, default=DEFAULT_NORMALIZE_TOLERANCE)
    ap.add_argument("--min-area", type=int, default=DEFAULT_MIN_AREA)
    ap.add_argument("--padding", type=int, default=DEFAULT_PADDING)
    ap.add_argument("--merge-distance", type=int, default=DEFAULT_MERGE_DISTANCE)
    ap.add_argument("--row-tolerance", type=int, default=DEFAULT_ROW_TOLERANCE)
    a = ap.parse_args()

    jobs_path = Path(a.jobs)
    try:
        data = json.loads(jobs_path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise SystemExit(f"slice_pack: cannot read --jobs {jobs_path}: {exc}")
    jobs = data.get("jobs", []) if isinstance(data, dict) else data
    if not jobs:
        print("slice_pack: no jobs")
        return 0

    if a.only:
        def matches(job: dict) -> bool:
            out = Path(job.get("out") or "")
            return a.only in (out.name, out.stem)
        jobs = [job for job in jobs if matches(job)]
        if not jobs:
            raise SystemExit(f"slice_pack: --only '{a.only}' matched no job in {jobs_path}")

    python = Path(sys.executable).resolve()
    outdir_override = Path(a.outdir).resolve() if a.outdir else None

    results = [
        process_job(
            python, job, outdir_override=outdir_override,
            key_tolerance=a.key_tolerance, min_area=a.min_area, padding=a.padding,
            merge_distance=a.merge_distance, row_tolerance=a.row_tolerance,
        )
        for job in jobs
    ]

    print(f"\nslice_pack: {len(results)} sheet(s)")
    print(f"{'sheet':<40} {'status':<8} {'cells':<6} detail")
    for row in results:
        first_path = row.paths[0] if row.paths else row.detail
        print(f"{row.sheet:<40} {row.status:<8} {row.cells_cut:<6} {first_path}")
        for extra in row.paths[1:]:
            print(f"{'':<40} {'':<8} {'':<6} {extra}")

    counts: dict[str, int] = {}
    for row in results:
        counts[row.status] = counts.get(row.status, 0) + 1
    print(f"done: {counts}")
    # Zero OK sheets (e.g. every sheet still MISSING) means nothing actually
    # succeeded -- exit 0 there would mask a broken pipe in automation.
    return 1 if (counts.get("REJECT") or counts.get("ERROR") or not counts.get("OK")) else 0


if __name__ == "__main__":
    raise SystemExit(main())
