"""Write and read the garment asset blob laid out in include/garment/asset.h.

The writer is where cooked values are checked: the runtime asserts the blob's structure and trusts
its values. Errors name the outfit, the piece and the field.
"""
import math
import struct

import numpy as np

MAGIC = int.from_bytes(b"GRMT", "little")
VERSION = 2
ALIGN = 16
OUTFIT = 0xFFFF
HEADER = struct.Struct("<IHHIIHH")
SECTION = struct.Struct("<IHHII")
VERTICES_MAX, EDGES_MAX, STRAIN_PASSES_MAX = 512, 1536, 32
PIECES_MAX, BONES_MAX, CAPSULES_MAX = 8, 160, 32
PI32 = np.float32(math.pi)
INT32 = (-2 ** 31, 2 ** 31 - 1)
UINT16, UINT8 = (0, 0xFFFF), (0, 0xFF)

MATERIAL_FIELDS = ("density", "stretch", "bend", "drag", "thickness", "iterations", "max_edge_strain", "strain_passes",
                   "aero_drag", "weft_stretch_scale", "diag_stretch_scale", "weft_bend_scale",
                   "weft_max_strain", "contact_friction", "max_speed_mps", "compression_limit", "reset_carry_range_m")
INTEGER_FIELDS = {"iterations", "strain_passes"}


def _record(fields, size):
    names, formats, offsets = zip(*fields)
    return np.dtype({"names": list(names), "formats": list(formats), "offsets": list(offsets), "itemsize": size})


MATERIAL = _record([(n, "<i4" if n in INTEGER_FIELDS else "<f4", 4 * i) for i, n in enumerate(MATERIAL_FIELDS)], 68)
PIECE = _record([("id", "<u4", 0), ("render_vertices", "<i4", 4), ("linear_inertia", "<f4", 8),
                 ("angular_inertia", "<f4", 12), ("downward_follow", "<f4", 16), ("material", MATERIAL, 20)], 88)
BONE = _record([("name", "<u4", 0), ("inverse_bind", ("<f4", (12,)), 4)], 52)
REST = np.dtype(("<f4", (3,)))
FACE = np.dtype(("<u2", (3,)))
EDGE = _record([("a", "<i4", 0), ("b", "<i4", 4), ("c", "<i4", 8), ("d", "<i4", 12), ("length", "<f4", 16),
                ("angle", "<f4", 20), ("kind", "u1", 24)], 28)
TETHER = _record([("pin", "<i4", 0), ("max_length", "<f4", 4)], 8)
VERTEX = _record([("bone", ("<u2", (4,)), 0), ("weight", ("<f4", (4,)), 8), ("normal", ("<f4", (3,)), 24),
                  ("travel_m", "<f4", 36), ("backstop_m", "<f4", 40)], 44)
COLLIDER = _record([("bone_a", "<u2", 0), ("bone_b", "<u2", 2), ("point_a", ("<f4", (3,)), 4),
                    ("point_b", ("<f4", (3,)), 16), ("radius_m", "<f4", 28), ("radius_b_m", "<f4", 32),
                    ("flags", "u1", 36)], 40)

