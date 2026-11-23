"""Read and rewrite one-buffer binary glTF files in place: accessors, rest transforms, skin joints.

Writes keep every byte they do not own. An attribute of the same size is overwritten in its own buffer
view, a new one is appended at the end of the binary chunk, and the JSON is written compactly, so a
second identical write reproduces the file byte for byte.
"""
import json
import struct
from pathlib import Path

import numpy as np

_COMPONENTS = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}
_WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
_NORMALIZED = {5121: 255.0, 5123: 65535.0}
_JSON, _BIN = 0x4E4F534A, 0x004E4942


def _quat_matrix(q):
    x, y, z, w = np.asarray(q, float) / np.linalg.norm(q)
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


class Glb:
    def __init__(self, source):
        raw = Path(source).read_bytes() if not isinstance(source, (bytes, bytearray)) else bytes(source)
        self.path = None if isinstance(source, (bytes, bytearray)) else Path(source)
        magic, version, total = struct.unpack_from("<III", raw, 0)
        size, kind = struct.unpack_from("<II", raw, 12)
        if magic != 0x46546C67 or version != 2 or total != len(raw) or kind != _JSON:
            raise ValueError(f"{self.path}: not a binary glTF 2 file")
        self.json = json.loads(raw[20:20 + size])
        offset = 20 + size
        self.bin = bytearray()
        if offset < total:
            bsize, bkind = struct.unpack_from("<II", raw, offset)
            if bkind != _BIN:
                raise ValueError(f"{self.path}: second chunk is not BIN")
            self.bin = bytearray(raw[offset + 8:offset + 8 + bsize])
        buffers = self.json.get("buffers", [])
        if len(buffers) > 1 or (buffers and "uri" in buffers[0]):
            raise ValueError(f"{self.path}: expected one embedded buffer")
        if buffers:
            del self.bin[buffers[0]["byteLength"]:]
        self.nodes = self.json.get("nodes", [])
        self.parent = {c: i for i, n in enumerate(self.nodes) for c in n.get("children", [])}
        self.by_name = {n.get("name"): i for i, n in enumerate(self.nodes)}

    # Reading -------------------------------------------------------------------------------------
    def _layout(self, index):
        acc = self.json["accessors"][index]
        view = self.json["bufferViews"][acc["bufferView"]]
        dtype = np.dtype(_COMPONENTS[acc["componentType"]])
        width = _WIDTH[acc["type"]]
        if view.get("byteStride", dtype.itemsize * width) != dtype.itemsize * width or "sparse" in acc:
            raise ValueError(f"{self.path}: accessor {index} is interleaved or sparse")
        return acc, dtype, width, view.get("byteOffset", 0) + acc.get("byteOffset", 0)

    def accessor(self, index, normalize=True):
        acc, dtype, width, start = self._layout(index)
        out = np.frombuffer(self.bin, dtype, acc["count"] * width, start).reshape(acc["count"], width)
        out = out.astype(np.float64) if dtype.kind == "f" else out.astype(np.int64)
        if normalize and acc.get("normalized") and acc["componentType"] in _NORMALIZED:
            out = out / _NORMALIZED[acc["componentType"]]
        return out

    def name(self, i):
        return self.nodes[i].get("name")

    def skin_joints(self):
        return [self.name(j) for j in self.json["skins"][0]["joints"]]

    def primitives(self):
        return self.json["meshes"][0]["primitives"]

    def material(self, primitive):
        return self.json["materials"][primitive["material"]]["name"]

    def world_rest(self):
        """Model-space 4x4 rest matrix of every node."""
        out = {}

        def visit(i, m):
            n = self.nodes[i]
            if "matrix" in n:
                local = np.asarray(n["matrix"], float).reshape(4, 4).T
            else:
                local = np.eye(4)
                local[:3, :3] = _quat_matrix(n.get("rotation", [0, 0, 0, 1])) * np.asarray(n.get("scale", [1, 1, 1]), float)
                local[:3, 3] = n.get("translation", [0, 0, 0])
            out[i] = m @ local
            for c in n.get("children", []):
                visit(c, out[i])

        for root in self.json["scenes"][self.json.get("scene", 0)]["nodes"]:
            visit(root, np.eye(4))
        return out

    def skin_weights(self, primitive, joint_count):
        """Dense (vertices, joints) weights of one primitive."""
        a = primitive["attributes"]
        n = self.json["accessors"][a["POSITION"]]["count"]
        w = np.zeros((n, joint_count))
        layer = 0
        while f"JOINTS_{layer}" in a:
            np.add.at(w, (np.arange(n)[:, None], self.accessor(a[f"JOINTS_{layer}"], False)),
                      self.accessor(a[f"WEIGHTS_{layer}"]))
            layer += 1
        return w

    # Writing -------------------------------------------------------------------------------------
    def overwrite(self, index, values):
        """Replace an accessor's values in its own bytes; the count and component type stay."""
        acc, dtype, width, start = self._layout(index)
        data = np.ascontiguousarray(np.asarray(values).reshape(acc["count"], width), dtype=dtype).tobytes()
        self.bin[start:start + len(data)] = data

    def set_attribute(self, primitive, name, values, component_type=5126):
        """Overwrite an existing vertex attribute or append a new one at the end of the binary chunk."""
        values = np.asarray(values)
        if name in primitive["attributes"]:
            self.overwrite(primitive["attributes"][name], values)
            return
        count = self.json["accessors"][primitive["attributes"]["POSITION"]]["count"]
        width = 1 if values.ndim == 1 else values.shape[1]
        data = np.ascontiguousarray(values.reshape(count, width), dtype=np.dtype(_COMPONENTS[component_type])).tobytes()
        self.bin.extend(b"\x00" * (-len(self.bin) % 4))
        views = self.json["bufferViews"]
        views.append({"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(data), "target": 34962})
        primitive["attributes"][name] = len(self.json["accessors"])
        self.json["accessors"].append({"bufferView": len(views) - 1, "componentType": component_type, "count": count,
                                       "type": {1: "SCALAR", 2: "VEC2", 3: "VEC3", 4: "VEC4"}[width]})
        self.bin.extend(data)

    def truncate_skin(self, keep):
        """Drop the joints after the first `keep` from the skin and delete their nodes, which must be leaves of
        the kept hierarchy listed after every other node. The dropped inverse bind matrices stay as unused bytes."""
        skin = self.json["skins"][0]
        dropped = skin["joints"][keep:]
        if not dropped:
            return
        first = len(self.nodes) - len(dropped)
        if sorted(dropped) != list(range(first, len(self.nodes))):
            raise ValueError(f"{self.path}: dropped joints must be the last nodes")
        gone = set(dropped)
        for i, node in enumerate(self.nodes[:first]):
            if "children" in node:
                node["children"] = [c for c in node["children"] if c not in gone]
                if not node["children"]:
                    del node["children"]
        for node in self.nodes[first:]:
            if "mesh" in node or "skin" in node or any(c < first for c in node.get("children", [])):
                raise ValueError(f"{self.path}: a dropped joint holds a kept node")
        del self.nodes[first:]
        skin["joints"] = skin["joints"][:keep]
        ibm = self.json["accessors"][skin["inverseBindMatrices"]]
        ibm["count"] = keep
        shared = [a for a in self.json["accessors"] if a.get("bufferView") == ibm["bufferView"]]
        if len(shared) == 1 and ibm.get("byteOffset", 0) == 0:
            self.json["bufferViews"][ibm["bufferView"]]["byteLength"] = 64 * keep
        self.parent = {c: i for i, n in enumerate(self.nodes) for c in n.get("children", [])}
        self.by_name = {n.get("name"): i for i, n in enumerate(self.nodes)}

    def to_bytes(self):
        data = bytes(self.bin) + b"\x00" * (-len(self.bin) % 4)
        if self.json.get("buffers"):
            self.json["buffers"][0]["byteLength"] = len(self.bin)
        text = json.dumps(self.json, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        text += b" " * (-len(text) % 4)
        chunks = struct.pack("<I4s", len(text), b"JSON") + text
        if data:
            chunks += struct.pack("<I4s", len(data), b"BIN\x00") + data
        return struct.pack("<4sII", b"glTF", 2, 12 + len(chunks)) + chunks
