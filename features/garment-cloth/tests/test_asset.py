import copy
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import garment_blob
from garment_blob import COLLIDER, MATERIAL_FIELDS, decode, encode, single
from garment_cook import cook_mesh

IDENTITY = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0]


def synthetic():
    """Two pieces: one with every optional section, one with none."""
    panel = cook_mesh("panel", [[-0.5, 0, 0], [0.5, 0, 0], [0, 1, 0.1], [0, 0.3, 1]], [[0, 1, 2], [1, 0, 3]], [0, 1],
                      wearable=True, tether_slack=1.05, drive=[0, 0, 140 * 0.35, 140 / 3])
    sash = cook_mesh("sash", [[0, 0, 0], [0.2, 0, 0], [0.1, -0.4, 0.05]], [[0, 1, 2]], [0, 1], wearable=True)
    material = {name: (i + 1 if name in ("iterations", "strain_passes") else 0.01 * (i + 1) + 1 / 7)
                for i, name in enumerate(MATERIAL_FIELDS)}
    material.update(contact_friction=0.6, compression_limit=0.2)
    return dict(outfit_id="synthetic", pieces=[
        dict(id="panel", mesh=panel, material=material, render_vertices=7,
             linear_inertia=0.45, angular_inertia=1 / 3, downward_follow=0.7,
             bones=[("Root", [0.1 * i + 1 / 9 for i in range(12)]), ("Hem", IDENTITY)],
             vertices=dict(bone=[[0, 1, 0, 0], [1, 0, 0, 0], [1, 1, 0, 0], [0, 0, 1, 1]],
                           weight=[[1, 0, 0, 0], [0.75, 0.25, 0, 0], [1 / 3, 2 / 3, 0, 0], [0.5, 0, 0.5, 0]],
                           normal=[[0, 0, 1], [0, 0.6, 0.8], [1, 0, 0], [0, -1, 0]],
                           travel=[0.003, 0.004, 0.0191, 0.02], backstop=[0.003, 0.0035, 0.0176, 0.018]),
             colliders=[(0, 1, [-0.062, 1.005, 0.004], [0.062, 1.005, 0.004], 0.084, 0.07, 0),
                        (1, 1, [0.1, 0.2, 0.3], [0.1, 0.4, 0.3], 0.05, 0, 1)]),
        dict(id="sash", mesh=sash, material=dict(density=0.3, iterations=1), bones=[("SashEnd", IDENTITY)],
             vertices=dict(bone=[[0, 0, 0, 0]] * 3, weight=[[1, 0, 0, 0]] * 3, normal=[[0, 0, 1]] * 3,
                           travel=[0, 0, 0.1], backstop=[0, 0, 0.1])),
    ])


def build(garment):
    return encode(garment["outfit_id"], garment["pieces"])


