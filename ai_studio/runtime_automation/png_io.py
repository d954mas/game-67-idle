#!/usr/bin/env python3
"""Dependency-free PNG read/write shared by the DevAPI capture + health tooling.

Single source for the PNG wire format (8-byte signature, chunk framing, IHDR
layout) that used to be hand-rolled three times: capture_window.py (BGRA->PNG
encode), devapi_client.py (RGB->PNG encode + a PPM bridge), and pixel_health.py
(full decode). stdlib only (zlib + struct) on purpose — these run on the minimal
CI Python with no Pillow. Capture-specific pixel munging (e.g. BGRA->RGB) stays
in the capture layer; this module only speaks RGB-in / PNG-out and PNG-in /
pixels-out.
"""

from __future__ import annotations

import os
import struct
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
# IHDR payload layout: width, height, bit_depth, color_type, compression method,
# filter method, interlace method.
_IHDR_FORMAT = ">IIBBBBB"
# We only ever emit 8-bit truecolour without alpha (PNG colour type 2).
COLOR_TYPE_RGB = 2


class PngError(RuntimeError):
    pass


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    """One PNG chunk: length + type + payload + CRC32(type + payload)."""
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def iter_png_chunks(data: bytes):
    """Yield (kind, payload) for each chunk in PNG bytes; stops after IEND."""
    if not data.startswith(PNG_SIGNATURE):
        raise PngError("not a PNG file")
    offset = len(PNG_SIGNATURE)
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        kind = data[offset + 4 : offset + 8]
        payload_start = offset + 8
        payload_end = payload_start + length
        if payload_end + 4 > len(data):
            raise PngError("truncated PNG chunk")
        yield kind, data[payload_start:payload_end]
        offset = payload_end + 4
        if kind == b"IEND":
            break


def encode_png_rgb(width: int, height: int, rgb: bytes) -> bytes:
    """Encode tightly-packed top-down 8-bit RGB pixels (no alpha) to PNG bytes."""
    stride = width * 3
    rows = []
    for y in range(height):
        rows.append(b"\x00" + rgb[y * stride : (y + 1) * stride])
    ihdr = struct.pack(_IHDR_FORMAT, width, height, 8, COLOR_TYPE_RGB, 0, 0, 0)
    return b"".join([
        PNG_SIGNATURE,
        png_chunk(b"IHDR", ihdr),
        png_chunk(b"IDAT", zlib.compress(b"".join(rows), 6)),
        png_chunk(b"IEND", b""),
    ])


def write_png_rgb(path: str, width: int, height: int, rgb: bytes) -> None:
    """Write 8-bit RGB pixels to a PNG file, creating parent dirs as needed."""
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(encode_png_rgb(width, height, rgb))


def bytes_per_pixel(color_type: int) -> int:
    if color_type == 0:
        return 1
    if color_type == 2:
        return 3
    if color_type == 6:
        return 4
    raise PngError(f"unsupported PNG color type: {color_type}")


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


def unfilter(raw: bytes, width: int, height: int, bpp: int) -> bytes:
    """Reverse the per-scanline PNG filters (types 0-4), returning raw pixels."""
    row_bytes = width * bpp
    expected = height * (row_bytes + 1)
    if len(raw) != expected:
        raise PngError(f"bad PNG payload size: {len(raw)} != {expected}")
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
                raise PngError(f"unsupported PNG filter: {filter_type}")
            row[x] = value
        out[y * row_bytes : (y + 1) * row_bytes] = row
    return bytes(out)


def read_png(path: str) -> tuple[int, int, int, bytes]:
    """Decode an 8-bit non-interlaced PNG to (width, height, color_type, pixels).

    Supports colour types 0 (grey), 2 (RGB), 6 (RGBA); pixels are tightly packed
    at bytes_per_pixel(color_type) per pixel.
    """
    with open(path, "rb") as handle:
        data = handle.read()

    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    for kind, payload in iter_png_chunks(data):
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _compression, _filter, interlace = struct.unpack(_IHDR_FORMAT, payload)
        elif kind == b"IDAT":
            compressed.extend(payload)

    if width is None or height is None or bit_depth is None or color_type is None:
        raise PngError("PNG missing IHDR")
    if bit_depth != 8:
        raise PngError(f"unsupported PNG bit depth: {bit_depth}")
    if interlace != 0:
        raise PngError("interlaced PNG is not supported")

    bpp = bytes_per_pixel(color_type)
    raw = zlib.decompress(bytes(compressed))
    pixels = unfilter(raw, width, height, bpp)
    return width, height, color_type, pixels
