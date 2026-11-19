"""Binary glTF read/write, scene geometry, skins and clip sampling without Blender.

Pure stdlib + numpy, so checks and repack tools run in plain Python and inside Blender alike.
Space is glTF's (Y up, metres); nothing here converts axes.
"""
import hashlib
import json
import struct
from pathlib import Path

import numpy as np

MAGIC = 0x46546C67
JSON_CHUNK = 0x4E4F534A
BINARY_CHUNK = 0x004E4942

FLOAT = 5126
UNSIGNED_BYTE = 5121
UNSIGNED_SHORT = 5123
UNSIGNED_INT = 5125
COMPONENT_FORMAT = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
COMPONENT_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}
NORMALIZED_DIVISOR = {5120: 127.0, 5121: 255.0, 5122: 32767.0, 5123: 65535.0}
ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963

TRIANGLES = 4
COMPRESSED = {"KHR_draco_mesh_compression", "EXT_meshopt_compression", "KHR_meshopt_compression"}


class GlbError(ValueError):
    pass


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def parse_glb(raw, label="glb"):
    """(document, binary) of a GLB byte string."""
    if len(raw) < 20:
        raise GlbError(f"{label}: too short for binary glTF")
    magic, version, _ = struct.unpack_from("<III", raw, 0)
    if magic != MAGIC or version != 2:
        raise GlbError(f"{label}: not binary glTF 2.0")
    json_length, json_type = struct.unpack_from("<II", raw, 12)
    if json_type != JSON_CHUNK:
        raise GlbError(f"{label}: first chunk is not JSON")
    document = json.loads(raw[20:20 + json_length])
    offset = 20 + json_length
    binary = b""
    if offset + 8 <= len(raw):
        binary_length, binary_type = struct.unpack_from("<II", raw, offset)
        if binary_type == BINARY_CHUNK:
            binary = bytes(raw[offset + 8:offset + 8 + binary_length])
    return document, binary


def encode_glb(document, binary):
    if binary and document.get("buffers"):
        document["buffers"][0]["byteLength"] = len(binary)
    text = json.dumps(document, separators=(",", ":")).encode("utf-8")
    text += b" " * (-len(text) % 4)
    payload = bytes(binary) + b"\0" * (-len(binary) % 4)
    out = struct.pack("<III", MAGIC, 2, 12 + 8 + len(text) + (8 + len(payload) if payload else 0))
    out += struct.pack("<II", len(text), JSON_CHUNK) + text
    if payload:
        out += struct.pack("<II", len(payload), BINARY_CHUNK) + payload
    return out


