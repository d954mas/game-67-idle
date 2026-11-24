import copy
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import garment_blob
from garment_blob import MATERIAL_FIELDS, decode, encode, single
from garment_cook import cook_mesh

IDENTITY = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0]


def synthetic():
    """Two pieces: one with every optional section, one with none; the outfit has shapes."""
    panel = cook_mesh("panel", [[-0.5, 0, 0], [0.5, 0, 0], [0, 1, 0.1], [0, 0.3, 1]], [[0, 1, 2], [1, 0, 3]], [0, 1],
                      wearable=True, tether_slack=1.05, max_distance=[0, 0, 0.05, 1 / 7],
                      backstop=[[0, 0], [0, 0], [0.004, 0.09], [0.01, 0.2]])
    sash = cook_mesh("sash", [[0, 0, 0], [0.2, 0, 0], [0.1, -0.4, 0.05]], [[0, 1, 2]], [0, 1], wearable=True)
    material = {name: (i + 1 if name in ("iterations", "strain_passes") else 0.01 * (i + 1) + 1 / 7)
                for i, name in enumerate(MATERIAL_FIELDS)}
    material.update(friction=0.6, compression_limit=0.2)
    return dict(outfit_id="synthetic",
                bones=[("Root", [0.1 * i + 1 / 9 for i in range(12)]), ("Hem", IDENTITY), ("SashEnd", IDENTITY)],
                spheres=[(0, [-0.062, 1.005, 0.004], 0.084), (1, [0.062, 1.005, 0.004], 0.07), (1, [0.1, 0.2, 0.3], 0.05)],
                cones=[(0, 1)],
                pieces=[
                    dict(id="panel", mesh=panel, material=material, render_vertices=7, linear_inertia=0.45,
                         angular_inertia=1 / 3,
                         skin=dict(bone=[[0, 1, 0, 0], [1, 0, 0, 0], [1, 1, 0, 0], [0, 0, 1, 1]],
                                   weight=[[1, 0, 0, 0], [0.75, 0.25, 0, 0], [1 / 3, 2 / 3, 0, 0], [0.5, 0, 0.5, 0]]),
                         direction=[[0, 0, 1], [0, 0.6, 0.8], [1, 0, 0], [0, -1, 0]]),
                    dict(id="sash", mesh=sash, material=dict(density=0.3, iterations=1),
                         skin=dict(bone=[[2, 2, 2, 2]] * 3, weight=[[1, 0, 0, 0]] * 3), direction=[[0, 0, 1]] * 3),
                ])


def build(garment):
    return encode(garment["outfit_id"], garment["pieces"], bones=garment["bones"], spheres=garment["spheres"],
                  cones=garment["cones"])


