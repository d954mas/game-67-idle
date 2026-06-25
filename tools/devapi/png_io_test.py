#!/usr/bin/env python3
"""Tests for the shared dependency-free PNG codec (tools/devapi/png_io.py)."""

from __future__ import annotations

import os
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from png_io import (  # noqa: E402
    PNG_SIGNATURE,
    PngError,
    bytes_per_pixel,
    encode_png_rgb,
    iter_png_chunks,
    png_chunk,
    read_png,
    unfilter,
    write_png_rgb,
)


def _ihdr(width: int, height: int, color_type: int) -> bytes:
    return struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)


def _png_from_scanlines(width: int, height: int, color_type: int, scanlines: bytes) -> bytes:
    """Assemble a PNG whose IDAT is the given pre-filtered scanline bytes."""
    return b"".join([
        PNG_SIGNATURE,
        png_chunk(b"IHDR", _ihdr(width, height, color_type)),
        png_chunk(b"IDAT", zlib.compress(scanlines, 6)),
        png_chunk(b"IEND", b""),
    ])


class EncodeRoundTrip(unittest.TestCase):
    def test_encode_rgb_then_read_back_is_lossless(self) -> None:
        width, height = 3, 2
        rgb = bytes(range(width * height * 3))  # 18 distinct-ish bytes
        png = encode_png_rgb(width, height, rgb)
        self.assertTrue(png.startswith(PNG_SIGNATURE))
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "nested", "out.png")  # parent must be created
            write_png_rgb(path, width, height, rgb)
            self.assertTrue(os.path.exists(path))
            w, h, color_type, pixels = read_png(path)
        self.assertEqual((w, h, color_type), (width, height, 2))
        self.assertEqual(pixels, rgb)

    def test_write_png_rgb_creates_parent_dirs(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "a", "b", "c.png")
            write_png_rgb(path, 1, 1, b"\x10\x20\x30")
            self.assertTrue(os.path.exists(path))


class FilterDecode(unittest.TestCase):
    """unfilter previously had no test; exercise all five filter types."""

    def test_filter_none(self) -> None:
        # 2x1 RGB, filter 0: bytes are literal.
        scan = b"\x00" + bytes([10, 20, 30, 40, 50, 60])
        out = unfilter(scan, 2, 1, 3)
        self.assertEqual(out, bytes([10, 20, 30, 40, 50, 60]))

    def test_filter_sub(self) -> None:
        # filter 1 (sub): value += left (bpp back). First pixel passes through.
        scan = b"\x01" + bytes([10, 20, 30, 5, 5, 5])
        out = unfilter(scan, 2, 1, 3)
        self.assertEqual(out, bytes([10, 20, 30, 15, 25, 35]))

    def test_filter_up(self) -> None:
        # filter 2 (up): value += pixel directly above.
        row0 = b"\x00" + bytes([10, 20, 30])
        row1 = b"\x02" + bytes([1, 2, 3])
        out = unfilter(row0 + row1, 1, 2, 3)
        self.assertEqual(out, bytes([10, 20, 30, 11, 22, 33]))

    def test_filter_average(self) -> None:
        # filter 3 (average): value += (left + up)//2. Top-left: left=0, up=0.
        row0 = b"\x00" + bytes([8, 8, 8])
        row1 = b"\x03" + bytes([2, 2, 2])
        out = unfilter(row0 + row1, 1, 2, 3)
        # up=8, left=0 -> +4
        self.assertEqual(out, bytes([8, 8, 8, 6, 6, 6]))

    def test_filter_paeth(self) -> None:
        # filter 4 (paeth): top row has no up/up-left, predictor == left.
        scan = b"\x04" + bytes([10, 20, 30, 1, 1, 1])
        out = unfilter(scan, 2, 1, 3)
        self.assertEqual(out, bytes([10, 20, 30, 11, 21, 31]))

    def test_filter_paeth_multirow_roundtrip(self) -> None:
        # Top-row paeth degenerates to `sub`; a second row exercises the real
        # predictor (up / up_left) and the tie-break branches. Encode with an
        # INDEPENDENT reference paeth, decode, and assert the pixels round-trip.
        def paeth_ref(a: int, b: int, c: int) -> int:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            if pa <= pb and pa <= pc:
                return a
            if pb <= pc:
                return b
            return c

        width, bpp = 2, 3  # 2x2 RGB
        rows = [bytes([10, 200, 30, 250, 5, 130]), bytes([60, 70, 90, 15, 240, 110])]
        scan = bytearray()
        prev = bytes(width * bpp)
        for row in rows:
            filtered = bytearray(len(row))
            for x in range(len(row)):
                left = row[x - bpp] if x >= bpp else 0
                up = prev[x]
                up_left = prev[x - bpp] if x >= bpp else 0
                filtered[x] = (row[x] - paeth_ref(left, up, up_left)) & 0xFF
            scan += b"\x04" + bytes(filtered)
            prev = row
        out = unfilter(bytes(scan), width, 2, bpp)
        self.assertEqual(out, b"".join(rows))

    def test_read_png_decodes_a_filtered_image(self) -> None:
        png = _png_from_scanlines(2, 1, 2, b"\x01" + bytes([10, 20, 30, 5, 5, 5]))
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "f.png")
            with open(path, "wb") as handle:
                handle.write(png)
            w, h, color_type, pixels = read_png(path)
        self.assertEqual((w, h, color_type), (2, 1, 2))
        self.assertEqual(pixels, bytes([10, 20, 30, 15, 25, 35]))


class ColorTypes(unittest.TestCase):
    def test_bytes_per_pixel(self) -> None:
        self.assertEqual(bytes_per_pixel(0), 1)
        self.assertEqual(bytes_per_pixel(2), 3)
        self.assertEqual(bytes_per_pixel(6), 4)

    def test_bytes_per_pixel_rejects_unsupported(self) -> None:
        with self.assertRaises(PngError):
            bytes_per_pixel(3)  # indexed/palette not supported

    def test_decode_grey(self) -> None:
        png = _png_from_scanlines(3, 1, 0, b"\x00" + bytes([10, 128, 240]))
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "grey.png")
            with open(path, "wb") as handle:
                handle.write(png)
            w, h, color_type, pixels = read_png(path)
        self.assertEqual((w, h, color_type), (3, 1, 0))
        self.assertEqual(pixels, bytes([10, 128, 240]))

    def test_decode_rgba(self) -> None:
        png = _png_from_scanlines(1, 1, 6, b"\x00" + bytes([1, 2, 3, 4]))
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "rgba.png")
            with open(path, "wb") as handle:
                handle.write(png)
            w, h, color_type, pixels = read_png(path)
        self.assertEqual((w, h, color_type), (1, 1, 6))
        self.assertEqual(pixels, bytes([1, 2, 3, 4]))


class ErrorPaths(unittest.TestCase):
    def test_iter_chunks_rejects_non_png(self) -> None:
        with self.assertRaises(PngError):
            list(iter_png_chunks(b"not a png"))

    def test_iter_chunks_rejects_truncated(self) -> None:
        truncated = PNG_SIGNATURE + struct.pack(">I", 100) + b"IHDR" + b"\x00\x00"
        with self.assertRaises(PngError):
            list(iter_png_chunks(truncated))

    def test_unfilter_rejects_bad_size(self) -> None:
        with self.assertRaises(PngError):
            unfilter(b"\x00\x01\x02", 5, 5, 3)

    def test_unfilter_rejects_unknown_filter(self) -> None:
        with self.assertRaises(PngError):
            unfilter(b"\x09\x00\x00\x00", 1, 1, 3)


if __name__ == "__main__":
    unittest.main()
