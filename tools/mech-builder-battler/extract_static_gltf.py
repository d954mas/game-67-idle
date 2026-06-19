#!/usr/bin/env python3
"""Extract static mesh streams from a downloaded GLB into a packer-friendly glTF."""

from __future__ import annotations

import base64
import json
import re
import struct
import sys
from pathlib import Path


COMPONENT_SIZE = {
    5121: 1,
    5123: 2,
    5125: 4,
    5126: 4,
}
TYPE_COUNT = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
}


def identity_transform() -> tuple[list[list[float]], list[float]]:
    return ([[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], [0.0, 0.0, 0.0])


def quat_to_mat3(q: list[float]) -> list[list[float]]:
    x, y, z, w = q
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    return [
        [1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy)],
        [2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx)],
        [2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)],
    ]


def mat3_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [
        [sum(a[row][k] * b[k][col] for k in range(3)) for col in range(3)]
        for row in range(3)
    ]


def mat3_vec(m: list[list[float]], v: tuple[float, float, float] | list[float]) -> list[float]:
    return [sum(m[row][col] * v[col] for col in range(3)) for row in range(3)]


def compose_transform(
    parent: tuple[list[list[float]], list[float]],
    local: tuple[list[list[float]], list[float]],
) -> tuple[list[list[float]], list[float]]:
    parent_m, parent_t = parent
    local_m, local_t = local
    out_m = mat3_mul(parent_m, local_m)
    local_world = mat3_vec(parent_m, local_t)
    out_t = [local_world[i] + parent_t[i] for i in range(3)]
    return out_m, out_t


def node_transform(node: dict) -> tuple[list[list[float]], list[float]]:
    if "matrix" in node:
        m = node["matrix"]
        return (
            [[float(m[0]), float(m[4]), float(m[8])],
             [float(m[1]), float(m[5]), float(m[9])],
             [float(m[2]), float(m[6]), float(m[10])]],
            [float(m[12]), float(m[13]), float(m[14])],
        )
    rotation = quat_to_mat3([float(v) for v in node.get("rotation", [0.0, 0.0, 0.0, 1.0])])
    scale = [float(v) for v in node.get("scale", [1.0, 1.0, 1.0])]
    scaled = [[rotation[row][col] * scale[col] for col in range(3)] for row in range(3)]
    translation = [float(v) for v in node.get("translation", [0.0, 0.0, 0.0])]
    return scaled, translation


def transform_position(
    transform: tuple[list[list[float]], list[float]],
    pos: tuple[float, float, float],
) -> tuple[float, float, float]:
    mat, trans = transform
    out = mat3_vec(mat, pos)
    return (out[0] + trans[0], out[1] + trans[1], out[2] + trans[2])


def transform_normal(
    transform: tuple[list[list[float]], list[float]],
    normal: tuple[float, float, float],
) -> tuple[float, float, float]:
    mat, _trans = transform
    out = mat3_vec(mat, normal)
    length = max((out[0] * out[0] + out[1] * out[1] + out[2] * out[2]) ** 0.5, 0.000001)
    return (out[0] / length, out[1] / length, out[2] / length)


def read_glb(path: Path) -> tuple[dict, bytes]:
    data = path.read_bytes()
    magic, version, _length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2:
        raise ValueError(f"{path} is not a glTF 2.0 GLB")
    offset = 12
    doc = None
    bin_chunk = b""
    while offset < len(data):
        chunk_len, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_len]
        offset += chunk_len
        if chunk_type == 0x4E4F534A:
            doc = json.loads(chunk.decode("utf-8"))
        elif chunk_type == 0x004E4942:
            bin_chunk = chunk
    if doc is None:
        raise ValueError(f"{path} has no JSON chunk")
    return doc, bin_chunk


def accessor_bytes(doc: dict, blob: bytes, accessor_index: int) -> bytes:
    accessor = doc["accessors"][accessor_index]
    view = doc["bufferViews"][accessor["bufferView"]]
    component_type = accessor["componentType"]
    component_size = COMPONENT_SIZE[component_type]
    width = TYPE_COUNT[accessor["type"]]
    elem_size = component_size * width
    count = accessor["count"]
    stride = view.get("byteStride", elem_size)
    base = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    out = bytearray()
    for i in range(count):
        start = base + i * stride
        out.extend(blob[start : start + elem_size])
    return bytes(out)


