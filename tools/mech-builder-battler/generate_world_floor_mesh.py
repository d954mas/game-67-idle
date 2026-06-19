#!/usr/bin/env python3
"""Generate a simple Y-up textured floor mesh for the stylized-studs material."""

from __future__ import annotations

import base64
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "meshes" / "mech_world_studs_floor.gltf"


def pack_floats(values: list[float]) -> bytes:
    return struct.pack("<" + "f" * len(values), *values)


def pack_u16(values: list[int]) -> bytes:
    return struct.pack("<" + "H" * len(values), *values)


def main() -> None:
    # Y-up plane. UVs intentionally exceed 0..1 so the texture repeats in world
    # space through the texture's repeat wrap defaults baked by the pack builder.
    positions = [
        -9.6,
        0.0,
        -8.7,
        9.6,
        0.0,
        -8.7,
        9.6,
        0.0,
        8.7,
        -9.6,
        0.0,
        8.7,
    ]
    normals = [0.0, 1.0, 0.0] * 4
    uvs = [0.0, 0.0, 5.0, 0.0, 5.0, 4.5, 0.0, 4.5]
    indices = [0, 1, 2, 0, 2, 3]

    chunks = [pack_floats(positions), pack_floats(normals), pack_floats(uvs), pack_u16(indices)]
    offsets = []
    blob = bytearray()
    for chunk in chunks:
        while len(blob) % 4:
            blob.append(0)
        offsets.append(len(blob))
        blob.extend(chunk)
    uri = "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")

    gltf = {
        "asset": {"version": "2.0", "generator": "game-67-idle world floor mesh generator"},
        "buffers": [{"uri": uri, "byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": offsets[0], "byteLength": len(chunks[0]), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[1], "byteLength": len(chunks[1]), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[2], "byteLength": len(chunks[2]), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[3], "byteLength": len(chunks[3]), "target": 34963},
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 4,
                "type": "VEC3",
                "min": [-9.6, 0.0, -8.7],
                "max": [9.6, 0.0, 8.7],
            },
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
        "meshes": [
            {
                "name": "mech_world_studs_floor",
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                        "indices": 3,
                        "mode": 4,
                    }
                ],
            }
        ],
        "nodes": [{"mesh": 0, "name": "mech_world_studs_floor"}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(gltf, separators=(",", ":")), encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