class TestBlob(unittest.TestCase):
    def test_reader_returns_what_the_writer_wrote(self):
        garment = synthetic()
        data = build(garment)
        self.assertEqual(len(data) % 4, 0)
        back = decode(data)
        self.assertEqual(back["id"], "synthetic")
        pieces = [dict(p, mesh=dict(p["mesh"], edges=p["mesh"]["edges"].tolist(),
                                    tethers=None if p["mesh"]["tethers"] is None else p["mesh"]["tethers"].tolist()),
                       skin=dict(bone=p["skin"]["bone"], weight=p["skin"]["weight"])) for p in back["pieces"]]
        again = encode(back["id"], pieces, bones=back["bones"],
                       spheres=[(s["bone"], s["center"], s["radius"]) for s in back["spheres"]],
                       cones=[(c["a"], c["b"]) for c in back["cones"]])
        self.assertEqual(again, data)
        panel, source = back["pieces"][0], garment["pieces"][0]
        np.testing.assert_array_equal(panel["mesh"]["rest"], single(source["mesh"]["rest"]))
        np.testing.assert_array_equal(panel["skin"]["weight"], single(source["skin"]["weight"]))
        np.testing.assert_array_equal(panel["mesh"]["max_distance"], single(source["mesh"]["max_distance"]))
        self.assertEqual(panel["material"]["iterations"], source["material"]["iterations"])
        self.assertEqual(back["pieces"][1]["mesh"]["tethers"], None)
        self.assertEqual(len(back["spheres"]), 3)
        np.testing.assert_array_equal(back["pieces"][1]["mesh"]["backstop"], np.zeros((3, 2), np.float32))

    def test_outfit_without_cloth_round_trips(self):
        back = decode(encode("plain", []))
        self.assertEqual((back["id"], back["bones"], back["pieces"]), ("plain", [], []))
        self.assertEqual(len(back["spheres"]) + len(back["cones"]), 0)

    def test_single_reads_ten_significant_digits_like_a_c_literal(self):
        # Just below a single-precision midpoint the 10-digit literal 1.000000060 rounds up; a direct cast rounds down.
        value = 1 + 2 ** -24 - 2 ** -40
        self.assertEqual(single(value), np.float32(1 + 2 ** -23))
        self.assertEqual(np.float32(value), np.float32(1))
        self.assertTrue(np.isnan(single(float("nan"))))

    def test_cook_rejects_values_the_solver_trusts(self):
        cases = [
            (lambda g, p: p["mesh"]["rest"].__setitem__(2, [np.nan, 0, 0]), "panel: mesh.rest[2]"),
            (lambda g, p: p["mesh"]["inv_area"].__setitem__(3, 0), "panel: mesh.inv_area[3]"),
            (lambda g, p: p["mesh"]["edges"].__setitem__(0, (0, 9, 2, -1, 1.0, 0.0, 0)), "panel: mesh.edges[0]"),
            (lambda g, p: p["mesh"]["edges"].__setitem__(1, (0, 1, 2, -1, 0.0, 0.0, 0)), "panel: mesh.edges.length[1]"),
            (lambda g, p: p["mesh"]["edges"].__setitem__(1, (0, 1, 2, -1, 1.0, 3.2, 0)), "panel: mesh.edges.angle[1]"),
            (lambda g, p: p["mesh"]["edges"].__setitem__(1, (0, 1, 2, -1, 1.0, 0.0, 3)), "panel: mesh.edges.kind[1]"),
            (lambda g, p: p["mesh"]["faces"].__setitem__(1, (1, 1, 3)), "panel: mesh.faces[1]"),
            (lambda g, p: p["mesh"]["tethers"].__setitem__(3, (2, 0.1)), "panel: mesh.tethers.pin[3] must name a kinematic"),
            (lambda g, p: p["mesh"]["max_distance"].__setitem__(2, -0.01), "panel: mesh.max_distance[2]"),
            (lambda g, p: p["mesh"]["max_distance"].__setitem__(3, 1.5), "panel: mesh.max_distance[3]"),
            (lambda g, p: p["mesh"]["backstop"].__setitem__(2, [0, np.inf]), "panel: mesh.backstop[2]"),
            (lambda g, p: p["skin"]["bone"].__setitem__(1, [3, 0, 0, 0]), "panel: skin.bone[1]"),
            (lambda g, p: p["skin"]["weight"].__setitem__(1, [1.25, -0.25, 0, 0]), "panel: skin.weight[1] must be finite"),
            (lambda g, p: p["skin"]["weight"].__setitem__(1, [0.5, 0.4, 0, 0]), "panel: skin.weight[1] must sum to 1"),
            (lambda g, p: p["direction"].__setitem__(2, [0, 0, 0.5]), "panel: direction[2] must be a finite unit"),
            (lambda g, p: p["material"].__setitem__("drag", True), "panel: material.drag must be a number"),
            (lambda g, p: p["material"].__setitem__("density", 0), "panel: material.density"),
            (lambda g, p: p["material"].__setitem__("friction", -0.5), "panel: material.friction"),
            (lambda g, p: p["material"].__setitem__("compression_limit", 0.6), "panel: material.compression_limit"),
            (lambda g, p: p["material"].__setitem__("max_speed_mps", 4.0), "panel: material has unknown fields"),
            (lambda g, p: p["material"].__setitem__("strain_passes", 33), "panel: material.strain_passes"),
            (lambda g, p: p["material"].__setitem__("iterations", 0), "panel: material.iterations"),
            (lambda g, p: p.__setitem__("linear_inertia", 1.5), "panel: linear_inertia"),
        ]
        self.assert_rejected(cases)

    def test_outfit_shapes_are_checked(self):
        self.assert_rejected([
            (lambda g, p: g["bones"].__setitem__(0, ("Root", [np.inf] * 12)), "bones.inverse_bind[0]"),
            (lambda g, p: g["spheres"].__setitem__(1, (3, [0, 0, 0], 0.1)), "spheres.bone[1]"),
            (lambda g, p: g["spheres"].__setitem__(0, (0, [0, 0, 0], -0.1)), "spheres.radius[0]"),
            (lambda g, p: g["cones"].__setitem__(0, (0, 0)), "cones[0] must join two different spheres"),
            (lambda g, p: g["cones"].__setitem__(0, (0, 3)), "cones[0]"),
            (lambda g, p: g["spheres"].extend([g["spheres"][0]] * 62), "shapes exceed"),
        ], prefix="synthetic: ")

    def test_integer_fields_reject_fractions_and_values_their_record_cannot_hold(self):
        self.assert_rejected([
            (lambda g, p: p["mesh"]["edges"].__setitem__(0, (0, 1.5, 2, -1, 1.0, 0.0, 0)), "panel: mesh.edges.b must be integers"),
            (lambda g, p: p["mesh"]["edges"].__setitem__(0, (0, 1, 2, -1, 1.0, 0.0, 256)), "panel: mesh.edges.kind[0] must be in"),
            (lambda g, p: p["mesh"].__setitem__("faces", np.asarray(p["mesh"]["faces"], np.float64)), "panel: mesh.faces must be integers"),
            (lambda g, p: p["skin"].__setitem__("bone", p["skin"]["bone"] + 65536), "panel: skin.bone[0] must be in"),
            (lambda g, p: p["material"].__setitem__("iterations", 2 ** 31), "panel: material.iterations must be in"),
            (lambda g, p: p.__setitem__("render_vertices", 7.0), "panel: render_vertices must be"),
        ])

    def assert_rejected(self, cases, prefix="synthetic/"):
        for edit, message in cases:
            garment = synthetic()
            panel = garment["pieces"][0]
            panel["mesh"] = copy.deepcopy(panel["mesh"])
            for key in ("rest", "inv_area", "max_distance", "backstop"):
                panel["mesh"][key] = np.array(panel["mesh"][key])
            panel["skin"] = {k: np.array(v) for k, v in panel["skin"].items()}
            panel["direction"] = np.array(panel["direction"], float)
            edit(garment, panel)
            with self.subTest(message), self.assertRaisesRegex(ValueError, "^" + prefix + message.replace("[", r"\[")):
                build(garment)


