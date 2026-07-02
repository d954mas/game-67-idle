#!/usr/bin/env python3
"""Scale + encode canvas elements for export, one PIL spawn per export batch.

This is the canvas module's OWN Python tool (not a raster2d step). ops.exportElements
writes an export spec JSON (a flat list of jobs, each an absolute source path, an
absolute output path, explicit target pixels, format, quality, resample) and spawns
this script ONCE for the whole batch. The scale math lives in the JS op layer
(resolveExportScale), so this tool only resizes to the given pixels and encodes:

  * png  — lossless, RGBA preserved (no quality)
  * jpg  — flattened onto white (JPEG has no alpha), quality applied
  * webp — RGBA preserved, quality applied

resample: "lanczos" (smooth, the clean-art supersampling default) or "nearest"
(pixel art). Byte-identical 1x PNG copies never reach this tool — the JS op copies
those files verbatim so the lead's original pixels are never re-encoded.

Outputs are written atomically; a small JSON report echoes each job's real pixels
and encoded byte size for provenance.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic

RESAMPLE = {
    "lanczos": Image.Resampling.LANCZOS,
    "nearest": Image.Resampling.NEAREST,
}


def encode_job(job: dict[str, Any]) -> dict[str, Any]:
    source = Path(job["src"])
    if not source.exists():
        raise FileNotFoundError(f"export source image missing: {source}")
    out = Path(job["out"])
    target_w = int(job["target_w"])
    target_h = int(job["target_h"])
    fmt = str(job.get("format") or "png").lower()
    resample = RESAMPLE.get(str(job.get("resample") or "lanczos").lower(), Image.Resampling.LANCZOS)

    image = Image.open(source).convert("RGBA")
    if (image.width, image.height) != (target_w, target_h):
        image = image.resize((max(1, target_w), max(1, target_h)), resample)

    if fmt == "png":
        save_image_atomic(image, out, format="PNG")
    elif fmt == "jpg":
        # JPEG has no alpha: flatten onto a solid white matte, then encode at quality.
        flat = Image.new("RGB", image.size, (255, 255, 255))
        flat.paste(image, mask=image.getchannel("A"))
        save_image_atomic(flat, out, format="JPEG", quality=int(job.get("quality") or 90))
    elif fmt == "webp":
        save_image_atomic(image, out, format="WEBP", quality=int(job.get("quality") or 90))
    else:
        raise ValueError(f"unsupported export format: {fmt}")

    size_bytes = out.stat().st_size if out.exists() else 0
    return {
        "out": out.as_posix(),
        "file": out.name,
        "format": fmt,
        "width": image.width,
        "height": image.height,
        "size_bytes": size_bytes,
    }


def run(spec: dict[str, Any]) -> dict[str, Any]:
    results = [encode_job(job) for job in (spec.get("jobs") or [])]
    report = {
        "schema": "ai_studio.canvas.export_images_report.v1",
        "job_count": len(results),
        "jobs": results,
    }
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Scale + encode canvas elements for export.")
    parser.add_argument("--spec", required=True, type=Path, help="export spec JSON (ai_studio.canvas.export_images_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    report = run(spec)
    print(f"pass: encoded {report['job_count']} export job(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
