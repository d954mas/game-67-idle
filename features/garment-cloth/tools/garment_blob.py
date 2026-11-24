"""Write and read the garment asset blob laid out in include/garment/asset.h.

The writer is where cooked values are checked: the runtime asserts the blob's structure and trusts
its values. Errors name the outfit, the piece and the field.
"""
import math
import struct

import numpy as np

MAGIC = int.from_bytes(b"GRMT", "little")
VERSION = 4
ALIGN = 16
OUTFIT = 0xFFFF
HEADER = struct.Struct("<IHHIIHH")
SECTION = struct.Struct("<IHHII")
VERTICES_MAX, EDGES_MAX, STRAIN_PASSES_MAX = 512, 1536, 32
PIECES_MAX, BONES_MAX, SHAPES_MAX, CORRECTIVES_MAX = 8, 160, 64, 16
# A corrective's direction may turn the rest one by up to about 168 degrees: every blend toward it then keeps a length
# of at least this, a direction the runtime's normalize can trust.
BLEND_DIRECTION_MIN = 0.1
MAX_DISTANCE_MAX = 1.0  # m: a larger r is no longer a garment's motion around its reference
PI32 = np.float32(math.pi)
INT32 = (-2 ** 31, 2 ** 31 - 1)
UINT16, UINT8 = (0, 0xFFFF), (0, 0xFF)

MATERIAL_FIELDS = ("density", "stretch", "bend", "drag", "thickness", "iterations", "max_edge_strain", "strain_passes",
                   "aero_drag", "weft_stretch_scale", "diag_stretch_scale", "weft_bend_scale",
                   "weft_max_strain", "friction", "compression_limit")
INTEGER_FIELDS = {"iterations", "strain_passes"}


def _record(fields, size):
    names, formats, offsets = zip(*fields)
    return np.dtype({"names": list(names), "formats": list(formats), "offsets": list(offsets), "itemsize": size})


MATERIAL = _record([(n, "<i4" if n in INTEGER_FIELDS else "<f4", 4 * i) for i, n in enumerate(MATERIAL_FIELDS)], 60)
PIECE = _record([("id", "<u4", 0), ("render_vertices", "<i4", 4), ("linear_inertia", "<f4", 8),
                 ("angular_inertia", "<f4", 12), ("material", MATERIAL, 16)], 76)
BONE = _record([("name", "<u4", 0), ("inverse_bind", ("<f4", (12,)), 4)], 52)
SPHERE = _record([("bone", "<u2", 0), ("center", ("<f4", (3,)), 4), ("radius", "<f4", 16)], 20)
CONE = _record([("a", "<u2", 0), ("b", "<u2", 2)], 4)
REST = np.dtype(("<f4", (3,)))
FACE = np.dtype(("<u2", (3,)))
EDGE = _record([("a", "<i4", 0), ("b", "<i4", 4), ("c", "<i4", 8), ("d", "<i4", 12), ("length", "<f4", 16),
                ("angle", "<f4", 20), ("kind", "u1", 24)], 28)
TETHER = _record([("pin", "<i4", 0), ("max_length", "<f4", 4)], 8)
SKIN = _record([("bone", ("<u2", (4,)), 0), ("weight", ("<f4", (4,)), 8)], 24)
BACKSTOP = _record([("d", "<f4", 0), ("R", "<f4", 4)], 8)
CORRECTIVE = _record([("delta", ("<f4", (3,)), 0), ("direction", ("<f4", (3,)), 12), ("backstop", BACKSTOP, 24)], 32)

# name: (tag, record, outfit-wide)
SECTIONS = {
    "strings": (b"STRS", np.dtype("u1"), True), "bones": (b"BONE", BONE, True),
    "spheres": (b"SPHR", SPHERE, True), "cones": (b"CONE", CONE, True),
    "corrective_names": (b"CNAM", np.dtype("<u4"), True),
    "piece": (b"PIEC", PIECE, False), "rest": (b"REST", REST, False),
    "inv_area": (b"INVA", np.dtype("<f4"), False), "faces": (b"FACE", FACE, False), "edges": (b"EDGE", EDGE, False),
    "tethers": (b"TETH", TETHER, False), "skin": (b"SKIN", SKIN, False),
    "max_distance": (b"MAXD", np.dtype("<f4"), False), "backstop": (b"BKST", BACKSTOP, False),
    "direction": (b"BDIR", REST, False), "correctives": (b"CORR", CORRECTIVE, False),
}
TAGS = {int.from_bytes(tag, "little"): name for name, (tag, _, _) in SECTIONS.items()}


