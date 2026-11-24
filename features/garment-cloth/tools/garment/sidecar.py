"""The sidecar `<model>.cloth.glb` (spec B.1): the authored sim meshes of one outfit model as a binary glTF.

It holds the skeleton nodes of the model's skin (same names, rest transforms and inverse binds) and per piece a
skinned mesh `piece:<id>` (`extras`: label, fabric) whose primitive carries
- POSITION (rest, model space), JOINTS_0 (u16) and WEIGHTS_0 on the model's skin joints;
- _MAX_DISTANCE r, _BACKSTOP_DISTANCE d, _BACKSTOP_RADIUS R (0 off, +inf the plane form), _BODY_DIRECTION b;
- one morph target per corrective (POSITION and _BODY_DIRECTION deltas), named by `extras.targetNames`.
The root `extras` hold `build` (the hash of the script and its inputs, and the library version) and `layers`
([outer, inner] piece ids). `write(read(data)) == data` for every file `write` produced."""
import json
import struct
from dataclasses import dataclass, field

import numpy as np

GLTF, JSON_CHUNK, BIN_CHUNK = 0x46546C67, 0x4E4F534A, 0x004E4942
FLOAT, U16, U32 = 5126, 5123, 5125
MAPS = (("r", "_MAX_DISTANCE"), ("d", "_BACKSTOP_DISTANCE"), ("R", "_BACKSTOP_RADIUS"))


@dataclass
class Piece:
    """One sim mesh as stored: float32 rest points (V, 3), triangles (F, 3), four joint lanes (u16) and weights, the
    maps r, d, R (V,) and b (V, 3), and correctives {name: (position delta (V, 3), direction delta (V, 3))}."""
    id: str
    label: int
    fabric: str
    points: np.ndarray
    faces: np.ndarray
    joints: np.ndarray
    weights: np.ndarray
    r: np.ndarray
    d: np.ndarray
    R: np.ndarray
    b: np.ndarray
    correctives: dict = field(default_factory=dict)


@dataclass
class Sidecar:
    """nodes: the skeleton part of the glTF node list (JSON objects, children indexing this list); joints: the skin's
    joint nodes in order; inverse_binds: (J, 16) float32 as glTF stores them (column-major)."""
    nodes: list
    joints: list
    inverse_binds: np.ndarray
    pieces: list
    build: dict
    layers: list = field(default_factory=list)

    @property
    def joint_names(self):
        return [self.nodes[j]["name"] for j in self.joints]

    def inverse_bind_matrices(self):
        """(J, 4, 4) row-major float64."""
        return self.inverse_binds.astype(float).reshape(-1, 4, 4).transpose(0, 2, 1)

    def piece(self, piece_id):
        for p in self.pieces:
            if p.id == piece_id:
                return p
        raise KeyError(piece_id)


def skeleton(glb):
    """(nodes, joints, inverse_binds) of a model's skin for a Sidecar: its joint nodes and their ancestors with their
    rest transforms, re-indexed in the model's node order."""
    skin = glb.json["skins"][0]
    keep = set()
    for j in skin["joints"]:
        while j is not None and j not in keep:
            keep.add(j)
            j = glb.parent.get(j)
    order = sorted(keep)
    index = {n: i for i, n in enumerate(order)}
    nodes = []
    for n in order:
        source = glb.nodes[n]
        node = {"name": source.get("name", f"node{n}")}
        for key in ("matrix", "translation", "rotation", "scale"):
            if key in source:
                node[key] = source[key]
        children = [index[c] for c in source.get("children", []) if c in index]
        if children:
            node["children"] = children
        nodes.append(node)
    acc = glb.json["accessors"][skin["inverseBindMatrices"]]
    view = glb.json["bufferViews"][acc["bufferView"]]
    start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ibm = np.frombuffer(glb.bin, "<f4", 16 * len(skin["joints"]), start).reshape(-1, 16).copy()
    return nodes, [index[j] for j in skin["joints"]], ibm


