"""Colour conversion and a palette-atlas allocator that refuses to overflow.

glTF stores COLOR_0 and base colour factors linear; artists, palettes and PNG texels are sRGB.
Mixing the two up washes models out or darkens them, so every conversion goes through here.
"""
import struct
import zlib
from pathlib import Path

import numpy as np

# The engine refills texels whose alpha is at or below 8 when it bleeds atlas edges, so a palette
# that uses alpha as a material channel keeps every texel above that.
ENGINE_ALPHA_REFILL = 8


def srgb_to_linear(value):
    value = np.asarray(value, float)
    return np.where(value <= 0.04045, value / 12.92, ((value + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(value):
    value = np.clip(np.asarray(value, float), 0.0, None)
    return np.where(value <= 0.0031308, value * 12.92, 1.055 * value ** (1 / 2.4) - 0.055)


def hex_to_srgb(text):
    """(r, g, b[, a]) floats 0..1 from '#rrggbb' or '#rrggbbaa'."""
    text = text.lstrip("#")
    if len(text) not in (6, 8):
        raise ValueError(f"colour {text!r} is not #rrggbb or #rrggbbaa")
    return tuple(int(text[i:i + 2], 16) / 255.0 for i in range(0, len(text), 2))


def hex_to_linear(text):
    """Linear RGB(A) for a Blender material or COLOR_0; alpha stays as authored."""
    values = hex_to_srgb(text)
    return tuple(float(v) for v in srgb_to_linear(values[:3])) + tuple(values[3:])


def linear_to_hex(rgb):
    encoded = np.clip(np.round(linear_to_srgb(np.asarray(rgb, float)[:3]) * 255), 0, 255).astype(int)
    return "#" + "".join(f"{v:02x}" for v in encoded)


def as_linear(colour):
    """Accept '#hex' (sRGB) or an already-linear tuple."""
    if isinstance(colour, str):
        return hex_to_linear(colour)
    return tuple(float(c) for c in colour)


class PaletteFull(ValueError):
    pass


class PaletteAtlas:
    """Rows of colour ramps in one texture; each material samples the centre of its row.

    Rows never wrap: when the texture is full, `add_row` raises instead of handing out a v that
    lands on another material. A texture whose row height is under 4 px loses rows to block
    compression (UASTC/BCn work in 4x4 blocks), so that layout is refused too.
    """

    def __init__(self, width=64, height=64, row_height=4, alpha_floor=12, allow_small_rows=False):
        if height % row_height:
            raise ValueError(f"height {height} is not a multiple of row_height {row_height}")
        if row_height < 4 and not allow_small_rows:
            raise ValueError("row_height under 4 px bleeds under block compression; "
                             "pass allow_small_rows=True for an uncompressed atlas")
        if not ENGINE_ALPHA_REFILL < alpha_floor <= 255:
            raise ValueError(f"alpha_floor must be above {ENGINE_ALPHA_REFILL}")
        self.width, self.height, self.row_height = width, height, row_height
        self.alpha_floor = alpha_floor
        self.rows = {}
        self.pixels = np.zeros((height, width, 4), np.uint8)

    @property
    def capacity(self):
        return self.height // self.row_height

    def add_row(self, name, stops, alpha=255):
        """Fill the next row with a ramp through `stops` ('#hex' or linear tuples), interpolated
        in linear space; one stop gives a flat row. Returns the row index."""
        if name in self.rows:
            raise ValueError(f"palette row {name!r} already exists")
        row = len(self.rows)
        if row >= self.capacity:
            raise PaletteFull(f"palette is full: {self.capacity} rows of {self.row_height} px in "
                              f"{self.width}x{self.height}; grow the texture or merge materials")
        if not self.alpha_floor <= alpha <= 255:
            raise ValueError(f"row {name!r} alpha {alpha} is below the floor {self.alpha_floor}")
        linear = np.array([as_linear(s)[:3] for s in stops], float)
        u = (np.arange(self.width) + 0.5) / self.width
        if len(linear) == 1:
            ramp = np.repeat(linear, self.width, axis=0)
        else:
            knots = np.linspace(0.0, 1.0, len(linear))
            ramp = np.stack([np.interp(u, knots, linear[:, c]) for c in range(3)], axis=1)
        encoded = np.clip(np.round(linear_to_srgb(ramp) * 255), 0, 255).astype(np.uint8)
        top = row * self.row_height
        self.pixels[top:top + self.row_height, :, :3] = encoded[None]
        self.pixels[top:top + self.row_height, :, 3] = alpha
        self.rows[name] = row
        return row

    def uv(self, name, u=0.5):
        """glTF UV (origin top-left) at the vertical centre of the row, so filtering never reaches
        the neighbouring row."""
        if name not in self.rows:
            raise KeyError(f"palette has no row {name!r}")
        v = (self.rows[name] * self.row_height + self.row_height / 2) / self.height
        return (float(u), float(v))

    def write_png(self, path):
        write_png(path, self.pixels)
        return Path(path)


def write_png(path, rgba):
    """Minimal RGBA8 PNG writer: Blender's Python has no PIL."""
    rgba = np.ascontiguousarray(rgba, np.uint8)
    height, width = rgba.shape[:2]
    raw = b"".join(b"\0" + rgba[y].tobytes() for y in range(height))

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data
                + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)
