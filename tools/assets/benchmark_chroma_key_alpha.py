#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys
from time import perf_counter
from typing import Any

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.assets.atomic_io import write_json_atomic
from tools.assets.chroma_key_alpha import key_to_alpha


DEFAULT_SOURCES = [
    Path("gamedesign/projects/critter-corral/art/generated/T0070/generated-critter-source-v1.png"),
    Path("gamedesign/projects/critter-corral/art/generated/T0070/generated-pen-source-v1.png"),
    Path("gamedesign/projects/critter-corral/art/generated/T0070/generated-card-horizontal-source-v2.png"),
    Path("gamedesign/projects/critter-corral/art/generated/T0070/generated-upgrade-icons-source-v1.png"),
]


def parse_key(value: str) -> tuple[int, int, int]:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("key must be #rrggbb or rrggbb")
    try:
        return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("key must be hex RGB") from exc


def source_paths(values: list[str]) -> list[Path]:
    if values:
        return [Path(value) for value in values]
    return [path for path in DEFAULT_SOURCES if path.exists()]


def benchmark_source(path: Path, *, key: tuple[int, int, int], iterations: int) -> dict[str, Any]:
    if not path.exists():
        raise FileNotFoundError(path)
    durations: list[float] = []
    bbox: tuple[int, int, int, int] | None = None
    size: tuple[int, int] | None = None
    for _index in range(iterations):
        image = Image.open(path).convert("RGBA")
        size = image.size
        started = perf_counter()
        cleaned = key_to_alpha(image, key=key)
        durations.append(perf_counter() - started)
        bbox = cleaned.getchannel("A").getbbox()
    best = min(durations)
    mean = sum(durations) / len(durations)
    megapixels = (size[0] * size[1] / 1_000_000.0) if size else 0.0
    return {
        "path": path.as_posix(),
        "size": list(size or (0, 0)),
        "iterations": iterations,
        "seconds": [round(value, 6) for value in durations],
        "best_seconds": round(best, 6),
        "mean_seconds": round(mean, 6),
        "best_megapixels_per_second": round(megapixels / best, 3) if best > 0 else 0,
        "alpha_bbox": list(bbox) if bbox is not None else None,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark shared generated-art chroma-key cleanup.")
    parser.add_argument("--source", action="append", default=[], help="PNG source to benchmark; may be repeated.")
    parser.add_argument("--iterations", type=int, default=1)
    parser.add_argument("--key", type=parse_key, default=(255, 0, 255))
    parser.add_argument("--json-output")
    args = parser.parse_args()

    if args.iterations < 1:
        raise SystemExit("--iterations must be >= 1")

    results = [benchmark_source(path, key=args.key, iterations=args.iterations) for path in source_paths(args.source)]
    report = {
        "schema": "game.asset_chroma_key_benchmark",
        "version": 1,
        "key": "#%02x%02x%02x" % args.key,
        "results": results,
    }
    for item in results:
        print(
            "{path} size={size} best={best_seconds:.3f}s mean={mean_seconds:.3f}s "
            "throughput={best_megapixels_per_second:.3f}MP/s alpha_bbox={alpha_bbox}".format(**item)
        )
    if args.json_output:
        write_json_atomic(Path(args.json_output), report)
        print(f"wrote {args.json_output}")


if __name__ == "__main__":
    main()