class _Buffer:
    def __init__(self):
        self.data, self.views, self.accessors = bytearray(), [], []

    def add(self, array, component, kind, bounds=False):
        dtype = {FLOAT: "<f4", U16: "<u2", U32: "<u4"}[component]
        array = np.ascontiguousarray(array, dtype)
        self.data.extend(b"\0" * (-len(self.data) % 4))
        self.views.append({"buffer": 0, "byteOffset": len(self.data), "byteLength": array.nbytes})
        self.data.extend(array.tobytes())
        count = array.shape[0]
        accessor = {"bufferView": len(self.views) - 1, "componentType": component, "count": count, "type": kind}
        if bounds:
            accessor["min"] = [float(v) for v in array.min(0)]
            accessor["max"] = [float(v) for v in array.max(0)]
        self.accessors.append(accessor)
        return len(self.accessors) - 1


def write(sidecar):
    """Bytes of the sidecar."""
    buf = _Buffer()
    nodes = [dict(n) for n in sidecar.nodes]
    roots = [i for i in range(len(nodes)) if not any(i in n.get("children", []) for n in nodes)]
    meshes = []
    for p in sidecar.pieces:
        n = len(p.points)
        _check(p)
        attributes = {"POSITION": buf.add(p.points, FLOAT, "VEC3", bounds=True),
                      "JOINTS_0": buf.add(p.joints, U16, "VEC4"), "WEIGHTS_0": buf.add(p.weights, FLOAT, "VEC4")}
        for name, semantic in MAPS:
            attributes[semantic] = buf.add(getattr(p, name).reshape(n, 1), FLOAT, "SCALAR")
        attributes["_BODY_DIRECTION"] = buf.add(p.b, FLOAT, "VEC3")
        primitive = {"attributes": attributes,
                     "indices": buf.add(p.faces.reshape(-1, 1), U16 if n <= 0xFFFF else U32, "SCALAR"), "mode": 4}
        mesh = {"name": f"piece:{p.id}", "primitives": [primitive], "extras": {"label": int(p.label), "fabric": p.fabric}}
        if p.correctives:
            primitive["targets"] = [{"POSITION": buf.add(dx, FLOAT, "VEC3", bounds=True),
                                     "_BODY_DIRECTION": buf.add(db, FLOAT, "VEC3")} for dx, db in p.correctives.values()]
            mesh["weights"] = [0.0] * len(p.correctives)
            mesh["extras"]["targetNames"] = list(p.correctives)
        meshes.append(mesh)
        nodes.append({"name": f"piece:{p.id}", "mesh": len(meshes) - 1, "skin": 0})
    skin = {"joints": list(sidecar.joints), "inverseBindMatrices": buf.add(sidecar.inverse_binds, FLOAT, "MAT4")}
    doc = {"asset": {"version": "2.0", "generator": "garment-cloth sidecar"},
           "extras": {"build": sidecar.build, "layers": [list(pair) for pair in sidecar.layers]},
           "scene": 0, "scenes": [{"nodes": roots + list(range(len(sidecar.nodes), len(nodes)))}],
           "nodes": nodes, "meshes": meshes, "skins": [skin],
           "accessors": buf.accessors, "bufferViews": buf.views, "buffers": [{"byteLength": len(buf.data)}]}
    text = json.dumps(doc, separators=(",", ":"), ensure_ascii=False, allow_nan=False).encode("utf-8")
    text += b" " * (-len(text) % 4)
    data = bytes(buf.data) + b"\0" * (-len(buf.data) % 4)
    body = struct.pack("<II", len(text), JSON_CHUNK) + text + struct.pack("<II", len(data), BIN_CHUNK) + data
    return struct.pack("<III", GLTF, 2, 12 + len(body)) + body


