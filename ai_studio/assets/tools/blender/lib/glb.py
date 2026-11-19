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

def quat_matrices(q):
    """(n, 3, 3) rotations of (n, 4) glTF (x, y, z, w) quaternions; normalises, since files drift off unit."""
    q = np.asarray(q, float).reshape(-1, 4)
    n = np.linalg.norm(q, axis=1)
    unit = n >= 1e-12
    x, y, z, w = (q / np.where(unit, n, 1.0)[:, None]).T
    m = np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
                  2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                  2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)], axis=1).reshape(-1, 3, 3)
    m[~unit] = np.eye(3)
    return m


def quat_matrix(q):
    return quat_matrices(q)[0]


def trs_matrices(t, r, s):
    """(n, 4, 4) from per-row translation, rotation and scale; a single (3,) or (4,) value broadcasts."""
    t, r, s = (np.atleast_2d(np.asarray(v, float)) for v in (t, r, s))
    m = np.zeros((max(len(t), len(r), len(s)), 4, 4))
    m[:, :3, :3] = quat_matrices(r) * s[:, None, :]
    m[:, :3, 3] = t
    m[:, 3, 3] = 1.0
    return m


def trs_matrix(t, r, s):
    return trs_matrices(t, r, s)[0]


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
    return sample_channel_at(times, values, [t], path, interpolation)[0]


