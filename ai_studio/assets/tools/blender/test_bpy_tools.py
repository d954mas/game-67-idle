"""Blender-side library and review renderer, run in a real Blender; skipped where none is installed."""
import json
import shutil
import tempfile
import textwrap
import unittest
from pathlib import Path

import numpy as np

from ai_studio.assets.tools.blender import run_blender
from ai_studio.assets.tools.blender.lib import glb as G

HERE = Path(__file__).resolve().parent
CUBE = np.array([[x, y, z] for x in (-0.5, 0.5) for y in (0, 1) for z in (-0.25, 0.25)], float)
CUBE_FACES = np.array([[0, 1, 2], [1, 3, 2], [4, 6, 5], [5, 6, 7], [0, 4, 1], [1, 4, 5],
                       [2, 3, 6], [3, 7, 6], [0, 2, 4], [2, 6, 4], [1, 5, 3], [3, 5, 7]])

SMOKE = textwrap.dedent("""
    import json, sys
    import bmesh, bpy
    from ai_studio.assets.tools.blender.lib import bpy_io, bpy_scene as S
    out = sys.argv[sys.argv.index("--") + 1]
    S.reset_scene()
    kit = S.collection("kit")
    red = S.palette_material("red", "#c03020")
    S.box("crate", (2, 1, 0.5), location=(3, 4, 5), material=red, coll=kit)
    S.cylinder("post", 0.5, 2.0, 12, location=(3, 4, 5), material=S.vertex_colour_material("vc"), coll=kit)
    S.capsule("pill", 0.2, 1.0, location=(3, 4, 5), coll=kit)
    S.sphere("ball", 0.3, location=(3, 4, 5), coll=kit)
    S.prism("wedge", [(0, 0), (0.4, 0), (0, 0.4)], 0.2, location=(3, 4, 5), coll=kit)
    open_edges = {}
    for obj in kit.objects:
        bm = bmesh.new(); bm.from_mesh(obj.data)
        open_edges[obj.name] = sum(1 for e in bm.edges if not e.is_manifold); bm.free()
    tris = S.tri_count()
    pivot = bpy_io.apply_pivot(rule="floor")
    static = bpy_io.export_glb(out + "/kit.glb", preset="static")
    S.reset_scene()
    fresh = bpy_io.export_glb(out + "/fresh.glb", [S.box("fresh", 1.0)], preset="static")
    S.reset_scene()
    lid = S.box("lid", 1.0)
    for frame, z in ((1, 0.0), (25, 2.0)):
        lid.location.z = z
        lid.keyframe_insert("location", index=2, frame=frame)
    lid.animation_data.action.name = "Rise"
    animated = bpy_io.export_glb(out + "/anim.glb", preset="animated")
    json.dump({"open_edges": open_edges, "tris": tris, "pivot": pivot, "static": static, "fresh": fresh,
               "animated": animated, "eevee": S.eevee_engine()}, open(out + "/smoke.json", "w"))
""")


def setUpModule():
    try:
        run_blender.find_blender()
    except FileNotFoundError as error:
        raise unittest.SkipTest(str(error))


class BlenderCase(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="bpy_tools_"))
        self.addCleanup(shutil.rmtree, self.tmp, True)

    def blender(self, script, *args):
        log = self.tmp / f"{Path(script).stem}.log"
        result = run_blender.run(script, [str(a) for a in args], log=log, timeout=600)
        if result["code"] != 0:
            self.fail(f"blender exited {result['code']}:\n" + log.read_text(errors="replace")[-3000:])
        return result


class RenderViews(BlenderCase):
    def test_defaults_render_every_view_and_pass_with_stats(self):
        model = self.tmp / "cube.glb"
        b = G.GlbBuilder()
        b.add_node("cube", mesh=b.add_mesh("cube", CUBE, CUBE_FACES))
        b.write(model)
        out = self.tmp / "views"
        self.blender(HERE / "render_views.py", "--model", model, "--out", out, "--size", 128)
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        self.assertEqual(report["stats"]["triangles"], 12)
        np.testing.assert_allclose(report["stats"]["bounds"]["min"], CUBE.min(axis=0), atol=1e-4)
        np.testing.assert_allclose(report["stats"]["bounds"]["max"], CUBE.max(axis=0), atol=1e-4)
        self.assertEqual(report["sha256"], G.sha256(model))
        names = {f"{v}_{p}.png" for v in ("front", "right", "three_quarter") for p in ("beauty", "clay", "wire")}
        self.assertEqual({i["file"] for i in report["images"]}, names)
        for name in names | {"sheet.png"}:
            self.assertEqual((out / name).read_bytes()[:8], b"\x89PNG\r\n\x1a\n", name)


class SceneAndExport(BlenderCase):
    def test_primitives_pivot_export_and_animation_pose(self):
        script = self.tmp / "smoke.py"
        script.write_text(SMOKE, encoding="utf-8")
        self.blender(script, self.tmp)
        smoke = json.loads((self.tmp / "smoke.json").read_text(encoding="utf-8"))
        self.assertEqual(set(smoke["open_edges"].values()), {0}, smoke["open_edges"])

        static = smoke["static"]
        self.assertEqual(static["pivot"], "floor")
        self.assertEqual(static["triangles"], smoke["tris"])
        # An object exported right after creation must not come out empty.
        self.assertEqual(smoke["fresh"]["triangles"], 12)
        self.assertEqual(static["sha256"], G.sha256(self.tmp / "kit.glb"))
        positions, faces, _, _ = G.Glb.load(self.tmp / "kit.glb").merged()
        self.assertEqual(len(faces), smoke["tris"])
        np.testing.assert_allclose(positions.min(axis=0), [-1, 0, -0.5], atol=1e-4)
        np.testing.assert_allclose(positions.max(axis=0), [1, 2, 0.5], atol=1e-4)
        np.testing.assert_allclose(static["bounds_centre"], [0, 1, 0], atol=1e-4)

        # The render must show the clip at the same glTF time the file stores, so the last key
        # (lid fully risen, bottom at 1.5) is asked for by its own time, not a frame number.
        end = G.Glb.load(self.tmp / "anim.glb").duration("Rise")
        out = self.tmp / "posed"
        self.blender(HERE / "render_views.py", "--model", self.tmp / "anim.glb", "--out", out,
                     "--views", "front,turntable", "--turntable-frames", 2, "--passes", "silhouette",
                     "--size", 64, "--animation", "Rise", "--time", end, "--background", "#204060")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        self.assertEqual(report["pose"], f"Rise@{end:g}s")
        self.assertAlmostEqual(report["stats"]["bounds"]["min"][1], 1.5, places=3)
        self.assertEqual(len(report["images"]), 3)


if __name__ == "__main__":
    unittest.main()
