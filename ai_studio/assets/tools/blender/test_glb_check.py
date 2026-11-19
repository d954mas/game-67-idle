import unittest
import tempfile
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender import glb_check
from ai_studio.assets.tools.blender.lib import glb as G

CUBE = np.array([[x, y, z] for x in (-0.5, 0.5) for y in (0, 1) for z in (-0.5, 0.5)], float)
FACES = np.array([[1, 2, 0], [3, 2, 1], [6, 5, 4], [6, 7, 5], [4, 1, 0], [4, 5, 1],
                  [3, 6, 2], [7, 6, 3], [2, 4, 0], [6, 4, 2], [5, 3, 1], [5, 7, 3]])


def normals(points):
    return points / np.linalg.norm(points, axis=1, keepdims=True)


def cube(offset=(0, 0, 0), normal_scale=1.0):
    b = G.GlbBuilder()
    points = CUBE + offset
    b.add_node("box", mesh=b.add_mesh("box", points, FACES, normals=normals(CUBE - [0, 0.5, 0]) * normal_scale))
    return b


def skinned(joints, weights, bind_offset=0.0):
    """Cube weighted to a two-joint chain; `bind_offset` moves the rest away from the bind pose."""
    b = G.GlbBuilder()
    mesh = b.add_mesh("body", CUBE, FACES, extra={"JOINTS_0": (joints[:, :4], G.UNSIGNED_BYTE),
                                                    "WEIGHTS_0": (weights[:, :4], G.FLOAT),
                                                    **({"JOINTS_1": (joints[:, 4:], G.UNSIGNED_BYTE),
                                                        "WEIGHTS_1": (weights[:, 4:], G.FLOAT)}
                                                       if joints.shape[1] > 4 else {})})
    root = b.add_node("Hips", translation=(0, 1, 0))
    b.add_node("Spine", translation=(0, 0.5 + bind_offset, 0), parent=root)
    ibm = [np.linalg.inv(G.trs_matrix((0, y, 0), (0, 0, 0, 1), (1, 1, 1))).T.reshape(-1) for y in (1, 1.5)]
    b.json["skins"] = [{"joints": [0, 1], "inverseBindMatrices": b.add_accessor(np.array(ibm), kind="MAT4")}]
    b.add_node("body", mesh=mesh, skin=0)
    return b


def found(report, severity=None):
    return {f["check"] for f in report["findings"] if severity is None or f["severity"] == severity}


def serious(report):
    return found(report, "medium") | found(report, "high")


