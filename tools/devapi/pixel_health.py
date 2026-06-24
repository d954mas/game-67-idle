#!/usr/bin/env python3
"""Lightweight pixel-health checks for DevAPI screenshot evidence."""

from __future__ import annotations

import argparse
import math
import os
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from png_io import PngError, bytes_per_pixel, read_png  # noqa: E402


class PixelHealthError(RuntimeError):
    pass


@dataclass(frozen=True)
class PixelHealth:
    path: str
    width: int
    height: int
    sample_count: int
    unique_colors: int
    unique_buckets: int
    luma_min: float
    luma_max: float
    luma_mean: float
    luma_stdev: float

    @property
    def luma_range(self) -> float:
        return self.luma_max - self.luma_min

    def summary(self) -> str:
        return (
            f"{self.width}x{self.height}, samples={self.sample_count}, "
            f"unique={self.unique_colors}, buckets={self.unique_buckets}, "
            f"luma_range={self.luma_range:.1f}, stdev={self.luma_stdev:.1f}, "
            f"mean={self.luma_mean:.1f}"
        )


def analyze_png(path: str, max_samples: int = 50000) -> PixelHealth:
    if not os.path.exists(path) or os.path.getsize(path) <= 0:
        raise PixelHealthError(f"screenshot missing or empty: {path}")
    width, height, color_type, pixels = read_png(path)
    bpp = bytes_per_pixel(color_type)
    total_pixels = width * height
    if total_pixels <= 0:
        raise PixelHealthError("screenshot has no pixels")
    step = max(1, total_pixels // max(1, max_samples))

    unique: set[tuple[int, int, int]] = set()
    buckets: set[tuple[int, int, int]] = set()
    count = 0
    luma_min = 255.0
    luma_max = 0.0
    mean = 0.0
    m2 = 0.0

    for pixel_index in range(0, total_pixels, step):
        offset = pixel_index * bpp
        if color_type == 0:
            r = g = b = pixels[offset]
        else:
            r = pixels[offset]
            g = pixels[offset + 1]
            b = pixels[offset + 2]
        unique.add((r, g, b))
        buckets.add((r >> 4, g >> 4, b >> 4))
        luma = (0.2126 * r) + (0.7152 * g) + (0.0722 * b)
        luma_min = min(luma_min, luma)
        luma_max = max(luma_max, luma)
        count += 1
        delta = luma - mean
        mean += delta / count
        m2 += delta * (luma - mean)

    variance = m2 / count if count > 0 else 0.0
    return PixelHealth(
        path=os.path.abspath(path),
        width=width,
        height=height,
        sample_count=count,
        unique_colors=len(unique),
        unique_buckets=len(buckets),
        luma_min=luma_min,
        luma_max=luma_max,
        luma_mean=mean,
        luma_stdev=math.sqrt(variance),
    )


def assert_pixel_health(
    path: str,
    *,
    min_unique_colors: int = 16,
    min_unique_buckets: int = 8,
    min_luma_range: float = 32.0,
    min_luma_stdev: float = 10.0,
) -> PixelHealth:
    health = analyze_png(path)
    failures: list[str] = []
    if health.unique_colors < min_unique_colors:
        failures.append(f"unique colors {health.unique_colors} < {min_unique_colors}")
    if health.unique_buckets < min_unique_buckets:
        failures.append(f"unique color buckets {health.unique_buckets} < {min_unique_buckets}")
    if health.luma_range < min_luma_range:
        failures.append(f"luma range {health.luma_range:.1f} < {min_luma_range:.1f}")
    if health.luma_stdev < min_luma_stdev:
        failures.append(f"luma stdev {health.luma_stdev:.1f} < {min_luma_stdev:.1f}")
    if failures:
        raise PixelHealthError(f"unhealthy screenshot: {health.summary()} :: {'; '.join(failures)}")
    return health


def main() -> int:
    parser = argparse.ArgumentParser(description="Fail nonzero for blank or flat PNG screenshots.")
    parser.add_argument("path")
    parser.add_argument("--min-unique-colors", type=int, default=16)
    parser.add_argument("--min-unique-buckets", type=int, default=8)
    parser.add_argument("--min-luma-range", type=float, default=32.0)
    parser.add_argument("--min-luma-stdev", type=float, default=10.0)
    args = parser.parse_args()
    try:
        health = assert_pixel_health(
            args.path,
            min_unique_colors=args.min_unique_colors,
            min_unique_buckets=args.min_unique_buckets,
            min_luma_range=args.min_luma_range,
            min_luma_stdev=args.min_luma_stdev,
        )
        print(f"PASS pixel health: {health.summary()}")
        return 0
    except (PixelHealthError, PngError) as exc:
        # PngError covers malformed/undecodable PNGs (the decoder now lives in
        # png_io); the FAIL message text is unchanged from the old in-file codec.
        print(f"FAIL pixel health: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