def write_glb(path, document, binary):
    """Atomic write: a reader never sees a half-written asset."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    pending = path.with_name(path.name + ".pending")
    pending.write_bytes(encode_glb(document, binary))
    pending.replace(path)


# --- math -------------------------------------------------------------------------------------

def quat_matrix(q):
    """3x3 rotation of a glTF (x, y, z, w) quaternion; normalises, since files drift off unit."""
    q = np.asarray(q, float)
    n = np.linalg.norm(q)
    if n < 1e-12:
        return np.eye(3)
    x, y, z, w = q / n
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def trs_matrix(t, r, s):
    m = np.eye(4)
    m[:3, :3] = quat_matrix(r) * np.asarray(s, float)
    m[:3, 3] = t
    return m


def quat_angle_deg(a, b):
    a = np.asarray(a, float) / np.linalg.norm(a)
    b = np.asarray(b, float) / np.linalg.norm(b)
    return float(np.degrees(2 * np.arccos(min(1.0, abs(float(np.dot(a, b)))))))


def transform_points(matrix, points):
    points = np.asarray(points, float)
    return points @ matrix[:3, :3].T + matrix[:3, 3]


def transform_normals(matrix, normals):
    """Inverse-transpose so non-uniform scale keeps normals perpendicular to the surface."""
    normal_matrix = np.linalg.inv(matrix[:3, :3]).T
    out = np.asarray(normals, float) @ normal_matrix.T
    lengths = np.linalg.norm(out, axis=1, keepdims=True)
    return out / np.where(lengths < 1e-12, 1.0, lengths)


def sample_channel(times, values, t, path, interpolation="LINEAR"):
    """Sample one glTF channel the way an engine does: LINEAR (slerp-free nlerp for rotations),
    STEP, or CUBICSPLINE (values stored as in-tangent, value, out-tangent triplets)."""
    times = np.asarray(times, float)
    rotation = path == "rotation"
    cubic = interpolation == "CUBICSPLINE"
    keys = values.reshape(len(times), 3, -1)[:, 1] if cubic else values
    if t <= times[0] or len(times) == 1:
        out = keys[0]
    elif t >= times[-1]:
        out = keys[-1]
    else:
        k = int(np.searchsorted(times, t, side="right")) - 1
        dt = max(1e-12, times[k + 1] - times[k])
        u = (t - times[k]) / dt
        if interpolation == "STEP":
            out = keys[k]
        elif cubic:
            triplets = values.reshape(len(times), 3, -1)
            p0, m0 = triplets[k, 1], triplets[k, 2] * dt
            p1, m1 = triplets[k + 1, 1], triplets[k + 1, 0] * dt
            u2, u3 = u * u, u * u * u
            out = ((2 * u3 - 3 * u2 + 1) * p0 + (u3 - 2 * u2 + u) * m0
                   + (-2 * u3 + 3 * u2) * p1 + (u3 - u2) * m1)
        else:
            a, b = keys[k], keys[k + 1]
            if rotation and np.dot(a, b) < 0:
                b = -b
            out = a + (b - a) * u
    if rotation:
        n = np.linalg.norm(out)
        out = out / n if n > 1e-12 else np.array([0.0, 0.0, 0.0, 1.0])
    return np.asarray(out, float)


# --- reader -----------------------------------------------------------------------------------

class Part:
    """One mesh primitive placed in the scene: world-space triangles plus the attributes it has."""

    def __init__(self, name, node, mesh, primitive, positions, faces, world, attributes):
        self.name = name
        self.node = node
        self.mesh = mesh
        self.primitive = primitive
        self.positions = positions
        self.faces = faces
        self.world = world
        self.attributes = attributes

    def __repr__(self):
        return f"Part({self.name!r}, verts={len(self.positions)}, tris={len(self.faces)})"


class Glb:
    def __init__(self, document, binary=b"", path=None):
        self.json = document
        self.bin = binary
        self.path = Path(path) if path else None
        self.nodes = document.get("nodes", [])
        self.parent = {c: i for i, n in enumerate(self.nodes) for c in n.get("children", [])}
        self.by_name = {}
        for i, n in enumerate(self.nodes):
            self.by_name.setdefault(n.get("name"), i)
        compressed = sorted(COMPRESSED & set(document.get("extensionsUsed", []) + document.get("extensionsRequired", [])))
        if compressed:
            # Their accessors have no plain bufferView, so reading them would silently yield zeros.
            raise GlbError(f"{self.label}: compressed geometry ({', '.join(compressed)}) is not supported; "
                           "re-export without compression")

    @classmethod
    def load(cls, path):
        path = Path(path)
        document, binary = parse_glb(path.read_bytes(), str(path))
        return cls(document, binary, path)

    @property
    def label(self):
        return self.path.as_posix() if self.path else "glb"

    # accessors -------------------------------------------------------------------------------
    def accessor(self, index, normalize=True):
        """(count, width) array: float64 for float or normalised data, int64 otherwise."""
        acc = self.json["accessors"][index]
        fmt = COMPONENT_FORMAT[acc["componentType"]]
        width = COMPONENT_COUNT[acc["type"]]
        count = acc["count"]
        if "bufferView" in acc:
            view = self.json["bufferViews"][acc["bufferView"]]
            item = np.dtype("<" + fmt).itemsize
            stride = view.get("byteStride") or item * width
            start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
            end = start + stride * (count - 1) + item * width if count else start
            if end > len(self.bin):
                raise GlbError(f"{self.label}: accessor {index} reads past the binary chunk")
            if stride == item * width:
                out = np.frombuffer(self.bin, "<" + fmt, count * width, start).reshape(count, width)
            else:
                raw = np.frombuffer(self.bin, np.uint8, stride * (count - 1) + item * width, start)
                rows = np.lib.stride_tricks.as_strided(raw, (count, item * width), (stride, 1))
                out = np.ascontiguousarray(rows).view("<" + fmt).reshape(count, width)
        else:
            out = np.zeros((count, width), "<" + fmt)
        sparse = acc.get("sparse")
        if sparse:
            out = out.copy()
            idx = sparse["indices"]
            ivals = self._view_array(idx["bufferView"], idx.get("byteOffset", 0),
                                     COMPONENT_FORMAT[idx["componentType"]], sparse["count"], 1)[:, 0]
            vals = sparse["values"]
            out[ivals] = self._view_array(vals["bufferView"], vals.get("byteOffset", 0), fmt,
                                          sparse["count"], width)
        if fmt == "f":
            return out.astype(np.float64)
        if normalize and acc.get("normalized"):
            return np.maximum(out.astype(np.float64) / NORMALIZED_DIVISOR[acc["componentType"]], -1.0)
        return out.astype(np.int64)

    def _view_array(self, view_index, offset, fmt, count, width):
        view = self.json["bufferViews"][view_index]
        start = view.get("byteOffset", 0) + offset
        return np.frombuffer(self.bin, "<" + fmt, count * width, start).reshape(count, width)

    def accessor_info(self, index):
        return self.json["accessors"][index]

    # hierarchy -------------------------------------------------------------------------------
    def name(self, i):
        return self.nodes[i].get("name", f"node{i}")

    def node_index(self, name_or_index):
        if isinstance(name_or_index, int):
            return name_or_index
        if name_or_index not in self.by_name:
            raise GlbError(f"{self.label}: no node named {name_or_index!r}")
        return self.by_name[name_or_index]

    def roots(self, scene=None):
        scenes = self.json.get("scenes")
        if not scenes:
            return [i for i in range(len(self.nodes)) if i not in self.parent]
        return list(scenes[self.json.get("scene", 0) if scene is None else scene].get("nodes", []))

    def local_trs(self, i):
        n = self.nodes[i]
        if "matrix" in n:
            m = np.array(n["matrix"], float).reshape(4, 4).T
            t = m[:3, 3].copy()
            s = np.linalg.norm(m[:3, :3], axis=0)
            if np.linalg.det(m[:3, :3]) < 0:
                s[0] = -s[0]
            r = matrix_to_quat(m[:3, :3] / np.where(np.abs(s) < 1e-12, 1.0, s))
            return t, r, s
        return (np.array(n.get("translation", [0, 0, 0]), float),
                np.array(n.get("rotation", [0, 0, 0, 1]), float),
                np.array(n.get("scale", [1, 1, 1]), float))

    def local_matrix(self, i):
        n = self.nodes[i]
        if "matrix" in n:
            return np.array(n["matrix"], float).reshape(4, 4).T
        return trs_matrix(*self.local_trs(i))

    def walk(self, local, scene=None):
        """{node: world 4x4} given a callable node -> local 4x4."""
        out = {}
        stack = [(r, np.eye(4)) for r in self.roots(scene)]
        while stack:
            i, parent = stack.pop()
            out[i] = parent @ local(i)
            stack.extend((c, out[i]) for c in self.nodes[i].get("children", []))
        return out

    def world_rest(self, scene=None):
        return self.walk(self.local_matrix, scene)

    # geometry --------------------------------------------------------------------------------
    def primitive_faces(self, primitive):
        mode = primitive.get("mode", TRIANGLES)
        if "indices" in primitive:
            flat = self.accessor(primitive["indices"], normalize=False)[:, 0]
        else:
            flat = np.arange(self.accessor_info(primitive["attributes"]["POSITION"])["count"])
        if mode == TRIANGLES:
            if len(flat) % 3:
                raise GlbError(f"{self.label}: triangle index count {len(flat)} is not a multiple of 3")
            return flat.reshape(-1, 3)
        if mode == 5:
            tris = [(flat[k], flat[k + 1], flat[k + 2]) if k % 2 == 0 else (flat[k + 1], flat[k], flat[k + 2])
                    for k in range(len(flat) - 2)]
            return np.array(tris, np.int64).reshape(-1, 3)
        if mode == 6:
            return np.array([(flat[0], flat[k], flat[k + 1]) for k in range(1, len(flat) - 1)],
                            np.int64).reshape(-1, 3)
        return np.zeros((0, 3), np.int64)

    def parts(self, world=None, include=None):
        """Every mesh primitive in the active scene with world-space positions.

        `world` overrides node matrices (a pose); `include` filters by node name.
        Skinned meshes are skinned by the joints in `world`, as glTF ignores their node transform;
        a skin that cannot be evaluated (joint outside the scene, bad index) falls back to the node.
        """
        world = world if world is not None else self.world_rest()
        out = []
        for i, matrix in world.items():
            node = self.nodes[i]
            if "mesh" not in node:
                continue
            name = self.name(i)
            if include is not None and name not in include:
                continue
            palette = None
            if "skin" in node:
                try:
                    palette = self.skin_palette(world, node["skin"])
                except GlbError:
                    palette = None
            for p, prim in enumerate(self.json["meshes"][node["mesh"]]["primitives"]):
                if "POSITION" not in prim.get("attributes", {}):
                    continue
                positions = None
                if palette is not None:
                    try:
                        positions = self.skin_primitive(palette, prim)
                    except GlbError:
                        positions = None
                placed = positions is not None
                if not placed:
                    positions = transform_points(matrix, self.accessor(prim["attributes"]["POSITION"]))
                out.append(Part(name, i, node["mesh"], p, positions, self.primitive_faces(prim),
                                np.eye(4) if placed else matrix, prim["attributes"]))
        return out

    def merged(self, world=None, include=None):
        """(positions, faces, part index per face) of all parts joined into one soup."""
        parts = self.parts(world, include)
        positions, faces, owner, base = [], [], [], 0
        for k, part in enumerate(parts):
            positions.append(part.positions)
            faces.append(part.faces + base)
            owner.append(np.full(len(part.faces), k))
            base += len(part.positions)
        if not parts:
            return np.zeros((0, 3)), np.zeros((0, 3), np.int64), np.zeros(0, np.int64), parts
        return np.vstack(positions), np.vstack(faces), np.concatenate(owner), parts

    # skins -----------------------------------------------------------------------------------
    def skin_joints(self, skin=0):
        return [self.name(j) for j in self.json["skins"][skin]["joints"]]

    def inverse_binds(self, skin=0):
        s = self.json["skins"][skin]
        if "inverseBindMatrices" not in s:
            return np.repeat(np.eye(4)[None], len(s["joints"]), axis=0)
        return self.accessor(s["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)

    def skin_streams(self, primitive):
        """(joints, weights) concatenated over every JOINTS_n/WEIGHTS_n set: (count, 4*sets)."""
        attrs = primitive["attributes"]
        joints, weights = [], []
        k = 0
        while f"JOINTS_{k}" in attrs and f"WEIGHTS_{k}" in attrs:
            joints.append(self.accessor(attrs[f"JOINTS_{k}"], normalize=False))
            weights.append(self.accessor(attrs[f"WEIGHTS_{k}"]))
            k += 1
        if not joints:
            return None, None
        return np.hstack(joints), np.hstack(weights)

    def skin_palette(self, world, skin=0):
        """Joint world x inverse bind per joint; GlbError when a joint is outside `world`."""
        joints = self.json["skins"][skin]["joints"]
        missing = [self.name(j) if 0 <= j < len(self.nodes) else j for j in joints if j not in world]
        if missing:
            raise GlbError(f"{self.label}: skin {skin} joints outside the scene: {missing[:5]}")
        ibm = self.inverse_binds(skin)
        if len(ibm) < len(joints):
            raise GlbError(f"{self.label}: skin {skin} has {len(ibm)} inverse binds for {len(joints)} joints")
        return np.stack([world[j] @ ibm[p] for p, j in enumerate(joints)])

    def skin_primitive(self, palette, primitive):
        """Linear-blend-skinned positions of one primitive; GlbError on missing streams or bad indices."""
        pos = self.accessor(primitive["attributes"]["POSITION"])
        joints, weights = self.skin_streams(primitive)
        if joints is None:
            raise GlbError(f"{self.label}: skinned primitive has no JOINTS_0/WEIGHTS_0")
        outside = (joints >= len(palette)) | (joints < 0)
        if len(joints) != len(pos) or (outside & (weights > 0)).any():
            raise GlbError(f"{self.label}: joint indices outside the skin's {len(palette)} joints")
        joints = np.where(outside, 0, joints)  # zero-weight padding lanes may hold any index
        hom = np.c_[pos, np.ones(len(pos))]
        res = np.zeros((len(pos), 3))
        for lane in range(joints.shape[1]):
            res += weights[:, lane:lane + 1] * np.einsum("nij,nj->ni", palette[joints[:, lane]], hom)[:, :3]
        return res

    def skinned_positions(self, world, mesh_node):
        """Linear-blend-skinned world positions of one skinned mesh node's primitives."""
        node = self.nodes[self.node_index(mesh_node)]
        palette = self.skin_palette(world, node["skin"])
        return [self.skin_primitive(palette, prim) for prim in self.json["meshes"][node["mesh"]]["primitives"]
                if "POSITION" in prim.get("attributes", {})]

    # animation -------------------------------------------------------------------------------
    def animation_names(self):
        return [a.get("name", f"animation{k}") for k, a in enumerate(self.json.get("animations", []))]

    def animation_index(self, name_or_index):
        if isinstance(name_or_index, int):
            return name_or_index
        names = self.animation_names()
        if name_or_index not in names:
            raise GlbError(f"{self.label}: no animation {name_or_index!r}; have {names}")
        return names.index(name_or_index)

    def channels(self, anim=0, by_index=False):
        """{(node name, path): (times, values, interpolation)} of one animation; keyed by node index
        with `by_index`, which keeps two nodes of the same name apart (use it within one file)."""
        a = self.json["animations"][self.animation_index(anim)]
        out = {}
        for c in a["channels"]:
            target = c["target"]
            if "node" not in target:
                continue
            s = a["samplers"][c["sampler"]]
            key = target["node"] if by_index else self.name(target["node"])
            out[(key, target["path"])] = (
                self.accessor(s["input"])[:, 0], self.accessor(s["output"]), s.get("interpolation", "LINEAR"))
        return out

    def duplicate_names(self, nodes=None):
        """Names carried by more than one node (optionally only among `nodes`)."""
        names = [self.name(i) for i in (range(len(self.nodes)) if nodes is None else nodes)]
        every = [self.name(i) for i in range(len(self.nodes))]
        return sorted({n for n in names if every.count(n) > 1})

    def duration(self, anim=0):
        return max((float(times[-1]) for times, _, _ in self.channels(anim, True).values()), default=0.0)

    def key_times(self, anim=0):
        times = [t for t, _, _ in self.channels(anim, True).values()]
        return np.unique(np.concatenate(times)) if times else np.zeros(0)

    def posed_world(self, channels, t, scene=None):
        """World matrices with `channels` applied at time t; nodes the clip does not animate keep their
        rest. Channels keyed by node index (this file) hit that node; keyed by name (another file)
        they hit the first node of that name only, so duplicated names never move several nodes."""
        def local(i):
            nm = self.name(i)
            first = self.by_name.get(nm) == i
            found = {}
            for path in ("translation", "rotation", "scale"):
                if (i, path) in channels:
                    found[path] = channels[(i, path)]
                elif first and (nm, path) in channels:
                    found[path] = channels[(nm, path)]
            if not found:
                return self.local_matrix(i)
            trs = dict(zip(("translation", "rotation", "scale"), self.local_trs(i)))
            for path, (times, values, interp) in found.items():
                trs[path] = sample_channel(times, values, t, path, interp)
            return trs_matrix(trs["translation"], trs["rotation"], trs["scale"])
        return self.walk(local, scene)