class Contract(unittest.TestCase):
    def test_clean_cube_with_floor_pivot(self):
        report = glb_check.check(cube().to_glb(), pivot="floor")
        self.assertEqual(serious(report), set())
        self.assertEqual(report["metrics"]["bounds_centre"], [0, 0.5, 0])

    def test_bad_pivot_reports_height_and_bounds_centre_separately(self):
        glb = cube(offset=(0.39, 0.53, 0)).to_glb()
        report = glb_check.check(glb, pivot="floor-center")
        self.assertTrue({"pivot", "pivot_horizontal"} <= found(report, "medium"))
        floor = glb_check.check(glb, pivot="floor")
        self.assertEqual(found(floor, "medium"), {"pivot"})
        self.assertIn("origin_offset", found(floor, "info"))
        advisory = glb_check.check(glb)
        self.assertEqual(found(advisory), {"origin_offset"})
        self.assertEqual(serious(glb_check.check(cube(offset=(0, -0.5, 0)).to_glb(), pivot="center")), set())

    def test_normals_and_nan(self):
        self.assertIn("normals", serious(glb_check.check(cube(normal_scale=1.1).to_glb())))
        b = cube()
        glb = b.to_glb()
        data = bytearray(glb.bin)
        data[0:4] = np.array([np.nan], "<f4").tobytes()
        glb.bin = bytes(data)
        self.assertIn("finite", found(glb_check.check(glb), "high"))

    def test_images_and_uris(self):
        b = cube()
        jpeg = b"\xff\xd8\xff\xe0" + b"\0" * 12
        b.payload.extend(jpeg)
        b.json["bufferViews"].append({"buffer": 0, "byteOffset": len(b.payload) - len(jpeg), "byteLength": len(jpeg)})
        b.json["images"] = [{"bufferView": len(b.json["bufferViews"]) - 1}, {"uri": "wood.png"}]
        report = glb_check.check(b.to_glb())
        self.assertIn("jpeg", found(report, "low"))
        self.assertIn("uri", found(report, "high"))
        self.assertIn("jpeg", found(glb_check.check(b.to_glb(), jpeg="info"), "info"))
        self.assertIn("textures", found(glb_check.check(b.to_glb(), textures="none")))

    def test_colour_encoding_and_identity_nodes(self):
        b = G.GlbBuilder()
        b.add_node("box", mesh=b.add_mesh("box", CUBE, FACES, colors=np.ones((8, 4))), translation=(1, 0, 0))
        report = glb_check.check(b.to_glb(), color_encoding="u8", identity_nodes=True)
        self.assertTrue({"color_encoding", "node_transform"} <= serious(report))
        self.assertEqual(serious(glb_check.check(b.to_glb(), color_encoding="float")), set())

    def test_colour_encoding_u16(self):
        b = G.GlbBuilder()
        colors = np.full((8, 4), 65535, np.uint16)
        b.add_node("box", mesh=b.add_mesh("box", CUBE, FACES, extra={"COLOR_0": (colors, G.UNSIGNED_SHORT)}))
        b.json["accessors"][b.json["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"]]["normalized"] = True
        for encoding in ("u16", "normalized", "any"):
            self.assertNotIn("color_encoding", found(glb_check.check(b.to_glb(), color_encoding=encoding)))
        for encoding in ("u8", "float"):
            self.assertIn("color_encoding", found(glb_check.check(b.to_glb(), color_encoding=encoding)))


class Skins(unittest.TestCase):
    def test_clean_skin(self):
        joints = np.tile([0, 1, 0, 0], (8, 1))
        weights = np.tile([0.5, 0.5, 0, 0], (8, 1))
        report = glb_check.check(skinned(joints, weights).to_glb())
        self.assertEqual(serious(report), set())

    def test_weight_defects(self):
        joints = np.tile([0, 1, 0, 1, 0, 1, 0, 0], (8, 1))
        weights = np.tile([0.3, 0.2, 0.2, 0.15, 0.15, 0, 0, 0], (8, 1))
        weights[0] = 0
        weights[1, :2] = [0.9, 0.3]
        joints[2, 0] = 7
        report = glb_check.check(skinned(joints, weights).to_glb())
        self.assertTrue({"influences", "unweighted", "weight_sum", "joint_index"} <= serious(report))

    def test_rest_off_bind_and_reference_rig(self):
        joints = np.tile([0, 1, 0, 0], (8, 1))
        weights = np.tile([0.5, 0.5, 0, 0], (8, 1))
        moved = skinned(joints, weights, bind_offset=0.01).to_glb()
        self.assertIn("bind_pose", serious(glb_check.check(moved)))
        reference = skinned(joints, weights).to_glb()
        report = glb_check.check(moved, reference_rig=reference)
        self.assertIn("rig_rest", found(report, "high"))
        self.assertNotIn("rig_rest", found(glb_check.check(reference, reference_rig=reference)))

    def test_seam_twins_with_different_weights(self):
        points = np.vstack([CUBE, CUBE[:1]])
        faces = np.vstack([FACES, [[8, 1, 2]]])
        joints = np.tile([0, 1, 0, 0], (9, 1))
        weights = np.tile([1.0, 0, 0, 0], (9, 1))
        weights[8] = [0, 1, 0, 0]
        b = skinned(np.tile([0, 0, 0, 0], (8, 1)), np.tile([1.0, 0, 0, 0], (8, 1)))
        b.json["meshes"][0]["primitives"].append({"attributes": {
            "POSITION": b.add_accessor(points, target=G.ARRAY_BUFFER),
            "JOINTS_0": b.add_accessor(joints, G.UNSIGNED_BYTE), "WEIGHTS_0": b.add_accessor(weights)},
            "indices": b.add_accessor(faces.reshape(-1), G.UNSIGNED_SHORT, "SCALAR")})
        self.assertIn("seam_weights", serious(glb_check.check(b.to_glb())))