class TestBlob(unittest.TestCase):
    def test_reader_returns_what_the_writer_wrote(self):
        garment = synthetic()
        data = build(garment)
        self.assertEqual(len(data) % 4, 0)
        back = decode(data)
        self.assertEqual(back["id"], "synthetic")
        again = encode(back["id"], [dict(p, mesh=dict(p["mesh"], edges=p["mesh"]["edges"].tolist(),
                                                      tethers=None if p["mesh"]["tethers"] is None else p["mesh"]["tethers"].tolist()),
                                         vertices=dict(bone=p["vertices"]["bone"], weight=p["vertices"]["weight"],
                                                       normal=p["vertices"]["normal"], travel=p["vertices"]["travel_m"],
                                                       backstop=p["vertices"]["backstop_m"]),
                                         colliders=[tuple(c) for c in p["colliders"].tolist()]) for p in back["pieces"]])
        self.assertEqual(again, data)
        panel, source = back["pieces"][0], garment["pieces"][0]
        np.testing.assert_array_equal(panel["mesh"]["rest"], single(source["mesh"]["rest"]))
        np.testing.assert_array_equal(panel["vertices"]["weight"], single(source["vertices"]["weight"]))
        self.assertEqual(panel["material"]["iterations"], source["material"]["iterations"])
        self.assertEqual(back["pieces"][1]["mesh"]["tethers"], None)
        self.assertEqual(len(back["pieces"][1]["colliders"]), 0)
        np.testing.assert_array_equal(back["pieces"][1]["mesh"]["drive"], np.zeros(3, np.float32))

    def test_outfit_without_cloth_round_trips(self):
        self.assertEqual(decode(encode("plain", [])), dict(id="plain", pieces=[]))

    def test_single_reads_ten_significant_digits_like_a_c_literal(self):
        # Just below a single-precision midpoint the 10-digit literal 1.000000060 rounds up; a direct cast rounds down.
        value = 1 + 2 ** -24 - 2 ** -40
        self.assertEqual(single(value), np.float32(1 + 2 ** -23))
        self.assertEqual(np.float32(value), np.float32(1))
        self.assertTrue(np.isnan(single(float("nan"))))

    def test_cook_rejects_values_the_solver_trusts(self):
        cases = [
            (lambda p: p["mesh"]["rest"].__setitem__(2, [np.nan, 0, 0]), "panel: mesh.rest[2]"),
            (lambda p: p["mesh"]["inv_area"].__setitem__(3, -1), "panel: mesh.inv_area[3]"),
            (lambda p: p["mesh"]["edges"].__setitem__(0, (0, 9, 2, -1, 1.0, 0.0, 0)), "panel: mesh.edges[0]"),
            (lambda p: p["mesh"]["edges"].__setitem__(1, (0, 1, 2, -1, 0.0, 0.0, 0)), "panel: mesh.edges.length[1]"),
            (lambda p: p["mesh"]["edges"].__setitem__(1, (0, 1, 2, -1, 1.0, 3.2, 0)), "panel: mesh.edges.angle[1]"),
            (lambda p: p["mesh"]["edges"].__setitem__(1, (0, 1, 2, -1, 1.0, 0.0, 3)), "panel: mesh.edges.kind[1]"),
            (lambda p: p["mesh"]["faces"].__setitem__(1, (1, 1, 3)), "panel: mesh.faces[1]"),
            (lambda p: p["mesh"]["tethers"].__setitem__(3, (2, 0.1)), "panel: mesh.tethers.pin[3]"),
            (lambda p: p["mesh"]["drive"].__setitem__(2, -1.0), "panel: mesh.drive[2]"),
            (lambda p: p["vertices"]["bone"].__setitem__(1, [2, 0, 0, 0]), "panel: vertices.bone[1]"),
            (lambda p: p["vertices"]["weight"].__setitem__(1, [1.25, -0.25, 0, 0]), "panel: vertices.weight[1] must be non-negative"),
            (lambda p: p["vertices"]["weight"].__setitem__(1, [0.5, 0.4, 0, 0]), "panel: vertices.weight[1] must sum to 1"),
            (lambda p: p["vertices"]["normal"].__setitem__(2, [0, 0, 0]), "panel: vertices.normal[2] must be non-zero"),
            (lambda p: p["material"].__setitem__("drag", True), "panel: material.drag must be a number"),
            (lambda p: p["vertices"]["travel"].__setitem__(0, -0.1), "panel: vertices.travel_m[0]"),
            (lambda p: p["vertices"]["backstop"].__setitem__(1, -0.1), "panel: vertices.backstop_m[1]"),
            (lambda p: p["bones"].__setitem__(0, ("Root", [np.inf] * 12)), "panel: bones.inverse_bind[0]"),
            (lambda p: p["colliders"].__setitem__(1, (1, 2, [0, 0, 0], [0, 1, 0], 0.1, 0, 0)), "panel: colliders.bone_b[1]"),
            (lambda p: p["colliders"].__setitem__(0, (0, 1, [0, 0, 0], [0, 1, 0], -0.1, 0, 0)), "panel: colliders.radius_m[0]"),
            (lambda p: p["colliders"].extend([p["colliders"][0]] * 31), "panel: colliders exceed"),
            (lambda p: p["material"].__setitem__("density", 0), "panel: material.density"),
            (lambda p: p["material"].__setitem__("contact_friction", 1.5), "panel: material.contact_friction"),
            (lambda p: p["material"].__setitem__("compression_limit", 0.6), "panel: material.compression_limit"),
            (lambda p: p["material"].__setitem__("reset_carry_range_m", -0.1), "panel: material.reset_carry_range_m"),
            (lambda p: p["material"].__setitem__("strain_passes", 33), "panel: material.strain_passes"),
            (lambda p: p["material"].__setitem__("iterations", 0), "panel: material.iterations"),
            (lambda p: p.__setitem__("linear_inertia", 1.5), "panel: linear_inertia"),
        ]
        self.assert_rejected(cases)

    def test_integer_fields_reject_fractions_and_values_their_record_cannot_hold(self):
        self.assert_rejected([
            (lambda p: p["mesh"]["edges"].__setitem__(0, (0, 1.5, 2, -1, 1.0, 0.0, 0)), "panel: mesh.edges.b must be integers"),
            (lambda p: p["mesh"]["edges"].__setitem__(0, (0, 1, 2, -1, 1.0, 0.0, 256)), "panel: mesh.edges.kind[0] must be in"),
            (lambda p: p["mesh"].__setitem__("faces", np.asarray(p["mesh"]["faces"], np.float64)), "panel: mesh.faces must be integers"),
            (lambda p: p["vertices"].__setitem__("bone", p["vertices"]["bone"] + 65536), "panel: vertices.bone[0] must be in"),
            (lambda p: p["colliders"].__setitem__(0, (0, 1.0, [0, 0, 0], [0, 1, 0], 0.1, 0, 0)), "panel: colliders.bone_b must be integers"),
            (lambda p: p["material"].__setitem__("iterations", 2 ** 31), "panel: material.iterations must be in"),
            (lambda p: p.__setitem__("render_vertices", 7.0), "panel: render_vertices must be"),
        ])

    def assert_rejected(self, cases):
        for edit, message in cases:
            garment = synthetic()
            panel = garment["pieces"][0]
            panel["mesh"] = copy.deepcopy(panel["mesh"])
            panel["mesh"]["rest"] = np.array(panel["mesh"]["rest"])
            panel["mesh"]["inv_area"] = np.array(panel["mesh"]["inv_area"])
            panel["mesh"]["drive"] = np.array(panel["mesh"]["drive"])
            panel["vertices"] = {k: np.array(v) for k, v in panel["vertices"].items()}
            edit(panel)
            with self.subTest(message), self.assertRaisesRegex(ValueError, "^synthetic/" + message.replace("[", r"\[")):
                build(garment)