def single(values):
    """Single precision through 10-significant-digit decimal rounding, which keeps blobs bit-equal to the
    recorded baseline."""
    array = np.asarray(values, dtype=np.float64)
    # Rounding through double equals direct decimal-to-single rounding here: 2**24 * 5**9 < 2**53.
    rounded = np.fromiter((float(f"{v:.9e}") for v in array.ravel()), np.float64, array.size)
    return rounded.reshape(array.shape).astype(np.float32)


class _Check:
    def __init__(self, where):
        self.where = where

    def __call__(self, ok, field, problem):
        if not ok:
            raise ValueError(f"{self.where}: {field} {problem}")

    def each(self, ok, field, problem):
        bad = np.flatnonzero(~np.asarray(ok).reshape(len(ok), -1).all(axis=1)) if len(ok) else []
        self(len(bad) == 0, f"{field}[{bad[0] if len(bad) else 0}]", problem)

    def number(self, value, field, low, high=math.inf, low_open=False):
        ok = np.isfinite(value) and (value > low if low_open else value >= low) and value <= high
        bound = f"({low}" if low_open else f"[{low}"
        self(ok, field, f"must be finite in {bound}, {high}]: {value}")

    def integers(self, values, field, bounds):
        """Integer input in the record's range, checked before the cast so nothing truncates or wraps."""
        array = np.asarray(values)
        self(array.dtype.kind in "iu", field, f"must be integers, got {array.dtype}")
        self.each((array >= bounds[0]) & (array <= bounds[1]), field, f"must be in [{bounds[0]}, {bounds[1]}]")
        return array.astype(np.int64)


class _Strings:
    def __init__(self):
        self.data, self.offsets = bytearray(), {}

    def add(self, text, check, field):
        check(isinstance(text, str) and text and "\0" not in text, field, "must be a non-empty string without NUL")
        if text not in self.offsets:
            self.offsets[text] = len(self.data)
            self.data += text.encode("utf-8") + b"\0"
        return self.offsets[text]


def _material(check, material):
    unknown = set(material).difference(MATERIAL_FIELDS)
    check(not unknown, "material", f"has unknown fields {sorted(unknown)}")
    record = np.zeros((), MATERIAL)
    for name in MATERIAL_FIELDS:
        value = material.get(name, 0)
        field = f"material.{name}"
        if name in INTEGER_FIELDS:
            check(isinstance(value, (int, np.integer)) and not isinstance(value, bool), field, "must be an integer")
            check(INT32[0] <= value <= INT32[1], field, f"must be in [{INT32[0]}, {INT32[1]}]")
            record[name] = value
        else:
            check(isinstance(value, (int, float, np.integer, np.floating)) and not isinstance(value, bool), field,
                  "must be a number")
            record[name] = single(value)
    check.number(record["density"], "material.density", 0, low_open=True)
    check(int(record["iterations"]) >= 1, "material.iterations", "must be at least 1")
    check(0 <= int(record["strain_passes"]) <= STRAIN_PASSES_MAX, "material.strain_passes",
          f"must be in [0, {STRAIN_PASSES_MAX}]")
    highs = {"compression_limit": 0.5}
    for name in MATERIAL_FIELDS[1:]:
        if name not in INTEGER_FIELDS:
            check.number(record[name], f"material.{name}", 0, np.float32(highs.get(name, math.inf)))
    return record


