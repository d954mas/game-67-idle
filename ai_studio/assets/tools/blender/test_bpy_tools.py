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
                "material": crate.material_slots[0].material.name,
                "counts": {k: len(getattr(bpy.data, k)) for k in ("objects", "materials", "worlds", "cameras",
                                                                  "images", "meshes", "lights")}}
    before = state()
    live = review_live.review_scene(out / "live", name="crate", job={"size": 64, "views": ["front"], "tint": "#ff0000"})
    after = state()
    # An assembly root: the GLB the checks read must hold the meshes the images show.
    root = bpy.data.objects.new("assembly_root", None)
    scene.collection.objects.link(root)
    part = S.box("part", 1.0, location=(0, 0, 3))
    part.parent = root
    assembly = review_live.review_scene(out / "assembly", objects=[root], name="assembly",
                                        job={"size": 64, "views": ["front"], "passes": ["silhouette"]})
    # A keyed object moved off its keys by the script must come out of a review where it was.
    lid = S.box("lid", 0.5, location=(4, 0, 0))
    for frame, z in ((1, 0.0), (25, 2.0)):
        lid.location.z = z
        lid.keyframe_insert("location", index=2, frame=frame)
    scene.frame_set(1)
    lid.location.z = 5.0
    review_live.review_scene(out / "lid", objects=[lid], name="lid", checks=False, card=False,
                             job={"size": 64, "views": ["front"], "passes": ["silhouette"]})
    json.dump({"first": first, "second": second, "live": live, "before": before, "after": after,
               "assembly": assembly, "lid_z": lid.location.z}, open(out / "in_process.json", "w"))
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