class Robustness(unittest.TestCase):
    def test_skinned_mesh_is_measured_skinned_not_by_its_node(self):
        b = G.GlbBuilder()
        n = len(CUBE)
        mesh = b.add_mesh("body", CUBE * [1, 1.8, 1], FACES, extra={"JOINTS_0": (np.zeros((n, 4)), G.UNSIGNED_BYTE),
                                                                    "WEIGHTS_0": (np.tile([1.0, 0, 0, 0], (n, 1)), G.FLOAT)})
        armature = b.add_node("Armature", scale=(0.01, 0.01, 0.01))
        hips = b.add_node("Hips", parent=armature)
        ibm = b.add_accessor(np.linalg.inv(np.diag([0.01, 0.01, 0.01, 1.0])).T.reshape(1, 16), kind="MAT4")
        b.json["skins"] = [{"joints": [hips], "inverseBindMatrices": ibm}]
        b.add_node("body", mesh=mesh, parent=armature, skin=0)
        report = glb_check.check(b.to_glb(), pivot="floor", identity_nodes=True)
        self.assertAlmostEqual(report["metrics"]["size"][1], 1.8, places=4)
        self.assertNotIn("pivot", found(report))
        moved = next(f for f in report["findings"] if f["check"] == "node_transform")
        self.assertEqual(moved["where"]["nodes"], ["Armature"])

    def test_truncated_accessor_is_a_finding_not_a_crash(self):
        b = cube()
        b.json["accessors"][1]["count"] = 100000
        self.assertIn("accessor", found(glb_check.check(b.to_glb()), "high"))

    def test_broken_references_and_compression_are_findings(self):
        b = cube()
        b.json["nodes"][0]["mesh"] = 7
        self.assertIn("check_error", found(glb_check.check(b.to_glb()), "high"))
        b = cube()
        b.json["extensionsUsed"] = ["KHR_draco_mesh_compression"]
        with self.assertRaises(G.GlbError):
            b.to_glb()
        with tempfile.TemporaryDirectory() as tmp:
            path = b.write(Path(tmp) / "draco.glb")
            self.assertEqual(found(glb_check.check(path)), {"unreadable"})

    def test_mirror_is_an_odd_number_of_negative_axes(self):
        for scale, mirrored in (((-1, 1, 1), True), ((-1, -1, 1), False)):
            b = G.GlbBuilder()
            b.add_node("box", mesh=b.add_mesh("box", CUBE, FACES), scale=scale)
            self.assertEqual("node_scale" in found(glb_check.check(b.to_glb())), mirrored, scale)


class BindAndPivot(unittest.TestCase):
    def test_centimetre_mesh_under_scaled_armature_is_bound(self):
        b = G.GlbBuilder()
        n = len(CUBE)
        mesh = b.add_mesh("body", CUBE * [50, 170, 50], FACES, extra={
            "JOINTS_0": (np.zeros((n, 4)), G.UNSIGNED_BYTE), "WEIGHTS_0": (np.tile([1.0, 0, 0, 0], (n, 1)), G.FLOAT)})
        armature = b.add_node("Armature", scale=(0.01, 0.01, 0.01))
        hips = b.add_node("Hips", parent=armature)
        b.json["skins"] = [{"joints": [hips], "inverseBindMatrices": b.add_accessor(np.eye(4).reshape(1, 16),
                                                                                    kind="MAT4")}]
        b.add_node("body", mesh=mesh, parent=armature, skin=0)
        report = glb_check.check(b.to_glb())
        self.assertNotIn("bind_pose", found(report))
        self.assertAlmostEqual(report["metrics"]["size"][1], 1.7, places=4)


if __name__ == "__main__":
    unittest.main()
