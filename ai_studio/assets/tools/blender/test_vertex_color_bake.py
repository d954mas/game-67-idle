import json
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path

from ai_studio.assets.tools.blender.vertex_color_bake import (
    Settings, audit_glb, force_rgba_float, linear_to_srgb, parse, pivot_point, read_glb,
    accessor_rows, sample_texture, scale_for_height, settings_from_args, srgb_hex, srgb_to_linear)

TOOL = Path(__file__).resolve().parent / "vertex_color_bake.py"
# Mid tones only: black and white survive a missing sRGB decode, so they would prove nothing.
PALETTE = [(203, 74, 32), (18, 140, 96), (128, 128, 128), (96, 32, 192)]


def find_blender():
    override = os.environ.get("BLENDER") or os.environ.get("BLENDER_EXECUTABLE")
    if override and Path(override).exists():
        return Path(override)
    candidates = sorted(Path("C:/Program Files/Blender Foundation").glob("Blender */blender.exe"),
                        reverse=True) if os.name == "nt" else []
    found = next((path for path in candidates if path.exists()), None)
    return found or (Path(shutil.which("blender")) if shutil.which("blender") else None)


BLENDER = find_blender()


def png_bytes(width, height, rows):
    """Write a plain 8-bit RGB PNG so the expected colours come from bytes this test chose."""
    raw = b"".join(b"\0" + bytes(channel for pixel in row for channel in pixel) for row in rows)

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xffffffff))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


FIXTURE_SOURCE = """
import bpy, sys
from pathlib import Path
atlas, destination = sys.argv[-2], sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_cube_add(size=2)
cube = bpy.context.object
cube.name = "atlas_cube"
mesh = cube.data
mesh.uv_layers.new(name="UVMap")
uvs = mesh.uv_layers.active.data
for polygon in mesh.polygons:
    column = polygon.index % 4
    for loop in polygon.loop_indices:
        uvs[loop].uv = ((column + .5) / 4, .5)
material = bpy.data.materials.new("palette")
material.use_nodes = True
shader = material.node_tree.nodes["Principled BSDF"]
texture = material.node_tree.nodes.new("ShaderNodeTexImage")
texture.image = bpy.data.images.load(atlas)
texture.interpolation = "Closest"
material.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
mesh.materials.append(material)
bpy.ops.export_scene.gltf(filepath=destination, export_format="GLB", export_yup=True)
"""

OPERATOR_DRIVER = """
import bpy, sys
tool, source, output, name = sys.argv[-4:]
# Factory settings first: reloading them switches the add-on back off.
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.preferences.addon_install(filepath=tool, overwrite=True)
bpy.ops.preferences.addon_enable(module="vertex_color_bake")
settings = bpy.context.scene.vertex_colour_bake
settings.source = source
settings.output = output
settings.name = name
settings.height = 1.5
settings.pivot = "floor"
if bpy.ops.assetpipe.vertex_colour_bake() != {"FINISHED"}:
    raise SystemExit("the operator refused the bake")
print("PANEL_BAKE_DONE")
"""


def synthetic_glb(colours, count=3):
    """Build the smallest GLB the audit accepts, with COLOR_0 written as VEC3 floats."""
    positions = b"".join(struct.pack("<3f", i, 0, 0) for i in range(count))
    normals = b"".join(struct.pack("<3f", 0, 1, 0) for _ in range(count))
    colour_bytes = b"".join(struct.pack("<3f", *colour) for colour in colours)
    indices = struct.pack("<3I", 0, 1, 2)
    binary = positions + normals + colour_bytes + indices
    views, offset = [], 0
    for payload in (positions, normals, colour_bytes, indices):
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload)})
        offset += len(payload)
    document = {
        "asset": {"version": "2.0"},
        "scenes": [{"nodes": [0]}], "scene": 0,
        "nodes": [{"name": "probe", "mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2},
                                    "indices": 3}]}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 3, "componentType": 5125, "count": 3, "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(binary)}],
    }
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * ((-len(encoded)) % 4)
    binary += b"\0" * ((-len(binary)) % 4)
    return (struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(binary))
            + struct.pack("<II", len(encoded), 0x4E4F534A) + encoded
            + struct.pack("<II", len(binary), 0x004E4942) + binary)


class ColourTests(unittest.TestCase):
    def test_srgb_decode_matches_the_standard(self):
        self.assertAlmostEqual(srgb_to_linear(0), 0)
        self.assertAlmostEqual(srgb_to_linear(1), 1)
        self.assertAlmostEqual(srgb_to_linear(128 / 255), .21586, places=5)
        self.assertAlmostEqual(srgb_to_linear(.04), .04 / 12.92)

    def test_decode_round_trips(self):
        for step in range(0, 256, 17):
            value = step / 255
            self.assertAlmostEqual(linear_to_srgb(srgb_to_linear(value)), value, places=6)

    def test_hex_reports_the_source_byte(self):
        self.assertEqual(srgb_hex((srgb_to_linear(203 / 255), srgb_to_linear(74 / 255),
                                   srgb_to_linear(32 / 255))), "#cb4a20")


