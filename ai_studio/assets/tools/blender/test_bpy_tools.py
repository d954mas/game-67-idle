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


IN_PROCESS = textwrap.dedent("""
    import json, sys
    from pathlib import Path
    import bpy
    from ai_studio.assets.tools.blender import render_views as RV
    from ai_studio.assets.tools.blender.lib import bpy_scene as S, review_live
    model, out = sys.argv[sys.argv.index("--") + 1:]
    out = Path(out)
    first = RV.main(["--model", model, "--out", str(out / "first"), "--size", "64", "--passes", "silhouette"])
    second = RV.main(["--model", model, "--out", str(out / "second"), "--size", "64", "--views", "front"])
    S.reset_scene()
    crate = S.box("crate", (1, 1, 2), anchor="bottom", material=S.palette_material("wood", "#8a5a33"))
    scene = bpy.context.scene
    def state():
        return {"engine": scene.render.engine, "res": [scene.render.resolution_x, scene.render.resolution_y],
                "camera": scene.camera.name if scene.camera else None, "frame": scene.frame_current,
                "world": scene.world.name if scene.world else None, "hidden": crate.hide_render,
                "counts": {k: len(getattr(bpy.data, k)) for k in ("objects", "materials", "worlds", "cameras",
                                                                  "images", "meshes", "lights")}}
    before = state()
    live = review_live.review_scene(out / "live", name="crate", job={"size": 64, "views": ["front"]})
    after = state()
    json.dump({"first": first, "second": second, "live": live, "before": before, "after": after},
              open(out / "in_process.json", "w"))
""")


def cube_glb(path, height=1.0, clip=None):
    """The CUBE fixture scaled to `height`; `clip` = (name, times, y offsets) adds a translation
    clip on its node; with height None the file has the node and clip only (a clip-only file)."""
    b = G.GlbBuilder()
    mesh = b.add_mesh("cube", CUBE * [1, height, 1], CUBE_FACES) if height else None
    b.add_node("cube", mesh=mesh)
    if clip:
        name, times, ys = clip
        values = np.array([[0.0, y, 0.0] for y in ys])
        b.json["animations"] = [{"name": name, "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}],
                                 "samplers": [{"input": b.add_accessor(np.array(times, float), bounds=True),
                                               "output": b.add_accessor(values), "interpolation": "LINEAR"}]}]
    b.write(path)
    return path


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


    def test_lineup_renders_the_group_at_true_scale_beside_the_model(self):
        model = cube_glb(self.tmp / "cube.glb")
        tall = cube_glb(self.tmp / "tall.glb", height=3.0)
        out = self.tmp / "lineup"
        self.blender(HERE / "render_views.py", "--model", model, "--out", out, "--size", 96,
                     "--beside", tall, "--views", "front", "--passes", "clay")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        lineup = [i for i in report["images"] if i["view"] == "lineup"]
        self.assertEqual({i["base_view"] for i in lineup}, {"front", "three_quarter"})
        for image in lineup:
            self.assertEqual(image["members"], ["cube", "tall"])
            self.assertEqual(image["pass"], "beauty")
            self.assertEqual((out / image["file"]).read_bytes()[:4], b"\x89PNG")
        tall_member = report["lineup"]["members"][1]
        np.testing.assert_allclose(tall_member["size_gltf"], [1, 3, 0.5], atol=1e-4)
        # Bottoms level with the model, standing past its +X side after a 15% gap.
        self.assertAlmostEqual(tall_member["offset_gltf"][1], 0.0, places=4)
        self.assertAlmostEqual(tall_member["offset_gltf"][0], 1.0 + 0.15, places=4)
        # The model-only views leave the reference out.
        self.assertEqual({i["view"] for i in report["images"]} - {"lineup"}, {"front"})
        np.testing.assert_allclose(report["stats"]["bounds"]["max"], CUBE.max(axis=0), atol=1e-4)

    def test_clip_source_poses_a_body_by_node_name(self):
        body = cube_glb(self.tmp / "body.glb")
        clip = cube_glb(self.tmp / "clip.glb", height=None, clip=("Rise", [0.0, 1.0], [0.0, 1.5]))
        out = self.tmp / "clip_pose"
        self.blender(HERE / "render_views.py", "--model", body, "--out", out, "--size", 64,
                     "--clip-source", clip, "--poses", "Rise@1.0", "Rise@0.5", "--views", "front",
                     "--passes", "silhouette", "--pose-passes", "silhouette")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        self.assertEqual(report["clip_source"], {"animations": ["Rise"], "unmatched_nodes": []})
        lows = {p["time"]: p["bounds"]["min"][1] for p in report["poses"]}
        self.assertAlmostEqual(lows[1.0], 1.5, places=3)
        self.assertAlmostEqual(lows[0.5], 0.75, places=3)
        self.assertAlmostEqual(report["stats"]["bounds"]["min"][1], 0.0, places=4)