# name: (tag, record, outfit-wide)
SECTIONS = {
    "strings": (b"STRS", np.dtype("u1"), True),
    "piece": (b"PIEC", PIECE, False), "bones": (b"BONE", BONE, False), "rest": (b"REST", REST, False),
    "inv_area": (b"INVA", np.dtype("<f4"), False), "faces": (b"FACE", FACE, False), "edges": (b"EDGE", EDGE, False),
    "tethers": (b"TETH", TETHER, False), "drive": (b"DRIV", np.dtype("<f4"), False),
    "vertices": (b"VERT", VERTEX, False), "colliders": (b"COLL", COLLIDER, False),
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
            record[name] = single(value)
    check.number(record["density"], "material.density", 0, low_open=True)
    check(int(record["iterations"]) >= 1, "material.iterations", "must be at least 1")
    check(0 <= int(record["strain_passes"]) <= STRAIN_PASSES_MAX, "material.strain_passes",
          f"must be in [0, {STRAIN_PASSES_MAX}]")
    highs = {"contact_friction": 1, "compression_limit": 0.5}
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
    check.each(np.isfinite(inv) & (inv >= 0), "mesh.inv_area", "must be finite and non-negative")
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
    records.update(rest=rest, inv_area=inv, faces=faces.astype("<u2"), edges=edges)
    if mesh.get("tethers") is not None:
        source = list(mesh["tethers"])
        check(len(source) == n, "mesh.tethers", "needs one tether per vertex")
        tethers = np.zeros(n, TETHER)
        tethers["pin"] = check.integers([t[0] for t in source], "mesh.tethers.pin", INT32)
        tethers["max_length"] = single([t[1] for t in source])
        pin = tethers["pin"]
        check.each((pin >= -1) & (pin < n), "mesh.tethers.pin", "must name a vertex or be -1")
        check.each((pin < 0) | (inv[np.maximum(pin, 0)] == 0), "mesh.tethers.pin", "must name a sewn vertex")
        check.each(np.isfinite(tethers["max_length"]) & (tethers["max_length"] >= 0), "mesh.tethers.max_length",
                   "must be finite and non-negative")
        records["tethers"] = tethers
    drive = single(mesh["drive"])
    check(drive.shape == (n,), "mesh.drive", "needs one squared angular frequency per vertex")
    check.each(np.isfinite(drive) & (drive >= 0), "mesh.drive", "must be finite and non-negative (s^-2)")
    records["drive"] = drive
    return n


def _piece(check, piece, strings):
    records = {}
    n = _mesh(check, piece["mesh"], records)
    bones = list(piece["bones"])
    check(1 <= len(bones) <= BONES_MAX, "bones", f"needs 1 to {BONES_MAX} bones, got {len(bones)}")
    bone = np.zeros(len(bones), BONE)
    bone["name"] = [strings.add(name, check, f"bones[{i}].name") for i, (name, _) in enumerate(bones)]
    bone["inverse_bind"] = single([matrix for _, matrix in bones]).reshape(-1, 12)
    check.each(np.isfinite(bone["inverse_bind"]), "bones.inverse_bind", "must be finite")
    records["bones"] = bone
    source = piece["vertices"]
    vertex = np.zeros(n, VERTEX)
    indices = check.integers(source["bone"], "vertices.bone", UINT16).reshape(-1, 4)
    check(indices.shape == (n, 4), "vertices.bone", "needs four influences per vertex")
    check.each((indices >= 0) & (indices < len(bones)), "vertices.bone", "must name a piece bone")
    vertex["bone"] = indices
    for field, key, shape in (("weight", "weight", (n, 4)), ("normal", "normal", (n, 3)),
                              ("travel_m", "travel", (n,)), ("backstop_m", "backstop", (n,))):
        values = single(source[key])
        check(values.shape == shape, f"vertices.{field}", f"needs shape {shape}")
        check.each(np.isfinite(values) & (values >= 0 if field in ("travel_m", "backstop_m") else True),
                   f"vertices.{field}", "must be finite" + (" and non-negative" if values.ndim == 1 else ""))
        vertex[field] = values
    records["vertices"] = vertex
    colliders = list(piece.get("colliders", ()))
    check(len(colliders) <= CAPSULES_MAX, "colliders", f"exceed {CAPSULES_MAX} capsules")
    if colliders:
        collider = np.zeros(len(colliders), COLLIDER)
        for key, column in (("bone_a", 0), ("bone_b", 1), ("flags", 6)):
            values = check.integers([c[column] for c in colliders], f"colliders.{key}", UINT16 if column < 2 else UINT8)
            check.each((values >= 0) & (values < (len(bones) if column < 2 else 2)), f"colliders.{key}",
                       "must name a piece bone" if column < 2 else "must be inside (0) or outside (1)")
            collider[key] = values
        for key, column in (("point_a", 2), ("point_b", 3), ("radius_m", 4), ("radius_b_m", 5)):
            values = single([c[column] for c in colliders])
            check.each(np.isfinite(values) & ((values >= 0) if column >= 4 else True), f"colliders.{key}",
                       "must be finite" + (" and non-negative" if column >= 4 else ""))
            collider[key] = values
        records["colliders"] = collider
    head = np.zeros(1, PIECE)
    head["id"] = strings.add(piece["id"], check, "id")
    render = piece.get("render_vertices", 0)
    check(isinstance(render, (int, np.integer)) and not isinstance(render, bool) and 0 <= render <= INT32[1],
          "render_vertices", "must be a non-negative 32-bit integer")
    head["render_vertices"] = render
    for key in ("linear_inertia", "angular_inertia", "downward_follow"):
        head[key] = single(piece.get(key, 0.0))
        check.number(head[key][0], key, 0, 1)
    head["material"] = _material(check, piece["material"])
    records["piece"] = head
    return records


def encode(outfit_id, pieces):
    """Return the blob for one outfit. A piece is a dict of id, mesh, bones, vertices, material and optional
    colliders, render_vertices and inertia weights; an outfit without cloth has no pieces."""
    strings = _Strings()
    outfit = _Check(str(outfit_id))
    header_id = strings.add(outfit_id, outfit, "id")
    outfit(len(pieces) <= PIECES_MAX, "pieces", f"exceed {PIECES_MAX}")
    names = [piece.get("id") for piece in pieces]
    outfit(len(set(names)) == len(names), "pieces", "need unique ids")
    sections = []
    for index, piece in enumerate(pieces):
        check = _Check(f"{outfit_id}/{piece.get('id', index)}")
        for name, array in _piece(check, piece, strings).items():
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
    """Read a blob back into numpy records: the inverse of encode, for tools and tests."""
    data = bytes(data)
    magic, version, header_size, size, outfit_id, piece_count, count = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION or header_size != HEADER.size or size != len(data):
        raise ValueError("not a garment blob of this version")
    found = {}
    for i in range(count):
        tag, piece, stride, start, length = SECTION.unpack_from(data, header_size + i * SECTION.size)
        name = TAGS.get(tag)
        if name is None or stride != SECTIONS[name][1].itemsize or start + stride * length > size:
            raise ValueError(f"invalid garment blob section {tag:#x}")
        found[name, piece] = np.frombuffer(data, SECTIONS[name][1], length, start)
    strings = found["strings", OUTFIT].tobytes()

    def text(offset):
        return strings[offset:strings.index(b"\0", offset)].decode("utf-8")

    pieces = []
    for p in range(piece_count):
        head = found["piece", p][0]
        pieces.append(dict(
            id=text(int(head["id"])),
            bones=[(text(int(b["name"])), b["inverse_bind"].copy()) for b in found["bones", p]],
            mesh=dict(rest=found["rest", p], inv_area=found["inv_area", p], faces=found["faces", p],
                      edges=found["edges", p], tethers=found.get(("tethers", p)), drive=found["drive", p]),
            vertices=found["vertices", p], colliders=found.get(("colliders", p), np.zeros(0, COLLIDER)),
            material={name: head["material"][name].item() for name in MATERIAL_FIELDS},
            render_vertices=int(head["render_vertices"]), linear_inertia=head["linear_inertia"].item(),
            angular_inertia=head["angular_inertia"].item(), downward_follow=head["downward_follow"].item()))
    return dict(id=text(outfit_id), pieces=pieces)
