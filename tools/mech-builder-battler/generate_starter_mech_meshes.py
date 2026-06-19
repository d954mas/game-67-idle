#!/usr/bin/env python3
"""Generate authored low-poly starter mech part meshes.

The generated glTF files are project-owned source assets for the first
Roblox-like starter pass. They stay blocky on purpose, but each part carries a
clear mech role: chunky torso, cockpit head, oversized shoulders, broad feet,
studs, armor plates, and readable module sockets.
"""

from __future__ import annotations

import base64
import json
import math
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "assets" / "meshes"


def norm(v: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = v
    d = math.sqrt((x * x) + (y * y) + (z * z)) or 1.0
    return (x / d, y / d, z / d)


def face_normal(a, b, c):
    ux, uy, uz = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    vx, vy, vz = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    return norm(((uy * vz) - (uz * vy), (uz * vx) - (ux * vz), (ux * vy) - (uy * vx)))


class Mesh:
    def __init__(self) -> None:
        self.positions: list[tuple[float, float, float]] = []
        self.normals: list[tuple[float, float, float]] = []
        self.indices: list[int] = []

    def add_face(self, verts: list[tuple[float, float, float]], normal=None) -> None:
        if normal is None:
            normal = face_normal(verts[0], verts[1], verts[2])
        start = len(self.positions)
        self.positions.extend(verts)
        self.normals.extend([normal] * len(verts))
        if len(verts) == 3:
            self.indices.extend([start, start + 1, start + 2])
        elif len(verts) == 4:
            self.indices.extend([start, start + 1, start + 2, start, start + 2, start + 3])
        else:
            for i in range(1, len(verts) - 1):
                self.indices.extend([start, start + i, start + i + 1])


def bevel_box(bevel: float = 0.11) -> Mesh:
    m = Mesh()
    hx = hy = hz = 0.5
    b = bevel

    # Six broad faces.
    m.add_face([(hx, -hy + b, -hz + b), (hx, hy - b, -hz + b), (hx, hy - b, hz - b), (hx, -hy + b, hz - b)], (1, 0, 0))
    m.add_face([(-hx, -hy + b, hz - b), (-hx, hy - b, hz - b), (-hx, hy - b, -hz + b), (-hx, -hy + b, -hz + b)], (-1, 0, 0))
    m.add_face([(-hx + b, hy, -hz + b), (hx - b, hy, -hz + b), (hx - b, hy, hz - b), (-hx + b, hy, hz - b)], (0, 1, 0))
    m.add_face([(-hx + b, -hy, hz - b), (hx - b, -hy, hz - b), (hx - b, -hy, -hz + b), (-hx + b, -hy, -hz + b)], (0, -1, 0))
    m.add_face([(-hx + b, -hy + b, hz), (hx - b, -hy + b, hz), (hx - b, hy - b, hz), (-hx + b, hy - b, hz)], (0, 0, 1))
    m.add_face([(-hx + b, hy - b, -hz), (hx - b, hy - b, -hz), (hx - b, -hy + b, -hz), (-hx + b, -hy + b, -hz)], (0, 0, -1))

    # Edge chamfers.
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.add_face(
                [
                    (sx * hx, sy * (hy - b), -hz + b),
                    (sx * hx, sy * (hy - b), hz - b),
                    (sx * (hx - b), sy * hy, hz - b),
                    (sx * (hx - b), sy * hy, -hz + b),
                ],
                norm((sx, sy, 0)),
            )
    for sx in (-1, 1):
        for sz in (-1, 1):
            m.add_face(
                [
                    (sx * (hx - b), -hy + b, sz * hz),
                    (sx * (hx - b), hy - b, sz * hz),
                    (sx * hx, hy - b, sz * (hz - b)),
                    (sx * hx, -hy + b, sz * (hz - b)),
                ],
                norm((sx, 0, sz)),
            )
    for sy in (-1, 1):
        for sz in (-1, 1):
            m.add_face(
                [
                    (-hx + b, sy * hy, sz * (hz - b)),
                    (hx - b, sy * hy, sz * (hz - b)),
                    (hx - b, sy * (hy - b), sz * hz),
                    (-hx + b, sy * (hy - b), sz * hz),
                ],
                norm((0, sy, sz)),
            )

    # Corner facets.
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                m.add_face(
                    [
                        (sx * hx, sy * (hy - b), sz * (hz - b)),
                        (sx * (hx - b), sy * hy, sz * (hz - b)),
                        (sx * (hx - b), sy * (hy - b), sz * hz),
                    ],
                    norm((sx, sy, sz)),
                )
    return m


def tapered_body() -> Mesh:
    m = Mesh()
    layers = [
        (-0.56, 0.46, 0.36),
        (-0.22, 0.58, 0.48),
        (0.30, 0.74, 0.54),
        (0.58, 0.66, 0.48),
    ]
    rings = []
    for y, x, z in layers:
        rings.append([(-x, y, -z), (x, y, -z), (x, y, z), (-x, y, z)])
    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        for j in range(4):
            m.add_face([a[j], a[(j + 1) % 4], b[(j + 1) % 4], b[j]])
    m.add_face([rings[0][3], rings[0][2], rings[0][1], rings[0][0]])
    m.add_face([rings[-1][0], rings[-1][1], rings[-1][2], rings[-1][3]])
    # Raised block plates keep the silhouette closer to a toy mech than a
    # tapered realistic robot.
    m.add_face([(-0.44, -0.16, -0.56), (0.44, -0.16, -0.56), (0.38, 0.46, -0.62), (-0.38, 0.46, -0.62)])
    m.add_face([(-0.26, -0.48, 0.50), (0.26, -0.48, 0.50), (0.22, 0.56, 0.62), (-0.22, 0.56, 0.62)])
    m.add_face([(-0.68, 0.18, -0.40), (-0.48, 0.18, -0.40), (-0.48, 0.54, -0.42), (-0.68, 0.54, -0.42)])
    m.add_face([(0.48, 0.18, -0.40), (0.68, 0.18, -0.40), (0.68, 0.54, -0.42), (0.48, 0.54, -0.42)])
    return m


def cockpit_head() -> Mesh:
    m = bevel_box(0.10)
    # Forward brow and rear plug read like a Roblox block helmet/cockpit.
    m.add_face([(-0.50, 0.12, -0.56), (0.50, 0.12, -0.56), (0.42, 0.36, -0.62), (-0.42, 0.36, -0.62)])
    m.add_face([(-0.26, -0.28, 0.50), (0.26, -0.28, 0.50), (0.22, 0.28, 0.58), (-0.22, 0.28, 0.58)])
    return m


def foot_skid() -> Mesh:
    m = Mesh()
    b = [(-0.62, -0.30, -0.66), (0.62, -0.30, -0.66), (0.60, -0.30, 0.62), (-0.60, -0.30, 0.62)]
    t = [(-0.54, 0.24, -0.48), (0.54, 0.24, -0.48), (0.42, 0.24, 0.46), (-0.42, 0.24, 0.46)]
    for i in range(4):
        m.add_face([b[i], b[(i + 1) % 4], t[(i + 1) % 4], t[i]])
    m.add_face([b[3], b[2], b[1], b[0]])
    m.add_face([t[0], t[1], t[2], t[3]])
    m.add_face([(-0.64, -0.08, -0.70), (0.64, -0.08, -0.70), (0.48, 0.08, -0.46), (-0.48, 0.08, -0.46)])
    m.add_face([(-0.46, 0.24, 0.02), (-0.18, 0.24, 0.02), (-0.18, 0.36, 0.28), (-0.46, 0.36, 0.28)])
    m.add_face([(0.18, 0.24, 0.02), (0.46, 0.24, 0.02), (0.46, 0.36, 0.28), (0.18, 0.36, 0.28)])
    return m


def cylinder_z(sides: int = 14) -> Mesh:
    m = Mesh()
    r = 0.5
    z0 = -0.5
    z1 = 0.5
    front_center = (0.0, 0.0, z1)
    back_center = (0.0, 0.0, z0)
    for i in range(sides):
        a0 = (i / sides) * math.tau
        a1 = ((i + 1) / sides) * math.tau
        p0 = (math.cos(a0) * r, math.sin(a0) * r, z0)
        p1 = (math.cos(a1) * r, math.sin(a1) * r, z0)
        p2 = (math.cos(a1) * r, math.sin(a1) * r, z1)
        p3 = (math.cos(a0) * r, math.sin(a0) * r, z1)
        m.add_face([p0, p1, p2, p3], norm((math.cos((a0 + a1) * 0.5), math.sin((a0 + a1) * 0.5), 0)))
        m.add_face([front_center, p3, p2], (0, 0, 1))
        m.add_face([back_center, p1, p0], (0, 0, -1))
    return m


def cylinder_y(sides: int = 14) -> Mesh:
    m = Mesh()
    r = 0.5
    y0 = -0.5
    y1 = 0.5
    top_center = (0.0, y1, 0.0)
    bottom_center = (0.0, y0, 0.0)
    for i in range(sides):
        a0 = (i / sides) * math.tau
        a1 = ((i + 1) / sides) * math.tau
        p0 = (math.cos(a0) * r, y0, math.sin(a0) * r)
        p1 = (math.cos(a1) * r, y0, math.sin(a1) * r)
        p2 = (math.cos(a1) * r, y1, math.sin(a1) * r)
        p3 = (math.cos(a0) * r, y1, math.sin(a0) * r)
        m.add_face([p0, p1, p2, p3], norm((math.cos((a0 + a1) * 0.5), 0, math.sin((a0 + a1) * 0.5))))
        m.add_face([top_center, p2, p3], (0, 1, 0))
        m.add_face([bottom_center, p0, p1], (0, -1, 0))
    return m


def lowpoly_joint() -> Mesh:
    m = Mesh()
    top = (0.0, 0.48, 0.0)
    bottom = (0.0, -0.48, 0.0)
    ring = []
    for i in range(8):
        a = (i / 8) * math.tau
        ring.append((math.cos(a) * 0.50, 0.0, math.sin(a) * 0.50))
    for i in range(8):
        m.add_face([top, ring[i], ring[(i + 1) % 8]])
        m.add_face([bottom, ring[(i + 1) % 8], ring[i]])
    return m


def armor_plate() -> Mesh:
    m = Mesh()
    front = [(-0.56, -0.42, -0.16), (0.56, -0.42, -0.16), (0.42, 0.42, -0.20), (-0.42, 0.42, -0.20)]
    back = [(-0.40, -0.32, 0.16), (0.40, -0.32, 0.16), (0.30, 0.32, 0.18), (-0.30, 0.32, 0.18)]
    m.add_face(front)
    m.add_face([back[3], back[2], back[1], back[0]])
    for i in range(4):
        m.add_face([front[i], front[(i + 1) % 4], back[(i + 1) % 4], back[i]])
    m.add_face([(-0.30, -0.06, -0.24), (0.30, -0.06, -0.24), (0.24, 0.16, -0.26), (-0.24, 0.16, -0.26)])
    return m


def visor_plate() -> Mesh:
    m = Mesh()
    front = [(-0.52, -0.16, -0.08), (0.52, -0.16, -0.08), (0.42, 0.16, -0.10), (-0.42, 0.16, -0.10)]
    back = [(-0.42, -0.12, 0.08), (0.42, -0.12, 0.08), (0.34, 0.12, 0.08), (-0.34, 0.12, 0.08)]
    m.add_face(front)
    m.add_face([back[3], back[2], back[1], back[0]])
    for i in range(4):
        m.add_face([front[i], front[(i + 1) % 4], back[(i + 1) % 4], back[i]])
    return m


def write_mesh(name: str, mesh: Mesh) -> None:
    assert len(mesh.positions) < 65535
    blob = b"".join(struct.pack("<3f", *p) for p in mesh.positions)
    normal_offset = len(blob)
    blob += b"".join(struct.pack("<3f", *n) for n in mesh.normals)
    index_offset = len(blob)
    blob += b"".join(struct.pack("<H", i) for i in mesh.indices)

    mins = [min(p[i] for p in mesh.positions) for i in range(3)]
    maxs = [max(p[i] for p in mesh.positions) for i in range(3)]
    gltf = {
        "asset": {"version": "2.0", "generator": "game-67-idle starter mech mesh generator"},
        "buffers": [{"uri": "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii"), "byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": normal_offset, "target": 34962},
            {"buffer": 0, "byteOffset": normal_offset, "byteLength": index_offset - normal_offset, "target": 34962},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(blob) - index_offset, "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(mesh.positions), "type": "VEC3", "min": mins, "max": maxs},
            {"bufferView": 1, "componentType": 5126, "count": len(mesh.normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5123, "count": len(mesh.indices), "type": "SCALAR"},
        ],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "mode": 4}]}],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    (OUT_DIR / f"{name}.gltf").write_text(json.dumps(gltf, separators=(",", ":")), encoding="utf-8")


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    meshes = {
        "mech_starter_torso": tapered_body(),
        "mech_starter_pelvis": bevel_box(0.16),
        "mech_starter_head": cockpit_head(),
        "mech_starter_shoulder": bevel_box(0.18),
        "mech_starter_limb": bevel_box(0.14),
        "mech_starter_forearm": bevel_box(0.18),
        "mech_starter_weapon": cylinder_z(16),
        "mech_starter_foot": foot_skid(),
        "mech_starter_rocket_pod": cylinder_z(16),
        "mech_starter_rocket_tube": cylinder_z(12),
        "mech_starter_vent": bevel_box(0.08),
        "mech_starter_hydraulic": cylinder_y(14),
        "mech_starter_joint": lowpoly_joint(),
        "mech_starter_armor_plate": armor_plate(),
        "mech_starter_visor": visor_plate(),
    }
    for name, mesh in meshes.items():
        write_mesh(name, mesh)
        print(f"wrote assets/meshes/{name}.gltf")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