class SampleTests(unittest.TestCase):
    def setUp(self):
        # Two columns of one row: a palette atlas in miniature, bottom-up like Blender's buffer.
        self.pixels = [.2, .4, .6, 1, .8, .5, .1, 1]

    def test_nearest_returns_the_exact_texel(self):
        self.assertEqual(sample_texture(self.pixels, 2, 1, .25, .5)[:3], (.2, .4, .6))
        self.assertEqual(sample_texture(self.pixels, 2, 1, .75, .5)[:3], (.8, .5, .1))

    def test_nearest_wraps_out_of_range_coordinates(self):
        self.assertEqual(sample_texture(self.pixels, 2, 1, 1.25, .5)[:3], (.2, .4, .6))
        self.assertEqual(sample_texture(self.pixels, 2, 1, -.75, .5)[:3], (.2, .4, .6))

    def test_decode_happens_before_any_weighting(self):
        nearest = sample_texture(self.pixels, 2, 1, .25, .5, "nearest", True)
        self.assertAlmostEqual(nearest[0], srgb_to_linear(.2))
        blended = sample_texture(self.pixels, 2, 1, .5, .5, "bilinear", True)
        self.assertAlmostEqual(blended[0], (srgb_to_linear(.2) + srgb_to_linear(.8)) / 2, places=6)


class ContractShapeTests(unittest.TestCase):
    def test_pivot_presets_ride_the_bounds(self):
        low, high = (-1, -2, 0), (1, 2, 4)
        self.assertEqual(pivot_point(low, high, "floor"), (0, 0, 0))
        self.assertEqual(pivot_point(low, high, "center"), (0, 0, 2))
        self.assertEqual(pivot_point(low, high, "top"), (0, 0, 4))
        self.assertEqual(pivot_point(low, high, "keep"), (0, 0, 0))

    def test_offset_is_read_in_game_axes(self):
        low, high = (0, 0, 0), (2, 2, 2)
        self.assertEqual(pivot_point(low, high, "floor", (0, .5, 0)), (1, 1, .5))
        self.assertEqual(pivot_point(low, high, "floor", (0, 0, .25)), (1, .75, 0))

    def test_height_scales_uniformly(self):
        self.assertEqual(scale_for_height((0, 0, 0), (1, 1, 2), 3), 1.5)
        self.assertEqual(scale_for_height((0, 0, 0), (1, 1, 2), 0), 1.0)
        with self.assertRaises(ValueError):
            scale_for_height((0, 0, 0), (1, 1, 0), 3)


class SettingsTests(unittest.TestCase):
    def test_name_defaults_to_the_output_stem(self):
        self.assertEqual(Settings(output="out/park_bench.glb").name, "park_bench")

    def test_unknown_choices_are_refused(self):
        for field in ({"pivot": "middle"}, {"sample": "vertex"}, {"texture_filter": "cubic"}):
            with self.assertRaises(ValueError):
                Settings(output="a.glb", **field)

    def test_command_line_maps_onto_the_panel_fields(self):
        settings = settings_from_args(parse(["--input", "a.glb", "--output", "b.glb",
                                             "--height", "1.5", "--pivot", "top",
                                             "--pivot-offset", "0,-.25,1", "--yaw", "90",
                                             "--sample", "corner", "--filter", "bilinear"]))
        self.assertEqual(settings.pivot_offset, (0, -.25, 1))
        self.assertEqual((settings.height, settings.yaw, settings.pivot), (1.5, 90, "top"))
        self.assertEqual((settings.sample, settings.texture_filter), ("corner", "bilinear"))

    def test_relative_paths_become_absolute(self):
        self.assertTrue(Path(Settings(output="out/a.glb").output).is_absolute())


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.directory, ignore_errors=True)

    def write(self, name, payload):
        path = self.directory / name
        path.write_bytes(payload)
        return path

    def test_vec3_colour_is_repacked_as_float_rgba(self):
        path = self.write("probe.glb", synthetic_glb([(1, 0, 0), (0, 1, 0), (0, 0, 1)]))
        self.assertEqual(audit_glb(path)["status"], "fail")
        self.assertTrue(force_rgba_float(path)["rewritten"])
        report = audit_glb(path, node_name="probe")
        self.assertEqual(report["status"], "pass", report["failures"])
        document, binary = read_glb(path)
        colours, info = accessor_rows(document, binary,
                                      document["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"])
        self.assertEqual(info["type"], "VEC4")
        self.assertEqual(colours[0], (1, 0, 0, 1))

    def test_repacking_a_conforming_file_changes_nothing(self):
        path = self.write("probe.glb", synthetic_glb([(1, 0, 0), (0, 1, 0), (0, 0, 1)]))
        force_rgba_float(path)
        before = path.read_bytes()
        self.assertFalse(force_rgba_float(path)["rewritten"])
        self.assertEqual(path.read_bytes(), before)

    def test_a_wrong_node_name_fails(self):
        path = self.write("probe.glb", synthetic_glb([(1, 0, 0)] * 3))
        force_rgba_float(path)
        failures = [item["check"] for item in audit_glb(path, node_name="park_bench")["failures"]]
        self.assertIn("node name", failures)

    def test_colours_outside_the_unit_range_fail(self):
        path = self.write("probe.glb", synthetic_glb([(2, 0, 0), (0, 1, 0), (0, 0, 1)]))
        force_rgba_float(path)
        failures = [item["check"] for item in audit_glb(path)["failures"]]
        self.assertIn("colours in 0..1", failures)