def pixels(path):
    """(h, w, 4) uint8 of a PNG; Pillow lives in the Studio venv, not in Blender."""
    from PIL import Image
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA")).astype(np.int32)


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
        result = run_blender.run(script, [str(a) for a in args], log=log, timeout=600, server=False)
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

    def test_lineup_rests_references_whose_names_collide(self):
        # Model and both references name their node "cube", so Blender imports the references
        # as cube.001 and cube.002; each must still stand at its file's rest, not at its clip.
        model = cube_glb(self.tmp / "cube.glb")
        refs = [cube_glb(self.tmp / f"anim{k}.glb", clip=("Rise", [0.0, 1.0], [0.0, 1.5])) for k in (1, 2)]
        out = self.tmp / "lineup_names"
        self.blender(HERE / "render_views.py", "--model", model, "--out", out, "--size", 48, "--beside", *refs,
                     "--views", "front", "--passes", "silhouette", "--lineup-views", "front",
                     "--lineup-passes", "silhouette")
        members = json.loads((out / "views.json").read_text(encoding="utf-8"))["lineup"]["members"][1:]
        self.assertEqual([m["name"] for m in members], ["anim1", "anim2"])
        for member in members:
            self.assertAlmostEqual(member["offset_gltf"][1], 0.0, places=5, msg=member)

    def test_facing_pass_shows_an_inside_out_shell_red(self):
        model = self.tmp / "shells.glb"
        b = G.GlbBuilder()
        b.add_node("outward", mesh=b.add_mesh("outward", CUBE, CUBE_FACES), translation=[-1, 0, 0])
        b.add_node("inward", mesh=b.add_mesh("inward", CUBE, CUBE_FACES[:, ::-1]), translation=[1, 0, 0])
        b.write(model)
        out = self.tmp / "facing"
        self.blender(HERE / "render_views.py", "--model", model, "--out", out, "--size", 96,
                     "--views", "front", "--passes", "facing", "--no-sheet")
        rgba = pixels(out / "front_facing.png")
        opaque = rgba[..., 3] > 200
        half = rgba.shape[1] // 2
        red = (rgba[..., 0] - rgba[..., 1]) * opaque
        outward, inward = red[:, :half][opaque[:, :half]], red[:, half:][opaque[:, half:]]
        self.assertGreater(len(outward), 100)
        self.assertGreater(len(inward), 100)
        self.assertLess(float(np.median(outward)), 20)
        self.assertGreater(float(np.median(inward)), 150)

    def test_floor_pose_size_and_fitted_lineup_game_view(self):
        model = cube_glb(self.tmp / "lid.glb", clip=("Rise", [0.0, 1.0], [0.0, 1.5]))
        tall = cube_glb(self.tmp / "tall.glb", height=3.0)
        out = self.tmp / "floor"
        # A game target far from the group: hero framing would show nothing of it.
        self.blender(HERE / "render_views.py", "--model", model, "--out", out, "--size", 48, "--views", "front",
                     "--passes", "beauty", "--poses", "Rise@0", "Rise@1", "--pose-size", 96, "--floor",
                     "--beside", tall, "--lineup-views", "game", "--game", '{"target": [40, 0, 40]}', "--no-sheet")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        np.testing.assert_allclose(report["stats"]["bounds"]["min"], CUBE.min(axis=0), atol=1e-4)
        self.assertEqual(report["floor"]["views"], "poses")
        # The main view has no floor: the band under the cube stays transparent.
        main = pixels(out / "front_beauty.png")
        self.assertEqual(int(main[-1, :, 3].max()), 0)
        poses = [i for i in report["images"] if i["view"] == "pose"]
        self.assertEqual({tuple(i["size"]) for i in poses}, {(96, 96)})
        for image in poses:
            self.assertGreater(int(pixels(out / image["file"])[-1, :, 3].max()), 0, image["file"])
        game = next(i for i in report["images"] if i["view"] == "lineup" and i["base_view"] == "game")
        self.assertEqual(game["size"], [round(48 * 16 / 9), 48])
        # Fitted to the group, the tall reference fills the frame height instead of a tiny tile.
        rows = np.nonzero(pixels(out / game["file"])[..., 3].max(axis=1) > 0)[0]
        self.assertGreater(len(rows), 0)
        self.assertGreater(rows.max() - rows.min(), 0.75 * game["size"][1])

    def test_ortho_game_camera_model_scale_and_game_angle(self):
        model = cube_glb(self.tmp / "cube.glb")
        tall = cube_glb(self.tmp / "tall.glb", height=3.0)
        out = self.tmp / "ortho"
        # Level ortho camera, 2 m of view height centred at y=0.5: an unscaled 1 m cube fills half
        # the image height, the cube at --model-scale 0.5 a quarter.
        game = {"ortho": True, "ortho_half_height": 1.0, "pitch": 0, "height": 0.5, "behind": 5,
                "target": [0, 0.5, 0]}
        self.blender(HERE / "render_views.py", "--model", model, "--out", out, "--size", 64,
                     "--views", "game", "game_angle", "--passes", "silhouette", "--game", json.dumps(game),
                     "--model-scale", 0.5, "--beside", f"{tall}@0.5", "--lineup-views", "front",
                     "--lineup-passes", "silhouette", "--no-sheet")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))

        def rows(view):
            image = next(i for i in report["images"] if i["view"] == view)
            filled = np.nonzero(pixels(out / image["file"])[..., 3].max(axis=1) > 0)[0]
            return filled.max() - filled.min() + 1
        self.assertAlmostEqual(rows("game"), 16, delta=2)
        # Fitted along the game angle, the small model fills the frame instead of a dot.
        self.assertGreater(rows("game_angle"), 0.8 * 64)
        self.assertEqual(report["model_scale"], 0.5)
        np.testing.assert_allclose(report["stats"]["bounds"]["max"], CUBE.max(axis=0), atol=1e-4)
        ref = report["lineup"]["members"][1]
        self.assertEqual((ref["name"], ref["scale"]), ("tall", 0.5))
        np.testing.assert_allclose(ref["size_gltf"], [0.5, 1.5, 0.25], atol=1e-4)

    def test_atlas_and_tint_colour_the_beauty_pass(self):
        from PIL import Image
        uv = (np.full((len(CUBE), 2), 0.5), G.FLOAT)

        def white_cube(path):
            b = G.GlbBuilder()
            b.add_node("cube", mesh=b.add_mesh("cube", CUBE, CUBE_FACES, extra={"TEXCOORD_0": uv}))
            b.json["materials"] = [{"name": "white", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1]}}]
            b.json["meshes"][0]["primitives"][0]["material"] = 0
            b.write(path)
            return path
        model, ref = white_cube(self.tmp / "prop.glb"), white_cube(self.tmp / "ref.glb")
        # The model carries UVs but no glTF material at all; the runtime binds the atlas.
        b = G.GlbBuilder()
        b.add_node("cube", mesh=b.add_mesh("cube", CUBE, CUBE_FACES, extra={"TEXCOORD_0": uv}))
        b.write(model)
        atlas = self.tmp / "palette.png"
        Image.new("RGB", (4, 4), (20, 200, 20)).save(atlas)
        out = self.tmp / "atlas"
        beside = json.dumps([{"path": str(ref), "tint": "#2020ff"}])
        self.blender(HERE / "render_views.py", "--model", model, "--out", out, "--size", 64, "--views", "front",
                     "--passes", "beauty", "--atlas", atlas, "--beside", beside, "--lineup-views", "front",
                     "--no-sheet")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        self.assertEqual(Path(report["atlas"]), atlas.resolve())
        self.assertEqual(report["lineup"]["members"][1]["tint"], "#2020ff")
        self.assertEqual(report["stats"]["materials"], [])
        r, g, b, _ = pixels(out / "front_beauty.png")[32, 32]
        self.assertGreater(g, 2 * max(r, b))
        lineup = pixels(out / "lineup_front_beauty.png")
        tinted = lineup[:, 40:][lineup[:, 40:, 3] > 200]
        self.assertGreater(len(tinted), 20)
        self.assertGreater(float(np.median(tinted[:, 2])), 2 * float(np.median(tinted[:, 0])))

    def test_clip_source_poses_a_body_by_node_name(self):
        # The body carries its own "Rise" (to 3.0); the clip file's "Rise" (to 1.5) must win.
        body = cube_glb(self.tmp / "body.glb", clip=("Rise", [0.0, 1.0], [0.0, 3.0]))
        clip = cube_glb(self.tmp / "clip.glb", height=None, clip=("Rise", [0.0, 1.0], [0.0, 1.5]))
        out = self.tmp / "clip_pose"
        self.blender(HERE / "render_views.py", "--model", body, "--out", out, "--size", 64,
                     "--clip-source", clip, "--poses", "Rise@1.0", "Rise@0.5", "--views", "front", "--model-scale", 2,
                     "--passes", "silhouette", "--pose-passes", "silhouette")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        self.assertEqual(report["clip_source"], {"animations": ["Rise"], "replaced": ["Rise"], "unmatched_nodes": []})
        # Pose bounds stay in file units while the model renders at twice the size.
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
        result = run_blender.run(script, [str(model), str(cls.root)], log=log, timeout=600, server=False)
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

    def test_review_live_covers_an_assembly_root_with_its_children(self):
        assembly = self.data["assembly"]
        self.assertEqual(assembly["glb"]["triangles"], 12)
        self.assertIn("mesh_check", assembly["checks"])
        self.assertEqual(assembly["stats"]["triangles"], 12)

    def test_review_keeps_a_transform_set_off_the_keys(self):
        self.assertAlmostEqual(self.data["lid_z"], 5.0, places=5)


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
                     "--passes", "silhouette", "--size", 64, "--poses", f"Rise@{end}", "Rise@0",
                     "Rise@0.1234567", "Rise@0.1234568")
        report = json.loads((out / "views.json").read_text(encoding="utf-8"))
        self.assertEqual(report["pose"], "rest")
        lows = [p["bounds"]["min"][1] for p in report["poses"]]
        self.assertAlmostEqual(lows[0], 1.5, places=3)
        self.assertAlmostEqual(lows[1], -0.5, places=3)
        poses = [i for i in report["images"] if i["view"] == "pose"]
        self.assertEqual([(i["animation"], i["time"], i["base_view"], i["pass"]) for i in poses][:2],
                         [("Rise", end, "three_quarter", "beauty"), ("Rise", 0.0, "three_quarter", "beauty")])
        # Times that print alike under :g still get their own files.
        self.assertEqual(len({i["file"] for i in poses}), 4)
        for image in poses:
            self.assertTrue((out / image["file"]).is_file(), image)


if __name__ == "__main__":
    unittest.main()