def sample_channel_at(times, values, ts, path, interpolation="LINEAR"):
    """sample_channel at every time in `ts`: (len(ts), width)."""
    times = np.asarray(times, float)
    ts = np.asarray(ts, float).reshape(-1)
    rotation = path == "rotation"
    cubic = interpolation == "CUBICSPLINE"
    n = len(times)
    triplets = values.reshape(n, 3, -1) if cubic else None
    keys = triplets[:, 1] if cubic else values
    out = np.empty((len(ts), keys.shape[1]))
    if n == 1:
        out[:] = keys[0]
    else:
        k = np.clip(np.searchsorted(times, ts, side="right") - 1, 0, n - 2)
        dt = np.maximum(1e-12, times[k + 1] - times[k])
        u = ((ts - times[k]) / dt)[:, None]
        if interpolation == "STEP":
            out[:] = keys[k]
        elif cubic:
            p0, m0 = triplets[k, 1], triplets[k, 2] * dt[:, None]
            p1, m1 = triplets[k + 1, 1], triplets[k + 1, 0] * dt[:, None]
            u2, u3 = u * u, u * u * u
            out[:] = ((2 * u3 - 3 * u2 + 1) * p0 + (u3 - 2 * u2 + u) * m0
                      + (-2 * u3 + 3 * u2) * p1 + (u3 - u2) * m1)
        else:
            a, b = keys[k], keys[k + 1]
            if rotation:
                b = np.where((a * b).sum(axis=1, keepdims=True) < 0, -b, b)
            out[:] = a + (b - a) * u
        out[ts >= times[-1]] = keys[-1]
        out[ts <= times[0]] = keys[0]
    if rotation:
        length = np.linalg.norm(out, axis=1)
        unit = length > 1e-12
        out[unit] /= length[unit, None]
        out[~unit] = (0.0, 0.0, 0.0, 1.0)
    return out


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
        # Decoded data keyed by what produced it; the document and binary are treated as immutable.
        self._cache = {}
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

    def _cached(self, key, build):
        """Read-only result of build(), computed once per key: a caller that writes into it fails loudly
        instead of corrupting every later reader."""
        if key not in self._cache:
            value = build()
            for array in value if isinstance(value, tuple) else (value,):
                if isinstance(array, np.ndarray):
                    array.flags.writeable = False
            self._cache[key] = value
        return self._cache[key]

    # accessors -------------------------------------------------------------------------------
    def accessor(self, index, normalize=True):
        """(count, width) array: float64 for float or normalised data, int64 otherwise."""
        return self._accessor(index, normalize).copy()

    def _accessor(self, index, normalize=True):
        return self._cached(("accessor", index, bool(normalize)), lambda: self._decode(index, normalize))

    def _decode(self, index, normalize):
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
        return {i: m.copy() for i, m in self._world_rest(scene).items()}

    def _world_rest(self, scene=None):
        key = ("world_rest", scene)
        if key not in self._cache:
            world = self.walk(self.local_matrix, scene)
            for m in world.values():
                m.flags.writeable = False
            self._cache[key] = world
        return self._cache[key]

    # geometry --------------------------------------------------------------------------------
    def primitive_faces(self, primitive):
        mode = primitive.get("mode", TRIANGLES)
        if "indices" in primitive:
            flat = self._accessor(primitive["indices"], normalize=False)[:, 0]
        else:
            flat = np.arange(self.accessor_info(primitive["attributes"]["POSITION"])["count"])
        if mode == TRIANGLES:
            if len(flat) % 3:
                raise GlbError(f"{self.label}: triangle index count {len(flat)} is not a multiple of 3")
            return flat.reshape(-1, 3).copy()
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
        world = world if world is not None else self._world_rest()
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
                    positions = transform_points(matrix, self._accessor(prim["attributes"]["POSITION"]))
                out.append(Part(name, i, node["mesh"], p, positions, self.primitive_faces(prim),
                                np.eye(4) if placed else np.array(matrix), prim["attributes"]))
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
        """Read-only (joints, 4, 4)."""
        s = self.json["skins"][skin]
        if "inverseBindMatrices" not in s:
            return self._cached(("identity_binds", len(s["joints"])),
                                lambda: np.repeat(np.eye(4)[None], len(s["joints"]), axis=0))
        return self._accessor(s["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)

    def skin_streams(self, primitive):
        """Read-only (joints, weights) concatenated over every JOINTS_n/WEIGHTS_n set: (count, 4*sets)."""
        attrs = primitive["attributes"]
        sets = []
        while f"JOINTS_{len(sets)}" in attrs and f"WEIGHTS_{len(sets)}" in attrs:
            sets.append((attrs[f"JOINTS_{len(sets)}"], attrs[f"WEIGHTS_{len(sets)}"]))
        if not sets:
            return None, None
        return self._cached(("skin_streams", tuple(sets)), lambda: (
            np.hstack([self._accessor(j, normalize=False) for j, _ in sets]),
            np.hstack([self._accessor(w) for _, w in sets])))

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
        pos, joints, weights = self._skin_inputs(primitive, len(palette))
        hom = np.c_[pos, np.ones(len(pos))]
        res = np.zeros((len(pos), 3))
        for lane in range(joints.shape[1]):
            res += weights[:, lane:lane + 1] * np.einsum("nij,nj->ni", palette[joints[:, lane]], hom)[:, :3]
        return res

    def _skin_inputs(self, primitive, joint_count):
        """(positions, joint lanes, weights) of a skinned primitive, padding lanes clamped to joint 0."""
        pos = self._accessor(primitive["attributes"]["POSITION"])
        joints, weights = self.skin_streams(primitive)
        if joints is None:
            raise GlbError(f"{self.label}: skinned primitive has no JOINTS_0/WEIGHTS_0")
        outside = (joints >= joint_count) | (joints < 0)
        if len(joints) != len(pos) or (outside & (weights > 0)).any():
            raise GlbError(f"{self.label}: joint indices outside the skin's {joint_count} joints")
        return pos, np.where(outside, 0, joints), weights  # zero-weight padding lanes may hold any index

    def skinned_positions(self, world, mesh_node):
        """Linear-blend-skinned world positions of one skinned mesh node's primitives."""
        node = self.nodes[self.node_index(mesh_node)]
        palette = self.skin_palette(world, node["skin"])
        return [self.skin_primitive(palette, prim) for prim in self.json["meshes"][node["mesh"]]["primitives"]
                if "POSITION" in prim.get("attributes", {})]

    def lowest_skinned_y(self, worlds, mesh_nodes):
        """Lowest skinned-vertex height per pose of `worlds` ({node: (poses, 4, 4)}, see posed_worlds)
        over the skinned `mesh_nodes`: (poses,) array, or None when they hold no vertices.

        Exact, but only the vertices that can be lowest get skinned. Each joint's vertices are kept in
        small bind-space boxes; a box whose posed bottom (through that joint) is above the pose's
        lowest point so far cannot hold the minimum, because a blended vertex is never lower than
        its lowest influence scaled by its weight sum. GlbError when a skin cannot be evaluated."""
        soup = self._skin_soup(tuple(self.node_index(n) for n in mesh_nodes))
        if soup is None:
            return None
        slots, binds, pos, lanes, weights, boxes = soup
        missing = [self.name(j) for j in slots if j not in worlds]
        if missing:
            raise GlbError(f"{self.label}: skin joints outside the scene: {missing[:5]}")
        rows = np.einsum("fsk,skm->fsm", np.stack([worlds[j][:, 1, :] for j in slots], axis=1), binds)
        hom = np.c_[pos, np.ones(len(pos))]

        def heights(row, idx):
            y = np.zeros(len(idx))
            for lane in range(lanes.shape[1]):
                y += weights[idx, lane] * np.einsum("nj,nj->n", row[lanes[idx, lane]], hom[idx])
            return y

        out = np.empty(len(rows))
        if boxes is None:
            every = np.arange(len(pos))
            for f, row in enumerate(rows):
                out[f] = heights(row, every).min()
            return out
        slot, lo, hi, members, owner, start, floor, low_sum, high_sum = boxes
        lin = rows[:, slot]
        bottom = lin[..., 3] + np.minimum(lin[..., :3] * lo, lin[..., :3] * hi).sum(axis=-1)
        bottom = np.minimum(low_sum * bottom, high_sum * bottom)
        for f, row in enumerate(rows):
            seed = int(np.argmin(bottom[f]))
            best = min(floor, float(heights(row, members[start[seed]:start[seed + 1]]).min()))
            # Rounding slack: bounds and heights are computed in different orders.
            near = members[(bottom[f] <= best + 1e-6 * (1.0 + abs(best)))[owner]]
            # No box under `best` means it came from an unweighted vertex: nothing weighted is lower.
            out[f] = min(floor, float(heights(row, near).min())) if len(near) else best
        return out

    def skin_vertex_joints(self, mesh_nodes):
        """Joint node carrying the largest weight of each vertex of the skinned `mesh_nodes`, in the
        vertex order skinned_points uses; None without vertices."""
        soup = self._skin_soup(tuple(self.node_index(n) for n in mesh_nodes))
        if soup is None:
            return None
        slots, _, _, lanes, weights, _ = soup
        return np.asarray(slots)[lanes[np.arange(len(lanes)), np.argmax(weights, axis=1)]]

    def skinned_points(self, worlds, mesh_nodes, index):
        """(poses, len(index), 3) skinned world positions of the chosen vertices (see skin_vertex_joints)
        in every pose of `worlds` ({node: (poses, 4, 4)})."""
        slots, binds, pos, lanes, weights, _ = self._skin_soup(tuple(self.node_index(n) for n in mesh_nodes))
        index = np.asarray(index, np.int64)
        palette = np.stack([worlds[j] for j in slots], axis=1) @ binds
        hom = np.c_[pos[index], np.ones(len(index))]
        out = np.zeros((len(palette), len(index), 3))
        for lane in range(lanes.shape[1]):
            out += weights[index, lane][None, :, None] * np.einsum(
                "fnij,nj->fni", palette[:, lanes[index, lane]], hom)[..., :3]
        return out

    def _skin_soup(self, mesh_nodes):
        """Vertices of skinned mesh nodes in one array, their joint lanes pointing into one list of
        (joint node, inverse bind) slots, plus the bind-space boxes lowest_skinned_y prunes with."""
        key = ("skin_soup", mesh_nodes)
        if key in self._cache:
            return self._cache[key]
        slots, binds, base_of, parts = [], [], {}, []
        for i in mesh_nodes:
            node = self.nodes[i]
            skin = node["skin"]
            joints = self.json["skins"][skin]["joints"]
            if skin not in base_of:
                ibm = self.inverse_binds(skin)
                if len(ibm) < len(joints):
                    raise GlbError(f"{self.label}: skin {skin} has {len(ibm)} inverse binds for {len(joints)} joints")
                base_of[skin] = len(slots)
                slots.extend(joints)
                binds.extend(ibm[:len(joints)])
            for prim in self.json["meshes"][node["mesh"]]["primitives"]:
                if "POSITION" in prim.get("attributes", {}):
                    pos, lanes, weights = self._skin_inputs(prim, len(joints))
                    parts.append((pos, lanes + base_of[skin], weights))
        parts = [p for p in parts if len(p[0])]
        if not parts:
            self._cache[key] = None
            return None
        width = max(p[1].shape[1] for p in parts)
        pos = np.vstack([p[0] for p in parts])
        lanes = np.vstack([np.pad(p[1], ((0, 0), (0, width - p[1].shape[1]))) for p in parts])
        weights = np.vstack([np.pad(p[2], ((0, 0), (0, width - p[2].shape[1]))) for p in parts])
        soup = (slots, np.array(binds), pos, lanes, weights, self._skin_boxes(pos, lanes, weights))
        self._cache[key] = soup
        return soup

    @staticmethod
    def _skin_boxes(pos, lanes, weights, size=128):
        """(slot, lo, hi, members, owner, start, floor, low_sum, high_sum): boxes of at most `size`
        vertices per joint slot, split along their longest side. None when weights are negative,
        which breaks the lower bound."""
        if (weights < 0).any():
            return None
        total = weights.sum(axis=1)
        live = total > 0
        # A vertex without weight skins to the origin: its height is exactly 0.
        floor = 0.0 if not live.all() else np.inf
        used = (weights > 0) & live[:, None]
        vertex = np.broadcast_to(np.arange(len(pos))[:, None], lanes.shape)[used]
        slot_of = lanes[used]
        if not len(slot_of):
            return None
        order = np.argsort(slot_of, kind="stable")
        vertex, slot_of = vertex[order], slot_of[order]
        cuts = np.r_[0, np.flatnonzero(np.diff(slot_of)) + 1, len(slot_of)]
        slot, lo, hi, members = [], [], [], []
        for a, b in zip(cuts[:-1], cuts[1:]):
            stack = [vertex[a:b]]
            while stack:
                group = stack.pop()
                pts = pos[group]
                low, high = pts.min(axis=0), pts.max(axis=0)
                if len(group) <= size:
                    slot.append(slot_of[a])
                    lo.append(low)
                    hi.append(high)
                    members.append(group)
                    continue
                half = len(group) // 2
                split = np.argpartition(pts[:, int(np.argmax(high - low))], half)
                stack += [group[split[:half]], group[split[half:]]]
        if not members:
            return None
        sizes = np.array([len(m) for m in members])
        start = np.r_[0, np.cumsum(sizes)]
        return (np.array(slot), np.array(lo), np.array(hi), np.concatenate(members),
                np.repeat(np.arange(len(members)), sizes), start, floor,
                float(total[live].min()), float(total[live].max()))

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
        with `by_index`, which keeps two nodes of the same name apart (use it within one file).
        The arrays are read-only."""
        a = self.json["animations"][self.animation_index(anim)]
        out = {}
        for c in a["channels"]:
            target = c["target"]
            if "node" not in target:
                continue
            s = a["samplers"][c["sampler"]]
            key = target["node"] if by_index else self.name(target["node"])
            out[(key, target["path"])] = (
                self._accessor(s["input"])[:, 0], self._accessor(s["output"]), s.get("interpolation", "LINEAR"))
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
        return {i: m[0].copy() for i, m in self.posed_worlds(channels, [t], scene).items()}

    def posed_worlds(self, channels, times, scene=None):
        """posed_world at every time in `times` at once: {node: (len(times), 4, 4)}, read-only
        (nodes nothing animates above share one broadcast matrix)."""
        times = np.asarray(times, float).reshape(-1)
        paths = ("translation", "rotation", "scale")

        def local(i):
            nm = self.name(i)
            first = self.by_name.get(nm) == i
            found = {}
            for path in paths:
                if (i, path) in channels:
                    found[path] = channels[(i, path)]
                elif first and (nm, path) in channels:
                    found[path] = channels[(nm, path)]
            if not found:
                return self._rest_local(i)
            trs = dict(zip(paths, self.local_trs(i)))
            for path, (keys, values, interp) in found.items():
                trs[path] = sample_channel_at(keys, values, times, path, interp)
            return trs_matrices(trs["translation"], trs["rotation"], trs["scale"])

        out = {}
        stack = [(r, np.eye(4)) for r in self.roots(scene)]
        while stack:
            i, parent = stack.pop()
            out[i] = parent @ local(i)
            stack.extend((c, out[i]) for c in self.nodes[i].get("children", []))
        shape = (len(times), 4, 4)
        return {i: m if m.ndim == 3 else np.broadcast_to(m, shape) for i, m in out.items()}

    def _rest_local(self, i):
        return self._cached(("local", i), lambda: self.local_matrix(i))


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