@unittest.skipIf(BLENDER is None, "Blender was not found; set BLENDER to the executable")
class BakeTests(unittest.TestCase):
    """End to end through Blender: a palette atlas has to survive as flat COLOR_0 fills."""

    @classmethod
    def setUpClass(cls):
        cls.directory = Path(tempfile.mkdtemp())
        atlas = cls.directory / "palette.png"
        atlas.write_bytes(png_bytes(4, 4, [list(PALETTE) for _ in range(4)]))
        script = cls.directory / "fixture.py"
        script.write_text(FIXTURE_SOURCE, encoding="utf-8")
        cls.source = cls.directory / "atlas_cube.glb"
        cls.blender(["--python", str(script), "--", str(atlas), str(cls.source)])
        cls.output = cls.directory / "baked.glb"
        cls.report = cls.directory / "baked.json"
        cls.blender(["--python", str(TOOL), "--", "--input", str(cls.source),
                     "--output", str(cls.output), "--name", "baked", "--height", "1.5",
                     "--pivot", "floor", "--report", str(cls.report)])

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.directory, ignore_errors=True)

    @classmethod
    def blender(cls, arguments, environment=None):
        result = subprocess.run([str(BLENDER), "--background", "--factory-startup"] + arguments,
                                capture_output=True, text=True, env=environment)
        # Blender still exits zero after a script traceback, so the transcript is the real verdict.
        if result.returncode or "Traceback (most recent call last)" in result.stdout + result.stderr:
            raise AssertionError(result.stdout[-4000:] + result.stderr[-4000:])
        return result

    def test_the_flat_fills_match_the_source_texture(self):
        document, binary = read_glb(self.output)
        colours, _ = accessor_rows(document, binary,
                                   document["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"])
        baked = {tuple(round(channel, 4) for channel in colour[:3]) for colour in colours}
        expected = {tuple(round(srgb_to_linear(channel / 255), 4) for channel in entry)
                    for entry in PALETTE}
        self.assertEqual(baked, expected)
        self.assertTrue(all(colour[3] == 1 for colour in colours))

    def test_the_export_passes_the_contract(self):
        report = json.loads(self.report.read_text(encoding="utf-8"))
        self.assertEqual(report["contract"]["status"], "pass", report["contract"]["failures"])
        self.assertEqual(report["contract"]["node"], "baked")
        self.assertEqual(report["materials"][0]["source"], "texture")
        self.assertTrue(report["materials"][0]["decoded_srgb"])

    def test_the_model_is_scaled_and_stood_on_the_floor(self):
        report = json.loads(self.report.read_text(encoding="utf-8"))
        low = report["output"]["bounds_game_axes"]["min"]
        high = report["output"]["bounds_game_axes"]["max"]
        self.assertAlmostEqual(high[1] - low[1], 1.5, places=5)
        self.assertAlmostEqual(low[1], 0, places=6)
        self.assertAlmostEqual((low[0] + high[0]) / 2, 0, places=6)
        self.assertAlmostEqual((low[2] + high[2]) / 2, 0, places=6)

    def test_the_panel_and_the_command_line_write_the_same_file(self):
        scripts = self.directory / "userscripts"
        scripts.mkdir(exist_ok=True)
        driver = self.directory / "driver.py"
        driver.write_text(OPERATOR_DRIVER, encoding="utf-8")
        through_panel = self.directory / "panel.glb"
        # An isolated scripts root keeps the test's add-on install out of the user's Blender.
        environment = dict(os.environ, BLENDER_USER_SCRIPTS=str(scripts))
        result = self.blender(["--python", str(driver), "--", str(TOOL), str(self.source),
                               str(through_panel), "baked"], environment=environment)
        self.assertIn("PANEL_BAKE_DONE", result.stdout)
        self.assertEqual(through_panel.read_bytes(), self.output.read_bytes())


if __name__ == "__main__":
    unittest.main()
