#!/usr/bin/env python3
"""Lightweight pixel-health checks for DevAPI screenshot evidence."""

from __future__ import annotations

import argparse
import math
import os
import struct
import zlib
from dataclasses import dataclass


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


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


def _png_chunks(data: bytes):
    if not data.startswith(PNG_SIGNATURE):
        raise PixelHealthError("not a PNG file")
    offset = len(PNG_SIGNATURE)
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        kind = data[offset + 4 : offset + 8]
        payload_start = offset + 8
        payload_end = payload_start + length
        if payload_end + 4 > len(data):
            raise PixelHealthError("truncated PNG chunk")
        yield kind, data[payload_start:payload_end]
        offset = payload_end + 4
        if kind == b"IEND":
            break


def _bytes_per_pixel(color_type: int) -> int:
    if color_type == 0:
        return 1
    if color_type == 2:
        return 3
    if color_type == 6:
        return 4
    raise PixelHealthError(f"unsupported PNG color type: {color_type}")


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter(raw: bytes, width: int, height: int, bpp: int) -> bytes:
    row_bytes = width * bpp
    expected = height * (row_bytes + 1)
    if len(raw) != expected:
        raise PixelHealthError(f"bad PNG payload size: {len(raw)} != {expected}")
    out = bytearray(height * row_bytes)
    src = 0
    for y in range(height):
        filter_type = raw[src]
        src += 1
        row = bytearray(raw[src : src + row_bytes])
        src += row_bytes
        prior_offset = (y - 1) * row_bytes
        for x in range(row_bytes):
            left = row[x - bpp] if x >= bpp else 0
            up = out[prior_offset + x] if y > 0 else 0
            up_left = out[prior_offset + x - bpp] if y > 0 and x >= bpp else 0
            if filter_type == 0:
                value = row[x]
            elif filter_type == 1:
                value = (row[x] + left) & 0xFF
            elif filter_type == 2:
                value = (row[x] + up) & 0xFF
            elif filter_type == 3:
                value = (row[x] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                value = (row[x] + _paeth(left, up, up_left)) & 0xFF
            else:
                raise PixelHealthError(f"unsupported PNG filter: {filter_type}")
            row[x] = value
        out[y * row_bytes : (y + 1) * row_bytes] = row
    return bytes(out)


def _read_png_rgb(path: str) -> tuple[int, int, int, bytes]:
    with open(path, "rb") as handle:
        data = handle.read()

    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    for kind, payload in _png_chunks(data):
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _compression, _filter, interlace = struct.unpack(">IIBBBBB", payload)
        elif kind == b"IDAT":
            compressed.extend(payload)

    if width is None or height is None or bit_depth is None or color_type is None:
        raise PixelHealthError("PNG missing IHDR")
    if bit_depth != 8:
        raise PixelHealthError(f"unsupported PNG bit depth: {bit_depth}")
    if interlace != 0:
        raise PixelHealthError("interlaced PNG is not supported")

    bpp = _bytes_per_pixel(color_type)
    raw = zlib.decompress(bytes(compressed))
    pixels = _unfilter(raw, width, height, bpp)
    return width, height, color_type, pixels


def analyze_png(path: str, max_samples: int = 50000) -> PixelHealth:
    if not os.path.exists(path) or os.path.getsize(path) <= 0:
        raise PixelHealthError(f"screenshot missing or empty: {path}")
    width, height, color_type, pixels = _read_png_rgb(path)
    bpp = _bytes_per_pixel(color_type)
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
    except PixelHealthError as exc:
        print(f"FAIL pixel health: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