def accessor_floats(doc: dict, blob: bytes, accessor_index: int, width: int) -> list[tuple[float, ...]]:
    raw = accessor_bytes(doc, blob, accessor_index)
    values = struct.unpack("<" + "f" * (len(raw) // 4), raw)
    return [tuple(values[i : i + width]) for i in range(0, len(values), width)]


def accessor_indices(doc: dict, blob: bytes, accessor_index: int) -> list[int]:
    accessor = doc["accessors"][accessor_index]
    raw = accessor_bytes(doc, blob, accessor_index)
    if accessor["componentType"] == 5121:
        return list(raw)
    if accessor["componentType"] == 5123:
        return list(struct.unpack("<" + "H" * (len(raw) // 2), raw))
    if accessor["componentType"] == 5125:
        return list(struct.unpack("<" + "I" * (len(raw) // 4), raw))
    raise ValueError(f"unsupported index component type {accessor['componentType']}")


def material_slug(name: str, index: int) -> str:
    slug = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
    return slug or f"mat_{index}"


def build_static_gltf(src: Path, dst: Path, material_filter: int | None = None) -> bool:
    doc, blob = read_glb(src)
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    uvs: list[tuple[float, float]] = []
    indices: list[int] = []

    mesh_jobs: list[tuple[dict, tuple[list[list[float]], list[float]]]] = []

    def visit_node(node_index: int, parent_transform: tuple[list[list[float]], list[float]]) -> None:
        node = doc["nodes"][node_index]
        world = compose_transform(parent_transform, node_transform(node))
        if "mesh" in node:
            mesh_jobs.append((doc["meshes"][node["mesh"]], world))
        for child in node.get("children", []):
            visit_node(child, world)

    for scene in doc.get("scenes", []):
        for node_index in scene.get("nodes", []):
            visit_node(node_index, identity_transform())
    if not mesh_jobs:
        mesh_jobs = [(mesh, identity_transform()) for mesh in doc.get("meshes", [])]

    for mesh, transform in mesh_jobs:
        for primitive in mesh.get("primitives", []):
            if material_filter is not None and primitive.get("material") != material_filter:
                continue
            attrs = primitive.get("attributes", {})
            if not {"POSITION", "NORMAL", "TEXCOORD_0"}.issubset(attrs):
                continue
            base = len(positions)
            pos = accessor_floats(doc, blob, attrs["POSITION"], 3)
            nrm = accessor_floats(doc, blob, attrs["NORMAL"], 3)
            uv = accessor_floats(doc, blob, attrs["TEXCOORD_0"], 2)
            positions.extend(transform_position(transform, p) for p in pos)
            normals.extend(transform_normal(transform, n) for n in nrm)
            uvs.extend((float(x), float(y)) for x, y in uv)
            if "indices" in primitive:
                indices.extend(base + i for i in accessor_indices(doc, blob, primitive["indices"]))
            else:
                indices.extend(range(base, base + len(pos)))

    if not positions:
        return False
    if len(positions) != len(normals) or len(positions) != len(uvs):
        raise ValueError("static extraction produced invalid stream lengths")

    index_component = 5123 if len(positions) < 65535 else 5125
    index_fmt = "H" if index_component == 5123 else "I"
    out = b"".join(struct.pack("<3f", *p) for p in positions)
    normal_offset = len(out)
    out += b"".join(struct.pack("<3f", *n) for n in normals)
    uv_offset = len(out)
    out += b"".join(struct.pack("<2f", *uv) for uv in uvs)
    index_offset = len(out)
    out += b"".join(struct.pack("<" + index_fmt, i) for i in indices)

    mins = [min(p[i] for p in positions) for i in range(3)]
    maxs = [max(p[i] for p in positions) for i in range(3)]
    gltf = {
        "asset": {
            "version": "2.0",
            "generator": "game-67-idle static GLB extractor",
            "extras": {"source": src.as_posix()},
        },
        "buffers": [
            {
                "uri": "data:application/octet-stream;base64,"
                + base64.b64encode(out).decode("ascii"),
                "byteLength": len(out),
            }
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": normal_offset, "target": 34962},
            {"buffer": 0, "byteOffset": normal_offset, "byteLength": uv_offset - normal_offset, "target": 34962},
            {"buffer": 0, "byteOffset": uv_offset, "byteLength": index_offset - uv_offset, "target": 34962},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(out) - index_offset, "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": mins, "max": maxs},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
            {"bufferView": 3, "componentType": index_component, "count": len(indices), "type": "SCALAR"},
        ],
        "meshes": [
            {"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "mode": 4}]}
        ],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    dst.write_text(json.dumps(gltf, separators=(",", ":")), encoding="utf-8")
    return True


def split_materials(src: Path, dst_dir: Path, prefix: str) -> None:
    doc, _blob = read_glb(src)
    dst_dir.mkdir(parents=True, exist_ok=True)
    written = 0
    for index, material in enumerate(doc.get("materials", [])):
        name = str(material.get("name") or f"mat_{index}")
        dst = dst_dir / f"{prefix}_{material_slug(name, index)}_static_cc0.gltf"
        if build_static_gltf(src, dst, material_filter=index):
            print(dst.as_posix())
            written += 1
    if written == 0:
        raise ValueError("split material extraction produced no meshes")


def main(argv: list[str]) -> int:
    if len(argv) == 5 and argv[1] == "--split-materials":
        split_materials(Path(argv[2]), Path(argv[3]), argv[4])
        return 0
    if len(argv) != 3:
        print(
            "usage: extract_static_gltf.py <input.glb> <output.gltf>\n"
            "   or: extract_static_gltf.py --split-materials <input.glb> <output-dir> <prefix>",
            file=sys.stderr,
        )
        return 2
    if not build_static_gltf(Path(argv[1]), Path(argv[2])):
        raise ValueError("static extraction produced no meshes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
