#!/usr/bin/env python3
"""Crop a canvas element's stored regions into per-region PNGs, verbatim.

This is the canvas module's OWN Python tool (not a raster2d step). ops.sliceRegions
writes a crop spec JSON (absolute source path + the element's stored regions with
their exact rects) and spawns this script directly. Each region is cropped from the
element's own pixels by its STORED rect — no re-detection — so user-moved, resized,
and hand-drawn regions all crop exactly where they sit. One PIL spawn, atomic writes.

Per-region spec entries are objects ({id, rect}); a future polygon shape
({"shape": {"type": "polygon", "points": [...]}}) slots in additively without
changing the spec contract.
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


def crop_rect(image: Image.Image, rect: list[Any]) -> Image.Image:
    """Crop the axis-aligned rect [x, y, w, h], clamped to the image bounds."""
    x, y, w, h = (int(round(float(value))) for value in rect)
    left = max(0, min(x, image.width))
    top = max(0, min(y, image.height))
    right = max(left, min(x + w, image.width))
    bottom = max(top, min(y + h, image.height))
    return image.crop((left, top, right, bottom))


def run(spec: dict[str, Any]) -> dict[str, Any]:
    source = Path(spec["source"])
    if not source.exists():
        raise FileNotFoundError(f"source image missing: {source}")
    image = Image.open(source).convert("RGBA")
    out_dir = Path(spec["output_dir"])
    out_dir.mkdir(parents=True, exist_ok=True)

    crops: list[dict[str, Any]] = []
    for index, region in enumerate(spec.get("regions") or [], start=1):
        rect = region.get("rect")
        if not (isinstance(rect, list) and len(rect) == 4):
            raise ValueError(f"region {region.get('id') or index} is missing a rect")
        cropped = crop_rect(image, rect)
        file_name = f"{index:03d}.png"
        save_image_atomic(cropped, out_dir / file_name)
        crops.append(
            {
                "id": str(region.get("id") or f"region_{index:03d}"),
                "file": file_name,
                "width": cropped.width,
                "height": cropped.height,
            }
        )

    report = {
        "schema": "ai_studio.canvas.crop_regions_report.v1",
        "source": source.as_posix(),
        "crop_count": len(crops),
        "crops": crops,
    }
    report_path = spec.get("report")
    if report_path:
        write_json_atomic(Path(report_path), report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Crop a canvas element's stored regions into per-region PNGs.")
    parser.add_argument("--spec", required=True, type=Path, help="crop spec JSON (ai_studio.canvas.crop_regions_spec.v1)")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text(encoding="utf-8"))
    report = run(spec)
    print(f"pass: cropped {report['crop_count']} region(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
