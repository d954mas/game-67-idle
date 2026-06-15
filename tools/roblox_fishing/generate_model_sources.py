#!/usr/bin/env python3
"""Generate first-pass low-poly GLTF source meshes for Splash Rods."""

from __future__ import annotations

import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "gamedesign/projects/roblox-fishing/art/models"


def align4(data: bytearray) -> None:
    while len(data) % 4:
        data.append(0)


def write_gltf(name: str, positions: list[tuple[float, float, float]], indices: list[int]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    bin_data = bytearray()

    pos_offset = len(bin_data)
    for x, y, z in positions:
        bin_data += struct.pack("<fff", x, y, z)
    align4(bin_data)

    uv_offset = len(bin_data)
    for _ in positions:
        bin_data += struct.pack("<ff", 0.5, 0.5)
    align4(bin_data)

    idx_offset = len(bin_data)
    for index in indices:
        bin_data += struct.pack("<H", index)
    align4(bin_data)

    mins = [min(p[i] for p in positions) for i in range(3)]
    maxs = [max(p[i] for p in positions) for i in range(3)]
    bin_name = f"{name}.bin"
    gltf = {
        "asset": {"version": "2.0", "generator": "Splash Rods generated low-poly source"},
        "buffers": [{"uri": bin_name, "byteLength": len(bin_data)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": pos_offset, "byteLength": len(positions) * 12, "target": 34962},
            {"buffer": 0, "byteOffset": uv_offset, "byteLength": len(positions) * 8, "target": 34962},
            {"buffer": 0, "byteOffset": idx_offset, "byteLength": len(indices) * 2, "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": mins, "max": maxs},
            {"bufferView": 1, "componentType": 5126, "count": len(positions), "type": "VEC2"},
            {"bufferView": 2, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
        "meshes": [
            {
                "name": name,
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "TEXCOORD_0": 1},
                        "indices": 2,
                        "mode": 4,
                    }
                ],
            }
        ],
        "nodes": [{"mesh": 0, "name": name}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    (OUT_DIR / bin_name).write_bytes(bin_data)
    (OUT_DIR / f"{name}.gltf").write_text(json.dumps(gltf, indent=2), encoding="utf-8")


def add_box(verts: list[tuple[float, float, float]], inds: list[int], cx: float, cy: float, cz: float, sx: float, sy: float, sz: float) -> None:
    base = len(verts)
    x0, x1 = cx - sx * 0.5, cx + sx * 0.5
    y0, y1 = cy - sy * 0.5, cy + sy * 0.5
    z0, z1 = cz - sz * 0.5, cz + sz * 0.5
    verts.extend(
        [
            (x0, y0, z0),
            (x1, y0, z0),
            (x1, y1, z0),
            (x0, y1, z0),
            (x0, y0, z1),
            (x1, y0, z1),
            (x1, y1, z1),
            (x0, y1, z1),
        ]
    )
    inds.extend(
        [
            base + 0,
            base + 1,
            base + 2,
            base + 0,
            base + 2,
            base + 3,
            base + 1,
            base + 5,
            base + 6,
            base + 1,
            base + 6,
            base + 2,
            base + 5,
            base + 4,
            base + 7,
            base + 5,
            base + 7,
            base + 6,
            base + 4,
            base + 0,
            base + 3,
            base + 4,
            base + 3,
            base + 7,
            base + 3,
            base + 2,
            base + 6,
            base + 3,
            base + 6,
            base + 7,
            base + 4,
            base + 5,
            base + 1,
            base + 4,
            base + 1,
            base + 0,
        ]
    )


def make_fish() -> None:
    verts = [
        (-0.72, 0.00, 0.00),
        (-0.34, 0.22, -0.18),
        (0.24, 0.28, -0.12),
        (0.58, 0.00, 0.00),
        (0.24, -0.28, -0.12),
        (-0.34, -0.22, -0.18),
        (-0.34, 0.22, 0.18),
        (0.24, 0.28, 0.12),
        (0.24, -0.28, 0.12),
        (-0.34, -0.22, 0.18),
        (0.62, 0.00, -0.26),
        (0.95, 0.30, 0.00),
        (0.95, -0.30, 0.00),
        (-0.05, 0.48, 0.00),
        (0.18, 0.24, 0.00),
    ]
    inds = [
        0, 1, 5, 1, 2, 4, 1, 4, 5, 2, 3, 4,
        0, 9, 6, 6, 7, 2, 6, 2, 1, 7, 8, 4, 7, 4, 2, 8, 9, 5, 8, 5, 4, 9, 0, 5,
        2, 10, 3, 4, 3, 10, 3, 11, 12, 10, 11, 3, 10, 3, 12, 2, 13, 14,
    ]
    write_gltf("fish_trophy", verts, inds)


def make_boat() -> None:
    verts = [
        (-0.95, -0.20, -0.42), (0.95, -0.20, -0.42), (0.72, 0.16, -0.30), (-0.72, 0.16, -0.30),
        (-1.12, -0.20, 0.42), (1.12, -0.20, 0.42), (0.78, 0.16, 0.30), (-0.78, 0.16, 0.30),
        (0.00, -0.36, -0.02),
    ]
    inds = [
        0, 1, 2, 0, 2, 3, 4, 7, 6, 4, 6, 5,
        0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7,
        0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2,
        0, 8, 4, 1, 5, 8, 0, 1, 8, 4, 8, 5,
    ]
    write_gltf("toy_boat_hull", verts, inds)


def make_sign() -> None:
    verts: list[tuple[float, float, float]] = []
    inds: list[int] = []
    add_box(verts, inds, -0.36, -0.22, 0.0, 0.10, 0.74, 0.08)
    add_box(verts, inds, 0.36, -0.22, 0.0, 0.10, 0.74, 0.08)
    add_box(verts, inds, 0.0, 0.22, 0.0, 1.0, 0.46, 0.10)
    roof_base = len(verts)
    verts.extend([(-0.62, 0.46, -0.06), (0.62, 0.46, -0.06), (0.0, 0.84, -0.06), (-0.62, 0.46, 0.06), (0.62, 0.46, 0.06), (0.0, 0.84, 0.06)])
    inds.extend([roof_base + 0, roof_base + 1, roof_base + 2, roof_base + 3, roof_base + 5, roof_base + 4, roof_base + 0, roof_base + 3, roof_base + 4, roof_base + 0, roof_base + 4, roof_base + 1, roof_base + 1, roof_base + 4, roof_base + 5, roof_base + 1, roof_base + 5, roof_base + 2, roof_base + 2, roof_base + 5, roof_base + 3, roof_base + 2, roof_base + 3, roof_base + 0])
    write_gltf("shop_sign", verts, inds)


def make_leaf() -> None:
    verts = [(-0.72, 0.0, 0.0), (-0.18, 0.10, -0.12), (0.72, 0.0, 0.0), (-0.18, -0.10, 0.12), (-0.04, 0.0, 0.0)]
    inds = [0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4, 4, 1, 0, 4, 2, 1, 4, 3, 2, 4, 0, 3]
    write_gltf("palm_leaf_chunk", verts, inds)


def make_bobber() -> None:
    verts = [(0.0, 0.34, 0.0), (-0.24, 0.0, -0.24), (0.24, 0.0, -0.24), (0.24, 0.0, 0.24), (-0.24, 0.0, 0.24), (0.0, -0.34, 0.0)]
    inds = [0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1, 5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4]
    write_gltf("bobber_diamond", verts, inds)


def main() -> None:
    make_fish()
    make_boat()
    make_sign()
    make_leaf()
    make_bobber()


if __name__ == "__main__":
    main()