def c_float(value):
    return float(value).hex() + "F"


def c_floats(values):
    return "{" + ",".join(c_float(v) for v in np.asarray(values, np.float32).ravel()) + "}"


def emit_expected(out, garment, blob_path, empty_path):
    """The garment as C initializers with exact hexadecimal floats: what the C view must reproduce."""
    back = decode(build(garment))
    rows = []
    out.write("static const GarmentBone expected_bones[] = {"
              + ",".join(f'{{"{name}",{c_floats(matrix)}}}' for name, matrix in back["bones"]) + "};\n")
    out.write("static const GarmentSphere expected_spheres[] = {" + ",".join(
        f"{{{s['bone']},{c_floats(s['center'])},{c_float(s['radius'])}}}" for s in back["spheres"]) + "};\n")
    out.write("static const GarmentCone expected_cones[] = {" + ",".join(
        f"{{{c['a']},{c['b']}}}" for c in back["cones"]) + "};\n")
    for i, piece in enumerate(back["pieces"]):
        s, mesh = f"p{i}", piece["mesh"]
        out.write(f"static const float {s}_rest[][3] = {{" + ",".join(c_floats(p) for p in mesh["rest"]) + "};\n")
        out.write(f"static const float {s}_inv[] = {c_floats(mesh['inv_area'])};\n")
        out.write(f"static const ClothFace {s}_faces[] = {{" + ",".join("{%d,%d,%d}" % tuple(f) for f in mesh["faces"]) + "};\n")
        out.write(f"static const ClothEdge {s}_edges[] = {{" + ",".join(
            f"{{{e['a']},{e['b']},{e['c']},{e['d']},{c_float(e['length'])},{c_float(e['angle'])},{e['kind']}}}"
            for e in mesh["edges"]) + "};\n")
        tethers = "NULL"
        if mesh["tethers"] is not None:
            tethers = f"{s}_tethers"
            out.write(f"static const ClothTether {tethers}[] = {{" + ",".join(
                f"{{{t['pin']},{c_float(t['max_length'])}}}" for t in mesh["tethers"]) + "};\n")
        out.write(f"static const float {s}_r[] = {c_floats(mesh['max_distance'])};\n")
        out.write(f"static const ClothBackstop {s}_backstop[] = {{" + ",".join(
            f"{{{c_float(d)},{c_float(r)}}}" for d, r in mesh["backstop"]) + "};\n")
        out.write(f"static const ClothMesh {s}_mesh = {{{len(mesh['rest'])},{len(mesh['faces'])},{len(mesh['edges'])},"
                  f"{s}_rest,{s}_inv,{s}_faces,{s}_edges,{tethers},{s}_r,{s}_backstop}};\n")
        out.write(f"static const GarmentSkin {s}_skin[] = {{" + ",".join(
            "{{%s},%s}" % (",".join(map(str, v["bone"])), c_floats(v["weight"])) for v in piece["skin"]) + "};\n")
        out.write(f"static const float {s}_direction[][3] = {{" + ",".join(c_floats(d) for d in piece["direction"]) + "};\n")
        material = ",".join(f".{k}={v}" if isinstance(v, int) else f".{k}={c_float(v)}" for k, v in piece["material"].items())
        rows.append(f'{{"{piece["id"]}",&{s}_mesh,{s}_skin,{s}_direction,{{{material}}},{piece["render_vertices"]},'
                    f'{c_float(piece["linear_inertia"])},{c_float(piece["angular_inertia"])}}}')
    out.write("static const GarmentAsset expected_pieces[] = {" + ",".join(rows) + "};\n")
    out.write(f'static const GarmentSet expected_set = {{"{garment["outfit_id"]}",expected_bones,{len(back["bones"])},'
              f'expected_spheres,{len(back["spheres"])},expected_cones,{len(back["cones"])},expected_pieces,{len(rows)}}};\n')
    limits = (("CLOTH_VERTICES", "VERTICES_MAX"), ("CLOTH_EDGES", "EDGES_MAX"), ("CLOTH_MAX_STRAIN_PASSES", "STRAIN_PASSES_MAX"),
              ("GARMENT_PIECES_MAX", "PIECES_MAX"), ("GARMENT_BONES_MAX", "BONES_MAX"), ("CLOTH_SHAPES", "SHAPES_MAX"),
              ("GARMENT_BLOB_ALIGN", "ALIGN"), ("GARMENT_BLOB_VERSION", "VERSION"))
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