def _mesh(check, mesh, records):
    rest = single(mesh["rest"])
    n = len(rest)
    check(rest.shape == (n, 3) and 3 <= n <= VERTICES_MAX, "mesh.rest", f"needs 3 to {VERTICES_MAX} points, got {rest.shape}")
    check.each(np.isfinite(rest), "mesh.rest", "must be finite")
    inv = single(mesh["inv_area"])
    check(inv.shape == (n,), "mesh.inv_area", "needs one value per vertex")
    check.each(np.isfinite(inv) & (inv > 0), "mesh.inv_area", "must be finite and positive")
    source = list(mesh["edges"])
    check(3 <= len(source) <= EDGES_MAX, "mesh.edges", f"needs 3 to {EDGES_MAX} edges, got {len(source)}")
    edges = np.zeros(len(source), EDGE)
    for key in "abcd":
        edges[key] = check.integers([e["abcd".index(key)] for e in source], f"mesh.edges.{key}", INT32)
    edges["length"] = single([e[4] for e in source])
    edges["angle"] = single([e[5] for e in source])
    kinds = check.integers([e[6] for e in source], "mesh.edges.kind", UINT8)
    check.each((kinds >= 0) & (kinds <= 2), "mesh.edges.kind", "must be warp, weft or diagonal (0..2)")
    edges["kind"] = kinds
    abc = np.stack([edges[k] for k in "abc"], axis=1)
    d = edges["d"]
    distinct = ((abc[:, 0] != abc[:, 1]) & (abc[:, 0] != abc[:, 2]) & (abc[:, 1] != abc[:, 2]) &
                (abc[:, 0] != d) & (abc[:, 1] != d) & (abc[:, 2] != d))
    check.each(((abc >= 0) & (abc < n)).all(axis=1) & (d >= -1) & (d < n) & distinct, "mesh.edges",
               "must name distinct vertices; d is -1 on a boundary")
    check.each(np.isfinite(edges["length"]) & (edges["length"] > 0), "mesh.edges.length", "must be finite and positive")
    check.each(np.isfinite(edges["angle"]) & (np.abs(edges["angle"]) <= PI32), "mesh.edges.angle", "must be finite in [-pi, pi]")
    faces = check.integers(mesh["faces"], "mesh.faces", UINT16).reshape(-1, 3)
    check(1 <= len(faces) <= 2 * len(edges) // 3, "mesh.faces", f"needs 1 to {2 * len(edges) // 3} triangles")
    check.each(((faces >= 0) & (faces < n)).all(axis=1) & (faces[:, 0] != faces[:, 1]) & (faces[:, 1] != faces[:, 2]) &
               (faces[:, 2] != faces[:, 0]), "mesh.faces", "must name three distinct vertices")
    r = single(mesh["max_distance"])
    check(r.shape == (n,), "mesh.max_distance", "needs one value per vertex")
    check.each(np.isfinite(r) & (r >= 0) & (r <= MAX_DISTANCE_MAX), "mesh.max_distance",
               f"must be finite in [0, {MAX_DISTANCE_MAX}] m")
    values = single(mesh["backstop"])
    check(values.shape == (n, 2), "mesh.backstop", "needs (d, R) per vertex")
    check.each(np.isfinite(values[:, 0]) & (values[:, 0] >= 0) & ((values[:, 1] >= 0) & ~np.isnan(values[:, 1])),
               "mesh.backstop", "needs a finite non-negative d and R >= 0 (+inf is the plane form)")
    backstop = np.zeros(n, BACKSTOP)
    backstop["d"], backstop["R"] = values[:, 0], values[:, 1]
    records.update(rest=rest, inv_area=inv, faces=faces.astype("<u2"), edges=edges, max_distance=r, backstop=backstop)
    if mesh.get("tethers") is not None:
        source = list(mesh["tethers"])
        check(len(source) == n, "mesh.tethers", "needs one tether per vertex")
        tethers = np.zeros(n, TETHER)
        tethers["pin"] = check.integers([t[0] for t in source], "mesh.tethers.pin", INT32)
        tethers["max_length"] = single([t[1] for t in source])
        pin = tethers["pin"]
        check.each((pin >= -1) & (pin < n), "mesh.tethers.pin", "must name a vertex or be -1")
        check.each((pin < 0) | (r[np.maximum(pin, 0)] == 0), "mesh.tethers.pin", "must name a kinematic vertex")
        check.each(np.isfinite(tethers["max_length"]) & (tethers["max_length"] >= 0), "mesh.tethers.max_length",
                   "must be finite and non-negative")
        records["tethers"] = tethers
    return n


def _correctives(check, shapes, count, n, kinematic, direction, backstop):
    """The CORR records of one piece: per corrective and vertex the position and body direction deltas and the
    backstop (d, R) at the key. None where no corrective changes the piece (every delta zero, every key backstop the
    rest one): the runtime then skips it. A failure names the corrective and the vertex."""
    if shapes is None or count == 0:
        check(shapes is None or not np.size(shapes), "correctives", "need corrective names on the outfit")
        return None
    shapes = single(shapes)
    check(shapes.shape == (count, n, 8), "correctives", f"need shape {(count, n, 8)} (delta, direction, d, R), got "
                                                        f"{shapes.shape}")
    for k in range(count):
        delta, turn, key = shapes[k, :, :3], shapes[k, :, 3:6], shapes[k, :, 6:]
        field = f"correctives[{k}]"
        check.each(np.isfinite(shapes[k, :, :7]), field, "must be finite (R may be +inf, the plane form)")
        check.each(~kinematic | ((delta == 0).all(1) & (turn == 0).all(1)), field,
                   "must not move a kinematic vertex (max distance 0)")
        corrected = direction.astype(np.float64) + turn
        check.each(np.abs(np.linalg.norm(corrected, axis=1) - 1) <= 1e-3, f"{field}.direction",
                   "must keep the corrected body direction a unit vector")
        check.each(np.linalg.norm(direction + 0.5 * turn, axis=1) >= BLEND_DIRECTION_MIN, f"{field}.direction",
                   "must not reverse the body direction")
        check.each((key[:, 0] >= 0) & (key[:, 1] >= 0) & ~np.isnan(key[:, 1]), f"{field}.backstop",
                   "needs a finite non-negative d and R >= 0 (+inf is the plane form)")
    rest = np.stack([backstop["d"], backstop["R"]], axis=1)
    if not shapes[:, :, :6].any() and (shapes[:, :, 6:] == rest[None]).all():
        return None
    out = np.zeros(count * n, CORRECTIVE)
    out["delta"], out["direction"] = shapes[:, :, :3].reshape(-1, 3), shapes[:, :, 3:6].reshape(-1, 3)
    out["backstop"]["d"], out["backstop"]["R"] = shapes[:, :, 6].ravel(), shapes[:, :, 7].ravel()
    return out


def _piece(check, piece, strings, bone_count, corrective_count=0):
    records = {}
    n = _mesh(check, piece["mesh"], records)
    source = piece["skin"]
    skin = np.zeros(n, SKIN)
    indices = check.integers(source["bone"], "skin.bone", UINT16).reshape(-1, 4)
    check(indices.shape == (n, 4), "skin.bone", "needs four influences per vertex")
    check.each((indices >= 0) & (indices < bone_count), "skin.bone", "must name an outfit bone")
    skin["bone"] = indices
    weight = single(source["weight"])
    check(weight.shape == (n, 4), "skin.weight", f"needs shape {(n, 4)}")
    check.each(np.isfinite(weight) & (weight >= 0), "skin.weight", "must be finite and non-negative")
    check.each(np.abs(weight.sum(1) - 1) <= 1e-3, "skin.weight", "must sum to 1 within 1e-3")
    skin["weight"] = weight
    records["skin"] = skin
    direction = single(piece["direction"])
    check(direction.shape == (n, 3), "direction", "needs a unit vector per vertex")
    check.each(np.isfinite(direction).all(axis=1) & (np.abs(np.linalg.norm(direction, axis=1) - 1) <= 1e-3),
               "direction", "must be a finite unit vector")
    records["direction"] = direction
    shapes = _correctives(check, piece.get("correctives"), corrective_count, n, records["max_distance"] == 0, direction,
                          records["backstop"])
    if shapes is not None:
        records["correctives"] = shapes
    head = np.zeros(1, PIECE)
    head["id"] = strings.add(piece["id"], check, "id")
    render = piece.get("render_vertices", 0)
    check(isinstance(render, (int, np.integer)) and not isinstance(render, bool) and 0 <= render <= INT32[1],
          "render_vertices", "must be a non-negative 32-bit integer")
    head["render_vertices"] = render
    for key in ("linear_inertia", "angular_inertia"):
        head[key] = single(piece.get(key, 0.0))
        check.number(head[key][0], key, 0, 1)
    head["material"] = _material(check, piece["material"])
    records["piece"] = head
    return records


def _outfit(check, strings, bones, spheres, cones, correctives=()):
    records = {}
    names = list(correctives)
    check(len(names) <= CORRECTIVES_MAX and len(set(names)) == len(names), "correctives",
          f"need at most {CORRECTIVES_MAX} unique names")
    if names:
        records["corrective_names"] = np.array([strings.add(n, check, f"correctives[{i}]") for i, n in enumerate(names)],
                                               "<u4")
    bones = list(bones)
    check(1 <= len(bones) <= BONES_MAX, "bones", f"needs 1 to {BONES_MAX} bones, got {len(bones)}")
    bone = np.zeros(len(bones), BONE)
    bone["name"] = [strings.add(name, check, f"bones[{i}].name") for i, (name, _) in enumerate(bones)]
    bone["inverse_bind"] = single([matrix for _, matrix in bones]).reshape(-1, 12)
    check.each(np.isfinite(bone["inverse_bind"]), "bones.inverse_bind", "must be finite")
    records["bones"] = bone
    spheres, cones = list(spheres), list(cones)
    check(len(spheres) <= SHAPES_MAX and len(cones) <= SHAPES_MAX, "shapes", f"exceed {SHAPES_MAX} spheres or cones")
    if spheres:
        sphere = np.zeros(len(spheres), SPHERE)
        sphere["bone"] = check.integers([s[0] for s in spheres], "spheres.bone", UINT16)
        check.each(sphere["bone"] < len(bones), "spheres.bone", "must name an outfit bone")
        sphere["center"] = single([s[1] for s in spheres]).reshape(-1, 3)
        sphere["radius"] = single([s[2] for s in spheres])
        check.each(np.isfinite(sphere["center"]), "spheres.center", "must be finite")
        check.each(np.isfinite(sphere["radius"]) & (sphere["radius"] >= 0), "spheres.radius",
                   "must be finite and non-negative")
        records["spheres"] = sphere
    if cones:
        cone = np.zeros(len(cones), CONE)
        for i, key in enumerate("ab"):
            cone[key] = check.integers([c[i] for c in cones], f"cones.{key}", UINT16)
        check.each((cone["a"] < len(spheres)) & (cone["b"] < len(spheres)) & (cone["a"] != cone["b"]), "cones",
                   "must join two different spheres")
        records["cones"] = cone
    return records


def encode(outfit_id, pieces, bones=(), spheres=(), cones=(), correctives=()):
    """Return the blob for one outfit. bones are (name, inverse bind 3x4), spheres (bone, center in bone space,
    radius) and cones (sphere a, sphere b), shared by every piece; correctives the names of the outfit's corrective
    shapes; a piece is a dict of id, mesh (rest, inv_area, faces, edges, tethers, max_distance, backstop as (d, R)),
    skin (bone, weight), direction, material, render_vertices, inertia weights and, with corrective names,
    "correctives": (names, vertices, 8) rest-space position and body direction deltas and the backstop (d, R) at each
    key (None: no corrective changes the piece). An outfit without cloth has no pieces and no bones."""
    strings = _Strings()
    outfit = _Check(str(outfit_id))
    header_id = strings.add(outfit_id, outfit, "id")
    outfit(len(pieces) <= PIECES_MAX, "pieces", f"exceed {PIECES_MAX}")
    names = [piece.get("id") for piece in pieces]
    outfit(len(set(names)) == len(names), "pieces", "need unique ids")
    sections = []
    if pieces:
        for name, array in _outfit(outfit, strings, bones, spheres, cones, correctives).items():
            sections.append((name, OUTFIT, array))
    for index, piece in enumerate(pieces):
        check = _Check(f"{outfit_id}/{piece.get('id', index)}")
        for name, array in _piece(check, piece, strings, len(bones), len(correctives)).items():
            sections.append((name, index, array))
    sections.insert(0, ("strings", OUTFIT, np.frombuffer(bytes(strings.data), "u1")))
    offset = HEADER.size + SECTION.size * len(sections)
    table, body = bytearray(), bytearray()
    for name, piece, array in sections:
        tag, record, _ = SECTIONS[name]
        assert array.dtype.base == record.base, name
        data = np.ascontiguousarray(array).tobytes()
        start = -(-(offset + len(body)) // ALIGN) * ALIGN
        body += bytes(start - offset - len(body)) + data
        table += SECTION.pack(int.from_bytes(tag, "little"), piece, record.itemsize, start, len(data) // record.itemsize)
    size = offset + len(body)
    return HEADER.pack(MAGIC, VERSION, HEADER.size, size, header_id, len(pieces), len(sections)) + table + body


def decode(data):
    """Read a blob back into numpy records: the inverse of encode, for tools and tests. It refuses what the C view
    asserts on: another version, an unknown or repeated section, corrective shapes without corrective names or with
    another count than names x vertices, and an invalid key backstop."""
    data = bytes(data)
    magic, version, header_size, size, outfit_id, piece_count, count = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION or header_size != HEADER.size or size != len(data):
        raise ValueError(f"not a garment blob of version {VERSION}")
    found = {}
    for i in range(count):
        tag, piece, stride, start, length = SECTION.unpack_from(data, header_size + i * SECTION.size)
        name = TAGS.get(tag)
        if name is None or stride != SECTIONS[name][1].itemsize or start + stride * length > size:
            raise ValueError(f"invalid garment blob section {tag:#x}")
        if (name, piece) in found:
            raise ValueError(f"garment blob section {name} of piece {piece} appears twice")
        found[name, piece] = np.frombuffer(data, SECTIONS[name][1], length, start)
    names = found.get(("corrective_names", OUTFIT), np.zeros(0, "<u4"))
    for p in range(piece_count):
        shapes = found.get(("correctives", p))
        if shapes is None:
            continue
        if not len(names) or len(shapes) != len(names) * len(found["rest", p]):
            raise ValueError(f"piece {p}: corrective shapes need {len(names)} corrective names x "
                             f"{len(found['rest', p])} vertices, got {len(shapes)} records")
        key = shapes["backstop"]
        if not (np.isfinite(key["d"]) & (key["d"] >= 0) & (key["R"] >= 0)).all():
            raise ValueError(f"piece {p}: a corrective's key backstop needs a finite d >= 0 and R >= 0")
    strings = found["strings", OUTFIT].tobytes()

    def text(offset):
        return strings[offset:strings.index(b"\0", offset)].decode("utf-8")

    pieces = []
    for p in range(piece_count):
        head = found["piece", p][0]
        backstop = found["backstop", p]
        shapes = found.get(("correctives", p))
        n = len(found["rest", p])
        pieces.append(dict(
            correctives=None if shapes is None else np.concatenate(
                [shapes["delta"], shapes["direction"], shapes["backstop"]["d"][:, None], shapes["backstop"]["R"][:, None]],
                1).reshape(-1, n, 8),
            id=text(int(head["id"])),
            mesh=dict(rest=found["rest", p], inv_area=found["inv_area", p], faces=found["faces", p],
                      edges=found["edges", p], tethers=found.get(("tethers", p)), max_distance=found["max_distance", p],
                      backstop=np.stack([backstop["d"], backstop["R"]], axis=1)),
            skin=found["skin", p], direction=found["direction", p],
            material={name: head["material"][name].item() for name in MATERIAL_FIELDS},
            render_vertices=int(head["render_vertices"]), linear_inertia=head["linear_inertia"].item(),
            angular_inertia=head["angular_inertia"].item()))
    bones = found.get(("bones", OUTFIT), np.zeros(0, BONE))
    return dict(id=text(outfit_id), bones=[(text(int(b["name"])), b["inverse_bind"].copy()) for b in bones],
                correctives=[text(int(n)) for n in names],
                spheres=found.get(("spheres", OUTFIT), np.zeros(0, SPHERE)),
                cones=found.get(("cones", OUTFIT), np.zeros(0, CONE)), pieces=pieces)