def c_float(value):
    return float(value).hex() + "F"


def c_floats(values):
    return "{" + ",".join(c_float(v) for v in np.asarray(values, np.float32).ravel()) + "}"


def emit_expected(out, garment, blob_path, empty_path):
    """The garment as C initializers with exact hexadecimal floats: what the C view must reproduce."""
    rows = []
    for i, piece in enumerate(decode(build(garment))["pieces"]):
        s, mesh = f"p{i}", piece["mesh"]
        out.write(f"static const GarmentBone {s}_bones[] = {{"
                  + ",".join(f'{{"{name}",{c_floats(matrix)}}}' for name, matrix in piece["bones"]) + "};\n")
        out.write(f"static const float {s}_rest[][3] = {{" + ",".join(c_floats(p) for p in mesh["rest"]) + "};\n")
        out.write(f"static const float {s}_inv[] = {c_floats(mesh['inv_area'])};\n")
        out.write(f"static const ClothFace {s}_faces[] = {{" + ",".join("{%d,%d,%d}" % tuple(f) for f in mesh["faces"]) + "};\n")
        out.write(f"static const ClothEdge {s}_edges[] = {{" + ",".join(
            f"{{{e['a']},{e['b']},{e['c']},{e['d']},{c_float(e['length'])},{c_float(e['angle'])},{e['kind']}}}"
            for e in mesh["edges"]) + "};\n")
        tethers = colliders = "NULL"
        if mesh["tethers"] is not None:
            tethers = f"{s}_tethers"
            out.write(f"static const ClothTether {tethers}[] = {{" + ",".join(
                f"{{{t['pin']},{c_float(t['max_length'])}}}" for t in mesh["tethers"]) + "};\n")
        drive = f"{s}_drive"
        out.write(f"static const float {drive}[] = {c_floats(mesh['drive'])};\n")
        out.write(f"static const ClothMesh {s}_mesh = {{{len(mesh['rest'])},{len(mesh['faces'])},{len(mesh['edges'])},"
                  f"{s}_rest,{s}_inv,{s}_faces,{s}_edges,{tethers},{drive}}};\n")
        out.write(f"static const GarmentVertex {s}_vertices[] = {{" + ",".join(
            "{{%s},%s,%s,%s,%s}" % (",".join(map(str, v["bone"])), c_floats(v["weight"]), c_floats(v["normal"]),
                                    c_float(v["travel_m"]), c_float(v["backstop_m"])) for v in piece["vertices"]) + "};\n")
        if len(piece["colliders"]):
            colliders = f"{s}_colliders"
            out.write(f"static const GarmentCollider {colliders}[] = {{" + ",".join(
                f"{{{c['bone_a']},{c['bone_b']},{c_floats(c['point_a'])},{c_floats(c['point_b'])},"
                f"{c_float(c['radius_m'])},{c_float(c['radius_b_m'])},{c['flags']}}}" for c in piece["colliders"]) + "};\n")
        material = ",".join(f".{k}={v}" if isinstance(v, int) else f".{k}={c_float(v)}" for k, v in piece["material"].items())
        rows.append(f'{{"{piece["id"]}",{s}_bones,{len(piece["bones"])},&{s}_mesh,{s}_vertices,{{{material}}},{colliders},'
                    f'{len(piece["colliders"])},{piece["render_vertices"]},{c_float(piece["linear_inertia"])},'
                    f'{c_float(piece["angular_inertia"])},{c_float(piece["downward_follow"])}}}')
    out.write("static const GarmentAsset expected_pieces[] = {" + ",".join(rows) + "};\n")
    out.write(f'static const GarmentSet expected_set = {{"{garment["outfit_id"]}",expected_pieces,{len(rows)}}};\n')
    limits = (("CLOTH_VERTICES", "VERTICES_MAX"), ("CLOTH_EDGES", "EDGES_MAX"), ("CLOTH_MAX_STRAIN_PASSES", "STRAIN_PASSES_MAX"),
              ("GARMENT_PIECES_MAX", "PIECES_MAX"), ("GARMENT_BONES_MAX", "BONES_MAX"),
              ("GARMENT_CAPSULES_MAX", "CAPSULES_MAX"), ("GARMENT_BLOB_ALIGN", "ALIGN"), ("GARMENT_BLOB_VERSION", "VERSION"))
    for c_name, py_name in limits:
        out.write(f'_Static_assert({c_name} == {getattr(garment_blob, py_name)}, "garment_blob.py {py_name} differs from {c_name}");\n')
    out.write(f'#define GARMENT_FIXTURE_BLOB "{blob_path.as_posix()}"\n#define GARMENT_FIXTURE_EMPTY "{empty_path.as_posix()}"\n')


if __name__ == "__main__":
    if sys.argv[1:2] == ["--emit"]:
        header, blob, empty = (Path(p).resolve() for p in sys.argv[2:5])
        blob.write_bytes(build(synthetic()))
        empty.write_bytes(encode("plain", []))
        with header.open("w", encoding="ascii", newline="\n") as out:
            emit_expected(out, synthetic(), blob, empty)
    else:
        unittest.main()