class InProcess(unittest.TestCase):
    """render_views.main twice and review_live in one Blender process, as a server or a build
    script uses them; one Blender start serves every test here."""

    @classmethod
    def setUpClass(cls):
        try:
            run_blender.find_blender()
        except FileNotFoundError as error:
            raise unittest.SkipTest(str(error))
        cls.root = Path(tempfile.mkdtemp(prefix="bpy_inproc_"))
        model = cube_glb(cls.root / "cube.glb")
        script = cls.root / "in_process.py"
        script.write_text(IN_PROCESS, encoding="utf-8")
        log = cls.root / "in_process.log"
        result = run_blender.run(script, [str(model), str(cls.root)], log=log, timeout=600)
        if result["code"] != 0:
            tail = log.read_text(errors="replace")[-3000:]
            shutil.rmtree(cls.root, True)
            raise AssertionError(f"blender exited {result['code']}:\n{tail}")
        cls.data = json.loads((cls.root / "in_process.json").read_text(encoding="utf-8"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.root, True)

    def test_main_returns_reports_and_runs_twice_in_one_process(self):
        first, second = self.data["first"]["models"][0], self.data["second"]["models"][0]
        self.assertEqual([i["file"] for i in first["images"]],
                         [f"{v}_silhouette.png" for v in ("front", "right", "three_quarter")])
        self.assertEqual([i["file"] for i in second["images"]],
                         ["front_beauty.png", "front_clay.png", "front_wire.png"])
        self.assertEqual(first["stats"]["triangles"], second["stats"]["triangles"])
        for run, report in (("first", first), ("second", second)):
            for image in report["images"]:
                self.assertTrue((self.root / run / image["file"]).is_file(), image)

    def test_review_live_exports_renders_checks_and_restores_the_scene(self):
        live = self.data["live"]
        self.assertTrue(Path(live["asset"]).is_file())
        self.assertEqual(live["glb"]["triangles"], 12)
        self.assertEqual(set(live["checks"]), {"mesh_check", "glb_check"})
        self.assertEqual(live["check_errors"], {})
        for path in live["reports"] + [live["views"]]:
            self.assertTrue(Path(path).is_file(), path)
        self.assertEqual(live["images"], 3)
        if live["card"]:
            self.assertTrue(Path(live["card"]).is_file())
        else:
            self.assertIn("venv", live["card_note"])
        self.assertEqual(self.data["before"], self.data["after"])


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

        # --poses reads the same glTF time; the main views stay in the bind pose.
        out = self.tmp / "poses"
        self.blender(HERE / "render_views.py", "--model", self.tmp / "anim.glb", "--out", out, "--views", "front",
                     "--passes", "silhouette", "--size", 64, "--poses", f"Rise@{end}", "Rise@0")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        self.assertEqual(report["pose"], "rest")
        lows = [p["bounds"]["min"][1] for p in report["poses"]]
        self.assertAlmostEqual(lows[0], 1.5, places=3)
        self.assertAlmostEqual(lows[1], -0.5, places=3)
        poses = [i for i in report["images"] if i["view"] == "pose"]
        self.assertEqual([(i["animation"], i["time"], i["base_view"], i["pass"]) for i in poses],
                         [("Rise", end, "three_quarter", "beauty"), ("Rise", 0.0, "three_quarter", "beauty")])
        for image in poses:
            self.assertTrue((out / image["file"]).is_file(), image)


if __name__ == "__main__":
    unittest.main()