def _check(p):
    n = len(p.points)
    shapes = dict(points=(n, 3), joints=(n, 4), weights=(n, 4), r=(n,), d=(n,), R=(n,), b=(n, 3))
    for name, shape in shapes.items():
        if np.shape(getattr(p, name)) != shape:
            raise ValueError(f"piece {p.id}: {name} has shape {np.shape(getattr(p, name))}, expected {shape}")
    if p.faces.ndim != 2 or p.faces.shape[1] != 3 or p.faces.min() < 0 or p.faces.max() >= n:
        raise ValueError(f"piece {p.id}: triangles must index its {n} points")
    for name, (dx, db) in p.correctives.items():
        if np.shape(dx) != (n, 3) or np.shape(db) != (n, 3):
            raise ValueError(f"piece {p.id}: corrective {name} needs (V, 3) deltas")


def read(data):
    """The Sidecar in a file `write` produced; a file of another shape is refused."""
    data = bytes(data)
    magic, version, total = struct.unpack_from("<III", data)
    size, kind = struct.unpack_from("<II", data, 12)
    if magic != GLTF or version != 2 or total != len(data) or kind != JSON_CHUNK:
        raise ValueError("not a binary glTF 2 sidecar")
    doc = json.loads(data[20:20 + size])
    bsize, bkind = struct.unpack_from("<II", data, 20 + size)
    if bkind != BIN_CHUNK:
        raise ValueError("the sidecar has no binary chunk")
    binary = data[28 + size:28 + size + bsize]
    dtypes = {FLOAT: "<f4", U16: "<u2", U32: "<u4"}
    width = {"SCALAR": 1, "VEC3": 3, "VEC4": 4, "MAT4": 16}

    def accessor(i):
        a = doc["accessors"][i]
        view = doc["bufferViews"][a["bufferView"]]
        return np.frombuffer(binary, dtypes[a["componentType"]], a["count"] * width[a["type"]],
                             view["byteOffset"]).reshape(a["count"], width[a["type"]]).copy()

    skins = doc.get("skins", [])
    if len(skins) != 1:
        raise ValueError("a sidecar has one skin")
    mesh_nodes = [i for i, n in enumerate(doc["nodes"]) if "mesh" in n]
    first = min(mesh_nodes, default=len(doc["nodes"]))
    if mesh_nodes != list(range(first, len(doc["nodes"]))):
        raise ValueError("the sidecar's piece nodes must follow its skeleton nodes")
    pieces = []
    for mesh in doc["meshes"]:
        if not mesh["name"].startswith("piece:") or len(mesh["primitives"]) != 1:
            raise ValueError(f"sidecar mesh {mesh['name']!r} is not one piece primitive")
        prim = mesh["primitives"][0]
        a = prim["attributes"]
        maps = {name: accessor(a[semantic]).ravel() for name, semantic in MAPS}
        names = mesh.get("extras", {}).get("targetNames", [])
        targets = prim.get("targets", [])
        if len(names) != len(targets):
            raise ValueError(f"{mesh['name']}: every morph target needs a name")
        correctives = {name: (accessor(t["POSITION"]), accessor(t["_BODY_DIRECTION"])) for name, t in zip(names, targets)}
        pieces.append(Piece(id=mesh["name"][len("piece:"):], label=int(mesh["extras"]["label"]),
                            fabric=mesh["extras"]["fabric"], points=accessor(a["POSITION"]),
                            faces=accessor(prim["indices"]).reshape(-1, 3).astype(np.int64),
                            joints=accessor(a["JOINTS_0"]), weights=accessor(a["WEIGHTS_0"]), b=accessor(a["_BODY_DIRECTION"]),
                            correctives=correctives, **maps))
    extras = doc.get("extras", {})
    return Sidecar(nodes=doc["nodes"][:first], joints=skins[0]["joints"],
                   inverse_binds=accessor(skins[0]["inverseBindMatrices"]), pieces=pieces,
                   build=extras.get("build", {}), layers=[tuple(pair) for pair in extras.get("layers", [])])