def matrix_to_quat(m):
    """(x, y, z, w) of a pure 3x3 rotation."""
    trace = m[0, 0] + m[1, 1] + m[2, 2]
    if trace > 0:
        s = 2.0 * np.sqrt(trace + 1.0)
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = 2.0 * np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2])
        q = [0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s]
    elif m[1, 1] > m[2, 2]:
        s = 2.0 * np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2])
        q = [(m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s]
    else:
        s = 2.0 * np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1])
        q = [(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s, (m[1, 0] - m[0, 1]) / s]
    q = np.array(q, float)
    return q / np.linalg.norm(q)


# --- writer -----------------------------------------------------------------------------------

class GlbBuilder:
    """Assemble a GLB from numpy arrays: fixtures for tests, repacked or generated assets."""

    def __init__(self, document=None, binary=b""):
        self.json = document or {"asset": {"version": "2.0", "generator": "ai_studio blender lib"},
                                 "scenes": [{"nodes": []}], "scene": 0, "nodes": []}
        self.payload = bytearray(binary)
        self.json.setdefault("buffers", [{"byteLength": 0}])
        if not self.json["buffers"]:
            self.json["buffers"].append({"byteLength": 0})
        for key in ("bufferViews", "accessors", "meshes"):
            self.json.setdefault(key, [])

    def add_accessor(self, array, component=FLOAT, kind=None, target=None, normalized=False, bounds=False):
        array = np.asarray(array)
        if array.ndim == 1:
            array = array[:, None]
        width = array.shape[1]
        kind = kind or {1: "SCALAR", 2: "VEC2", 3: "VEC3", 4: "VEC4", 16: "MAT4"}[width]
        data = np.ascontiguousarray(array.astype("<" + COMPONENT_FORMAT[component])).tobytes()
        while len(self.payload) % 4:
            self.payload.append(0)
        view = {"buffer": 0, "byteOffset": len(self.payload), "byteLength": len(data)}
        if target:
            view["target"] = target
        self.payload.extend(data)
        self.json["bufferViews"].append(view)
        acc = {"bufferView": len(self.json["bufferViews"]) - 1, "componentType": component,
               "count": int(array.shape[0]), "type": kind}
        if normalized:
            acc["normalized"] = True
        if bounds or kind == "VEC3" and target == ARRAY_BUFFER:
            acc["min"] = array.min(axis=0).astype(float).tolist()
            acc["max"] = array.max(axis=0).astype(float).tolist()
        self.json["accessors"].append(acc)
        return len(self.json["accessors"]) - 1

    def add_mesh(self, name, positions, faces, normals=None, colors=None, extra=None):
        attrs = {"POSITION": self.add_accessor(np.asarray(positions, float), target=ARRAY_BUFFER)}
        if normals is not None:
            attrs["NORMAL"] = self.add_accessor(np.asarray(normals, float), target=ARRAY_BUFFER)
        if colors is not None:
            attrs["COLOR_0"] = self.add_accessor(np.asarray(colors, float), target=ARRAY_BUFFER)
        for key, (array, component) in (extra or {}).items():
            attrs[key] = self.add_accessor(array, component, target=ARRAY_BUFFER)
        flat = np.asarray(faces).reshape(-1)
        component = UNSIGNED_SHORT if flat.max(initial=0) < 65535 else UNSIGNED_INT
        indices = self.add_accessor(flat, component, "SCALAR", ELEMENT_ARRAY_BUFFER)
        self.json["meshes"].append({"name": name, "primitives": [
            {"attributes": attrs, "indices": indices, "mode": TRIANGLES}]})
        return len(self.json["meshes"]) - 1

    def add_node(self, name, mesh=None, translation=None, rotation=None, scale=None,
                 children=None, parent=None, root=True, **extra):
        node = {"name": name}
        if mesh is not None:
            node["mesh"] = mesh
        for key, value in (("translation", translation), ("rotation", rotation), ("scale", scale)):
            if value is not None:
                node[key] = [float(v) for v in value]
        if children:
            node["children"] = list(children)
        node.update(extra)
        self.json["nodes"].append(node)
        index = len(self.json["nodes"]) - 1
        if parent is not None:
            self.json["nodes"][parent].setdefault("children", []).append(index)
        elif root:
            self.json["scenes"][self.json.get("scene", 0)]["nodes"].append(index)
        return index

    def to_glb(self):
        self.json["buffers"][0]["byteLength"] = len(self.payload)
        return Glb(json.loads(json.dumps(self.json)), bytes(self.payload))

    def write(self, path):
        self.json["buffers"][0]["byteLength"] = len(self.payload)
        write_glb(path, self.json, bytes(self.payload))
        return Path(path)
