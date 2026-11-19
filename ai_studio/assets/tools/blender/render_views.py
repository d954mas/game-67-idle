"""Render a GLB for review: fitted views x passes as PNGs, plus views.json with model stats.

Run inside Blender through the runner:

    node ai_studio/dev_environment/python_run.mjs ai_studio/assets/tools/blender/run_blender.py \
        ai_studio/assets/tools/blender/render_views.py -- --model m.glb --out tmp/review

Flags: --views front right three_quarter game turntable (or comma-separated), --passes beauty
clay wire silhouette facing, --size 768, --light studio_hdr|flat, --background transparent|#hex,
--animation NAME --time SECONDS, --samples 16, --game '{"height": 1.8, ...}',
--turntable-frames 8, --no-sheet, --eevee-passes (clay/silhouette lit by the HDR in EEVEE
instead of the about 3x faster Workbench). Several --model files render in one Blender start into
<out>/<model stem>/ each, which saves the start-up and first-frame cost per model. `--job job.json`
takes the same keys as JSON; a view there may be an object such as {"view": "game",
"height": 1.8, "behind": 2.0, "offset": 0, "pitch": 16, "fov_y": 55, "yaw": 0,
"target": [0, 0, 0], "aspect": 1.78} or {"view": "turntable", "frames": 8}.

Scale lineup: --beside ref1.glb ref2.glb puts the references next to the model along +X at true
scale (bottoms level, gap 15% of the widest member) and renders the group with --lineup-views
(default front three_quarter, plus game when a game view is asked for) and --lineup-passes
(default beauty). Poses: --poses Walk@0.4 Run@1.25 (glTF seconds) renders each with
--pose-views (default three_quarter) and --pose-passes (default beauty) at one framing shared
by all poses; --clip-source clips.glb plays a clip-only GLB on the model's nodes by name, as
the engine does. --pose-size PX sizes pose images (default --size). --floor puts a ground at
glTF y=0 (faint 0.5 m grid, contact shadow) under the pose views' beauty pass; --floor all
puts it under every view's beauty pass. The floor never enters stats or bounds.

Pass `facing`: front faces grey, back faces red and nothing culled, so an inside-out shell or a
flipped face shows red from outside.

Views use glTF axes: +Z is the model's front, so `front` looks at it along -Z and `right`
sees its +X side (Blender's numpad views). front/back/left/right/top are orthographic and fitted
to the vertices; three_quarter and turntable are perspective and fitted; `game_angle` looks
along the game camera (pitch, yaw, FOV or ortho) but is fitted to the model; `game` reproduces an
engine follow camera (yaw 0 = behind the model, looking along +Z) and is not fitted, except in
the lineup, where it keeps the camera's pitch, yaw and FOV and fits the whole group. A game
camera with "ortho": true and "ortho_half_height" (metres) projects orthographically.

--model-scale S scales the model about the origin in every view, as a game that scales the mesh
at runtime shows it; stats and pose bounds stay in file units and views.json records the scale.
--beside entries take a scale too: crown.glb@0.5.

Runtime looks the GLB does not carry: --atlas palette.png is bound as base colour (UV0, nearest,
sRGB) to every material without an image texture, --tint "#rrggbb" multiplies the base colour;
both apply to the beauty pass. For references, --beside takes a JSON list instead of paths:
'[{"path": "a.glb", "scale": 0.5, "atlas": "pal.png", "tint": "#c08040"}]'.

In-process use (a build script, a long-lived Blender): `main(argv)` returns the report and
raises instead of exiting; `render_objects(objects, job)` renders objects already in the
scene and puts the scene back as it found it (see lib/review_live.py).
"""
import argparse
import copy
import hashlib
import json
import math
import re
import sys
import tempfile
import time
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

from ai_studio.assets.tools.blender.lib import bpy_io
from ai_studio.assets.tools.blender.lib import bpy_scene as S
from ai_studio.assets.tools.blender.lib.color import as_linear, linear_to_srgb, write_png
from ai_studio.assets.tools.blender.lib.glb import Glb

VIEWS = ("front", "back", "left", "right", "top", "three_quarter", "game", "game_angle", "turntable")
GAME_VIEWS = ("game", "game_angle")
PASSES = ("beauty", "clay", "wire", "silhouette", "facing")
DEFAULTS = {"views": ["front", "right", "three_quarter"], "passes": ["beauty", "clay", "wire"],
            "size": 768, "light": "studio_hdr", "background": "transparent", "samples": 16,
            "beside": [], "lineup_views": None, "lineup_passes": ["beauty"],
            "poses": [], "pose_views": ["three_quarter"], "pose_passes": ["beauty"], "pose_size": None,
            "floor": None, "model_scale": 1.0, "atlas": None, "tint": None}
FLOOR_MODES = ("poses", "all")
# Direction from the model toward the camera, Blender axes (glTF +Z front = Blender -Y).
ORTHO = {"front": (0, -1, 0), "back": (0, 1, 0), "right": (1, 0, 0), "left": (-1, 0, 0), "top": (0, 0, 1)}
GAME = {"height": 1.8, "behind": 2.0, "offset": 0.0, "pitch": 16.0, "fov_y": 55.0, "yaw": 0.0,
        "target": [0.0, 0.0, 0.0], "aspect": 16 / 9}
MARGIN = 1.08
LINEUP_GAP = 0.15
TEXTURE_PREVIEW = 512
CLAY = (0.55, 0.55, 0.55)
BACKFACE = (0.9, 0.02, 0.02)
FLOOR_GRID = 0.5
# Workbench draws a 768 px image in about 10 ms against EEVEE's 80 ms; for flat-grey form and
# outline that is the same picture. Camera-space studio light keeps every view lit alike.
WORKBENCH = {
    "clay": {"type": "SOLID", "light": "STUDIO", "studio_light": "studio.sl", "use_world_space_lighting": False,
             "color_type": "SINGLE", "single_color": CLAY, "show_cavity": True, "cavity_type": "WORLD",
             "show_specular_highlight": False, "show_shadows": False, "show_object_outline": False,
             "show_xray": False, "show_backface_culling": False},
    "silhouette": {"type": "SOLID", "light": "FLAT", "color_type": "SINGLE", "single_color": (0.0, 0.0, 0.0),
                   "show_cavity": False, "show_specular_highlight": False, "show_shadows": False,
                   "show_object_outline": False, "show_xray": False, "show_backface_culling": False},
}


class Skip(Exception):
    """A model this run cannot render; the batch records why and goes on with the next one."""


class _Parser(argparse.ArgumentParser):
    """Raises instead of exiting, so a long-lived Blender survives a bad command line."""

    def error(self, message):
        raise ValueError(f"render_views: {message}")

    def exit(self, status=0, message=None):
        raise ValueError(f"render_views: {message or 'exit'}")


def parse(argv):
    p = _Parser(description=__doc__.splitlines()[0])
    p.add_argument("--job")
    p.add_argument("--model", nargs="+", help="one or more GLBs; several share one Blender start")
    p.add_argument("--out")
    p.add_argument("--views", nargs="+")
    p.add_argument("--passes", nargs="+")
    p.add_argument("--size", type=int)
    p.add_argument("--light", choices=("studio_hdr", "flat"))
    p.add_argument("--background")
    p.add_argument("--animation")
    p.add_argument("--time", type=float)
    p.add_argument("--samples", type=int)
    p.add_argument("--game", help="JSON object of game camera fields")
    p.add_argument("--turntable-frames", type=int)
    p.add_argument("--no-sheet", action="store_true", help="skip the sheet.png contact sheet")
    p.add_argument("--eevee-passes", action="store_true", help="clay and silhouette in EEVEE under the HDR")
    p.add_argument("--beside", nargs="+", help="reference GLBs for a true-scale lineup; path@scale scales one")
    p.add_argument("--model-scale", type=float, help="uniform display scale of the model, as the game applies it")
    p.add_argument("--atlas", help="palette PNG bound as base colour to materials without an image")
    p.add_argument("--tint", help="#rrggbb multiplied into the model's base colour")
    p.add_argument("--lineup-views", nargs="+")
    p.add_argument("--lineup-passes", nargs="+")
    p.add_argument("--poses", nargs="+", help="Clip@seconds ...")
    p.add_argument("--pose-views", nargs="+")
    p.add_argument("--pose-passes", nargs="+")
    p.add_argument("--clip-source", help="clip-only GLB to play on the model by node name")
    p.add_argument("--pose-size", type=int, help="pose image height in px (default: --size)")
    p.add_argument("--floor", nargs="?", const="poses", choices=FLOOR_MODES,
                   help="ground plane under the pose views (default) or all views, beauty pass")
    args = p.parse_args(argv)
    job = {}
    if args.job:
        job.update(json.loads(Path(args.job).read_text(encoding="utf-8")))
    if args.no_sheet:
        job["sheet"] = False
    if args.eevee_passes:
        job["eevee_passes"] = True
    for key in ("model", "out", "size", "light", "background", "animation", "time", "samples", "beside",
                "clip_source", "poses", "pose_size", "floor", "model_scale", "atlas", "tint"):
        if getattr(args, key) is not None:
            job[key] = getattr(args, key)
    for key in ("views", "passes", "lineup_views", "lineup_passes", "pose_views", "pose_passes"):
        if getattr(args, key):
            job[key] = split_list(getattr(args, key))
    if args.game:
        job["game"] = json.loads(args.game)
    if args.turntable_frames:
        job["turntable_frames"] = args.turntable_frames
    job = normalize(job)
    if not job.get("model") or not job.get("out"):
        p.error("--model and --out (or a job with both) are required")
    return job


def split_list(items):
    return [v for item in items for v in str(item).split(",") if v]


def _view_specs(views, game, turntable_frames):
    specs = []
    for view in views:
        view = dict(view) if isinstance(view, dict) else {"view": view}
        view["view"] = view.get("view") or view.get("name")
        if view["view"] in GAME_VIEWS and game:
            view = {**game, **view}
        if view["view"] == "turntable" and turntable_frames:
            view.setdefault("frames", turntable_frames)
        specs.append(view)
    return specs


def _pose_spec(item):
    if isinstance(item, dict):
        return {"animation": item["animation"], "time": float(item.get("time", 0.0))}
    name, _, seconds = str(item).rpartition("@")
    if not name:
        name, seconds = seconds, "0"
    return {"animation": name, "time": float(seconds)}


def normalize(job):
    """Defaults filled, view names turned into specs, poses parsed, unknown names refused."""
    job = {**copy.deepcopy(DEFAULTS), **job}
    game = job.get("game")
    if isinstance(game, str):
        game = json.loads(game)
    frames = job.get("turntable_frames")
    for key in ("views", "passes", "lineup_passes", "pose_views", "pose_passes", "beside", "poses"):
        if isinstance(job[key], str):
            job[key] = [job[key]]
    job["views"] = _view_specs(job["views"], game, frames)
    lineup = job["lineup_views"]
    if lineup is None:
        lineup = ["front", "three_quarter"]
        lineup += [{**v, "view": "game"} for v in job["views"] if v["view"] in GAME_VIEWS][:1]
    job["lineup_views"] = _view_specs(lineup, game, frames)
    job["pose_views"] = _view_specs(job["pose_views"], game, frames)
    job["poses"] = [_pose_spec(p) for p in job["poses"]]
    views = job["views"] + job["lineup_views"] + job["pose_views"]
    passes = job["passes"] + job["lineup_passes"] + job["pose_passes"]
    beside = job["beside"]
    if len(beside) == 1 and isinstance(beside[0], str) and beside[0].lstrip().startswith("["):
        job["beside"] = json.loads(beside[0])
    job["model_scale"] = float(job["model_scale"] or 1.0)
    if job["model_scale"] <= 0:
        raise ValueError(f"render_views: model_scale must be positive, not {job['model_scale']}")
    if job["floor"] not in (None, False, *FLOOR_MODES):
        raise ValueError(f"render_views: floor must be one of {FLOOR_MODES}, not {job['floor']!r}")
    bad = [v["view"] for v in views if v["view"] not in VIEWS] + [x for x in passes if x not in PASSES]
    if bad:
        raise ValueError(f"render_views: unknown view/pass {bad}; views: {VIEWS}, passes: {PASSES}")
    return job


# --- scene ------------------------------------------------------------------------------------

def setup_render(scene, samples):
    scene.render.engine = S.eevee_engine()
    scene.eevee.taa_render_samples = samples
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.film_transparent = True
    scene.display.render_aa = "8"
    # AgX/Filmic shift palette colours; Standard keeps an authored '#hex' looking like itself.
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0


class SceneState:
    """What a review changes in a scene, put back on exit, and every datablock it created removed,
    so a build script's scene (and file) comes out of a review as it went in."""

    RENDER = ("engine", "resolution_x", "resolution_y", "resolution_percentage", "filepath", "film_transparent")
    IMAGE = ("file_format", "color_mode", "color_depth", "compression")
    VIEW = ("view_transform", "look", "exposure", "gamma")
    SHADING = sorted({k for opts in WORKBENCH.values() for k in opts})
    DATA = ("objects", "cameras", "lights", "meshes", "armatures", "curves", "materials", "node_groups",
            "worlds", "images", "actions", "textures", "collections")

    def __init__(self, scene):
        self.scene = scene

    def __enter__(self):
        sc, layer = self.scene, bpy.context.view_layer
        self.layer = layer
        grab = lambda owner, keys: {k: _plain(getattr(owner, k)) for k in keys}
        shading = sc.display.shading
        self.values = [(sc.render, grab(sc.render, self.RENDER)),
                       (sc.render.image_settings, grab(sc.render.image_settings, self.IMAGE)),
                       (sc.view_settings, grab(sc.view_settings, self.VIEW)),
                       (sc.eevee, grab(sc.eevee, ("taa_render_samples", "use_shadows"))),
                       (sc.display, {"render_aa": sc.display.render_aa}),
                       (shading, grab(shading, self.SHADING))]
        self.camera, self.world, self.override = sc.camera, sc.world, layer.material_override
        self.frame = (sc.frame_current, sc.frame_subframe)
        self.hidden = {o: o.hide_render for o in sc.objects}
        self.parents = {o: (o.parent, o.matrix_parent_inverse.copy()) for o in sc.objects}
        self.undo = []
        self.selected = {o: o.select_get() for o in layer.objects}
        self.active = layer.objects.active
        self.basis = {o: o.matrix_basis.copy() for o in sc.objects}
        self.bones = {(o, b.name): b.matrix_basis.copy() for o in sc.objects if o.type == "ARMATURE"
                      for b in o.pose.bones}
        self.animation = {o: (o.animation_data.action, getattr(o.animation_data, "action_slot", None),
                              [(t, t.mute) for t in o.animation_data.nla_tracks])
                          for o in sc.objects if o.animation_data}
        self.data = {name: set(getattr(bpy.data, name)) for name in self.DATA}
        return self

    def restore_pose(self):
        """Back to the transforms and clips the scene had on entry (between poses, and at exit).
        The frame is set before the transforms: evaluating the clips at it would otherwise
        replace a transform the scene held off its keys (moved by a script after keying)."""
        for obj, (action, slot, tracks) in self.animation.items():
            data = obj.animation_data
            if data is None:
                continue
            data.action = action
            if slot is not None and hasattr(data, "action_slot"):
                data.action_slot = slot
            for track, mute in tracks:
                track.mute = mute
        for obj in list(self.scene.objects):
            if obj.animation_data and obj not in self.animation:
                obj.animation_data.action = None
        self.scene.frame_set(self.frame[0], subframe=self.frame[1])
        for obj, basis in self.basis.items():
            obj.matrix_basis = basis
        for (obj, bone), basis in self.bones.items():
            obj.pose.bones[bone].matrix_basis = basis
        bpy.context.view_layer.update()

    def __exit__(self, *exc):
        sc = self.scene
        for undo in reversed(self.undo):
            undo()
        for owner, values in self.values:
            for key, value in values.items():
                setattr(owner, key, value)
        sc.camera, sc.world = self.camera, self.world
        self.layer.material_override = self.override
        for obj, hidden in self.hidden.items():
            obj.hide_render = hidden
        for obj, (parent, inverse) in self.parents.items():
            if obj.parent is not parent:
                obj.parent = parent
                obj.matrix_parent_inverse = inverse
        self.restore_pose()
        for name in self.DATA:
            pool = getattr(bpy.data, name)
            for block in [b for b in pool if b not in self.data[name]]:
                pool.remove(block)
        for obj, selected in self.selected.items():
            obj.select_set(selected)
        self.layer.objects.active = self.active
        return False


def _plain(value):
    """Detach a vector/colour property value from the struct it was read from."""
    return tuple(value) if hasattr(value, "__len__") and not isinstance(value, str) else value


def file_rest(nodes, path):
    """Animated objects back to their node's rest TRS from the GLB (`nodes`: node index ->
    object, from bpy_io.import_glb_nodes). The importer makes a clip active and its first
    evaluation overwrites the object transform, so Blender no longer knows the rest the engine
    shows when no clip plays."""
    doc = Glb.load(path)
    for i, obj in nodes.items():
        if not obj.animation_data:
            continue
        t, r, s = doc.local_trs(i)
        obj.rotation_mode = "QUATERNION"
        obj.location = S.gltf_to_blender(t)
        obj.rotation_quaternion = (r[3], r[0], -r[2], r[1])
        obj.scale = (s[0], s[2], s[1])


def rest_pose(objects):
    """Bind pose: no active clip, every pose bone at rest."""
    for obj in objects:
        if obj.animation_data:
            obj.animation_data.action = None
            for track in obj.animation_data.nla_tracks:
                track.mute = True
        if obj.type == "ARMATURE":
            for bone in obj.pose.bones:
                bone.matrix_basis.identity()
    bpy.context.view_layer.update()


def clips(objects):
    """{clip name: [(object, action, slot)]} from the objects' NLA strips (how glTF imports clips)."""
    found = {}
    for obj in objects:
        data = obj.animation_data
        if not data:
            continue
        for track in data.nla_tracks:
            for strip in track.strips:
                if strip.action:
                    entry = (obj, strip.action, getattr(strip, "action_slot", None))
                    for name in dict.fromkeys((track.name, strip.action.name)):
                        found.setdefault(name, []).append(entry)
    return found


def pose(objects, animation, seconds, fps):
    """The named clip at glTF time `seconds` on every object that carries it; others keep rest."""
    hits = clips(objects).get(animation)
    if not hits:
        raise Skip(f"animation {animation!r} not found; the model has {sorted(clips(objects))}")
    for obj in objects:
        if obj.animation_data:
            obj.animation_data.action = None
            for track in obj.animation_data.nla_tracks:
                track.mute = True
    for obj, action, slot in hits:
        obj.animation_data.action = action
        if slot is not None and hasattr(obj.animation_data, "action_slot"):
            obj.animation_data.action_slot = slot
    # The glTF importer keys time t at frame t * fps, with no +1 offset.
    frame = (seconds or 0.0) * fps
    bpy.context.scene.frame_set(int(math.floor(frame)), subframe=frame - math.floor(frame))
    return f"{animation}@{seconds or 0.0:g}s"


def override_materials():
    clay = bpy.data.materials.new("review_clay")
    bsdf = S.find_node(S.node_tree(clay), "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*CLAY, 1.0)
    bsdf.inputs["Roughness"].default_value = 1.0
    wire = bpy.data.materials.new("review_wire")
    tree = S.node_tree(wire)
    bsdf = S.find_node(tree, "BSDF_PRINCIPLED")
    bsdf.inputs["Roughness"].default_value = 1.0
    # The Wireframe node draws the triangles the GPU gets, which is what a GLB ships. Workbench
    # cannot do this: its wireframe shading renders nothing in a final render.
    edge = tree.nodes.new("ShaderNodeWireframe")
    edge.use_pixel_size = True
    ramp = tree.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.interpolation = "CONSTANT"
    ramp.color_ramp.elements[0].color = (*CLAY, 1.0)
    ramp.color_ramp.elements[1].position = 0.5
    ramp.color_ramp.elements[1].color = (0.01, 0.01, 0.012, 1.0)
    tree.links.new(edge.outputs["Fac"], ramp.inputs["Fac"])
    tree.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    black = S.emission_material("review_silhouette", (0.0, 0.0, 0.0))
    return {"beauty": None, "clay": clay, "wire": wire, "silhouette": black, "facing": facing_material()}, edge


def facing_material():
    """Lit grey where the camera sees a face's front, flat red where it sees the back. Nothing is
    culled, so a shell wound inside-out reads red from outside, which a double-sided beauty or
    clay render hides."""
    mat = bpy.data.materials.new("review_facing")
    tree = S.node_tree(mat)
    bsdf = S.find_node(tree, "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*CLAY, 1.0)
    bsdf.inputs["Roughness"].default_value = 1.0
    geometry = tree.nodes.new("ShaderNodeNewGeometry")
    red = tree.nodes.new("ShaderNodeEmission")
    red.inputs["Color"].default_value = (*BACKFACE, 1.0)
    mix = tree.nodes.new("ShaderNodeMixShader")
    tree.links.new(geometry.outputs["Backfacing"], mix.inputs[0])
    tree.links.new(bsdf.outputs[0], mix.inputs[1])
    tree.links.new(red.outputs[0], mix.inputs[2])
    tree.links.new(mix.outputs[0], S.find_node(tree, "OUTPUT_MATERIAL").inputs["Surface"])
    mat.use_backface_culling = False
    return mat


class Floor:
    """Ground at glTF y=0 under the beauty pass: a 0.5 m grid and an overhead sun that, through
    light linking, lights only the floor, so the model keeps its studio look while its contact
    shadow shows whether it stands, floats or sinks. Created inside SceneState, which removes it."""

    def __init__(self, scene, points):
        lo, hi = points.min(axis=0), points.max(axis=0)
        centre = (lo + hi) / 2
        half = float(max(hi[0] - lo[0], hi[1] - lo[1])) * 1.5 + FLOOR_GRID
        # A thin slab, not a plane: front/right/left views look along the ground, where a plane
        # would vanish edge-on; the slab leaves a ground line to read sinking or floating against.
        thickness = max(0.01 * float(max(hi - lo)), 0.005)
        coll = bpy.data.collections.new("review_floor")
        scene.collection.children.link(coll)
        self.object = S.box("review_floor", (2 * half, 2 * half, thickness), location=(centre[0], centre[1], -thickness),
                            anchor="bottom", material=self._material(), coll=coll)
        self.sun = bpy.data.objects.new("review_floor_sun", bpy.data.lights.new("review_floor_sun", "SUN"))
        self.sun.data.energy = 2.0
        self.sun.data.angle = math.radians(4.0)
        self.sun.light_linking.receiver_collection = coll
        scene.collection.objects.link(self.sun)
        self.scene = scene
        self.show(False)

    @staticmethod
    def _material():
        mat = bpy.data.materials.new("review_floor")
        tree = S.node_tree(mat)
        bsdf = S.find_node(tree, "BSDF_PRINCIPLED")
        bsdf.inputs["Roughness"].default_value = 1.0
        coords = tree.nodes.new("ShaderNodeNewGeometry")
        xyz = tree.nodes.new("ShaderNodeSeparateXYZ")
        tree.links.new(coords.outputs["Position"], xyz.inputs[0])

        def math_node(op, a, b=None):
            node = tree.nodes.new("ShaderNodeMath")
            node.operation = op
            for socket, value in zip(node.inputs, (a, b)):
                if value is None:
                    continue
                if isinstance(value, (int, float)):
                    socket.default_value = value
                else:
                    tree.links.new(value, socket)
            return node.outputs[0]
        # Distance in metres to the nearest grid line along x and y, then a thin line mask.
        lines = []
        for axis in ("X", "Y"):
            cell = math_node("FRACT", math_node("ADD", math_node("DIVIDE", xyz.outputs[axis], FLOOR_GRID), 0.5))
            lines.append(math_node("ABSOLUTE", math_node("SUBTRACT", cell, 0.5)))
        near = math_node("MINIMUM", *lines)
        mask = math_node("LESS_THAN", math_node("MULTIPLY", near, FLOOR_GRID), 0.006)
        # Dark enough that the studio light plus the floor sun does not wash it out to white.
        tree.links.new(math_node("SUBTRACT", 0.3, math_node("MULTIPLY", mask, 0.08)), bsdf.inputs["Base Color"])
        return mat

    def show(self, visible):
        self.object.hide_render = self.sun.hide_render = not visible
        # Only the floor's contact shadow needs the shadow system.
        self.scene.eevee.use_shadows = visible or any(
            o.type == "LIGHT" and not o.hide_render for o in self.scene.objects)


class Passes:
    """Switches the scene between passes: EEVEE with a material override, or Workbench."""

    def __init__(self, scene, samples, eevee_only=False):
        self.scene, self.samples = scene, samples
        self.overrides, self.edge = override_materials()
        self.workbench = {} if eevee_only else WORKBENCH

    def engine(self, name):
        return "workbench" if name in self.workbench else "eevee"

    def apply(self, name, size):
        scene, layer = self.scene, bpy.context.view_layer
        if name in self.workbench:
            scene.render.engine = "BLENDER_WORKBENCH"
            layer.material_override = None
            for key, value in self.workbench[name].items():
                setattr(scene.display.shading, key, value)
            return
        scene.render.engine = S.eevee_engine()
        scene.eevee.taa_render_samples = self.samples
        layer.material_override = self.overrides[name]
        # A one-pixel wire vanishes in a large image and floods a thumbnail; scale with it.
        self.edge.inputs["Size"].default_value = max(1.0, size / 768)


# --- cameras ----------------------------------------------------------------------------------

def make_camera(scene):
    cam = bpy.data.objects.new("review_camera", bpy.data.cameras.new("review_camera"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam


def basis(direction, up=(0, 0, 1)):
    d = Vector(direction).normalized()
    up = Vector((0, 1, 0)) if abs(d.dot(Vector(up))) > 0.999 else Vector(up)
    right = (-d).cross(up).normalized()
    return d, right, right.cross(-d).normalized()


def place(cam, position, d, u):
    """Camera at `position` looking along -d with `u` up (to_track_quat would pick its own roll)."""
    x = u.cross(d).normalized()
    cam.matrix_world = Matrix((x, d.cross(x), d)).transposed().to_4x4()
    cam.location = position


def fit_ortho(cam, points, direction, aspect):
    d, r, u = basis(direction)
    px, py, pz = points @ np.array(r), points @ np.array(u), points @ np.array(d)
    cx, cy = (px.max() + px.min()) / 2, (py.max() + py.min()) / 2
    w, h = px.max() - px.min(), py.max() - py.min()
    depth = pz.max() - pz.min()
    cam.data.type = "ORTHO"
    cam.data.sensor_fit = "AUTO"
    cam.data.ortho_scale = (max(w, h * aspect) if aspect >= 1 else max(w / aspect, h)) * MARGIN or 1.0
    span = max(w, h, depth, 1e-3)
    place(cam, r * cx + u * cy + d * (pz.max() + span), d, u)
    cam.data.clip_start = span * 0.01
    cam.data.clip_end = depth + span * 3


def perspective_distance(points, centre, d, r, u, fov_y, aspect, shift=(0.0, 0.0)):
    rel = points - np.array(centre)
    px, py, pz = rel @ np.array(r) - shift[0], rel @ np.array(u) - shift[1], rel @ np.array(d)
    ty = math.tan(fov_y / 2) / MARGIN
    tx = ty * aspect
    return float(np.max(np.maximum(pz + np.abs(px) / tx, pz + np.abs(py) / ty))), px, py, pz


def fit_perspective(cam, points, direction, aspect, fov_y_deg=30.0, distance=None):
    """Place the camera along `direction` so every vertex is in frame and the image is centred."""
    d, r, u = basis(direction)
    centre = (points.min(axis=0) + points.max(axis=0)) / 2
    fov_y = math.radians(fov_y_deg)
    shift = [0.0, 0.0]
    for _ in range(4):
        dist, px, py, pz = perspective_distance(points, centre, d, r, u, fov_y, aspect, shift)
        if distance is not None:
            dist = distance
            break
        depth = dist - pz
        sx, sy = px / depth, py / depth
        shift[0] += (sx.max() + sx.min()) / 2 * float(np.median(depth))
        shift[1] += (sy.max() + sy.min()) / 2 * float(np.median(depth))
    set_perspective(cam, fov_y)
    place(cam, Vector(centre) + r * shift[0] + u * shift[1] + d * dist, d, u)
    span = float(np.ptp(points, axis=0).max()) or 1.0
    cam.data.clip_start = max(dist - span * 2, span * 0.01, 1e-3) * 0.5
    cam.data.clip_end = dist + span * 2
    return dist


def set_perspective(cam, fov_y):
    cam.data.type = "PERSP"
    cam.data.sensor_fit = "VERTICAL"
    cam.data.angle_y = fov_y


def orbit(azimuth_deg, elevation_deg):
    """Direction toward the camera; azimuth 0 = front, positive swings to the model's +X."""
    az, el = math.radians(azimuth_deg), math.radians(elevation_deg)
    return (math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el))


def _game_rig(spec):
    """(merged spec, eye, forward) of the engine follow camera, Blender axes."""
    g = {**GAME, **{k: v for k, v in spec.items() if k != "view"}}
    target = Vector(S.gltf_to_blender(g["target"]))
    yaw = math.radians(g["yaw"])
    rot = lambda v: Vector((v[0] * math.cos(yaw) - v[1] * math.sin(yaw), v[0] * math.sin(yaw) + v[1] * math.cos(yaw), v[2]))
    eye = target + rot(S.gltf_to_blender((g["offset"], g["height"], -g["behind"])))
    pitch = math.radians(g["pitch"])
    return g, eye, rot((0.0, -math.cos(pitch), -math.sin(pitch)))


def game_camera(cam, spec):
    """Engine follow camera: `behind` the target along -Z (glTF), `height` up, `offset` on X,
    pitched down by `pitch`, vertical FOV `fov_y`; `yaw` turns the rig about the target. With
    `ortho`, the same rig projects orthographically, `ortho_half_height` metres above and below."""
    g, eye, forward = _game_rig(spec)
    if g.get("ortho"):
        cam.data.type = "ORTHO"
        cam.data.sensor_fit = "VERTICAL"
        cam.data.ortho_scale = 2.0 * float(g["ortho_half_height"])
    else:
        set_perspective(cam, math.radians(g["fov_y"]))
    d, _, u = basis(-forward)
    place(cam, eye, d, u)
    cam.data.clip_start, cam.data.clip_end = 0.05, 500.0
    return g


def fit_game(cam, points, spec):
    """The game camera's direction and FOV (or ortho projection), distance, aim and ortho scale
    fitted to `points`: `game_angle`, and the lineup's game view, where the hero target would
    cut the references off."""
    g, _, forward = _game_rig(spec)
    if g.get("ortho"):
        fit_ortho(cam, points, -forward, g["aspect"])
    else:
        fit_perspective(cam, points, -forward, g["aspect"], g["fov_y"])
    return g


# --- output -----------------------------------------------------------------------------------

def read_pixels(image):
    w, h = image.size
    px = np.empty(w * h * 4, np.float32)
    image.pixels.foreach_get(px)
    return px.reshape(h, w, 4)[::-1]


def composite_background(path, colour):
    """Flatten a transparent render over a colour; PNG values are sRGB, so blend there."""
    image = bpy.data.images.load(str(path))
    rgba = read_pixels(image)
    bpy.data.images.remove(image)
    bg = linear_to_srgb(np.array(as_linear(colour)[:3]))
    a = rgba[..., 3:4]
    out = np.concatenate([rgba[..., :3] * a + bg * (1 - a), np.ones_like(a)], axis=2)
    write_png(path, np.round(np.clip(out, 0, 1) * 255).astype(np.uint8))


def safe(name):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", name).strip("_") or "image"


def contact_sheet(out, images, cell=320, backdrop=(0.82, 0.82, 0.84)):
    """All images on one grid (row per view position, column per pass), so a reviewer opens a
    single file; transparency sits on a light grey that keeps black silhouettes visible."""
    rows = list(dict.fromkeys(Path(i["file"]).stem.rsplit("_", 1)[0] for i in images))
    cols = list(dict.fromkeys(i["pass"] for i in images))
    cell_h = cell
    cell_w = max(round(cell * i["size"][0] / i["size"][1]) for i in images)
    sheet = np.ones((cell_h * len(rows), cell_w * len(cols), 3), np.float32) * np.array(backdrop, np.float32)
    for record in images:
        image = bpy.data.images.load(str(out / record["file"]))
        rgba = read_pixels(image)
        bpy.data.images.remove(image)
        h, w = rgba.shape[:2]
        scale = min(cell_w / w, cell_h / h)
        ys = (np.arange(int(h * scale)) / scale).astype(int)
        xs = (np.arange(int(w * scale)) / scale).astype(int)
        small = rgba[ys][:, xs]
        r = rows.index(Path(record["file"]).stem.rsplit("_", 1)[0])
        c = cols.index(record["pass"])
        y0, x0 = r * cell_h + (cell_h - len(ys)) // 2, c * cell_w + (cell_w - len(xs)) // 2
        region = sheet[y0:y0 + len(ys), x0:x0 + len(xs)]
        a = small[..., 3:4]
        region[:] = small[..., :3] * a + region * (1 - a)
    rgba = np.concatenate([sheet, np.ones(sheet.shape[:2] + (1,), np.float32)], axis=2)
    write_png(out / "sheet.png", np.round(np.clip(rgba, 0, 1) * 255).astype(np.uint8))
    return {"file": "sheet.png", "rows": rows, "columns": cols}


def material_images(meshes):
    """Images the meshes' materials sample (node groups included), in first-use order."""
    found = {}

    def walk(tree):
        for node in tree.nodes:
            if node.type == "TEX_IMAGE" and node.image:
                found.setdefault(node.image, None)
            elif node.type == "GROUP" and node.node_tree:
                walk(node.node_tree)
    for obj in meshes:
        for slot in obj.material_slots:
            if slot.material and slot.material.node_tree:
                walk(slot.material.node_tree)
    return list(found)


def save_textures(out, images):
    textures = []
    for image in images:
        if not image.size[0]:
            continue
        file = Path("textures") / f"{safe(image.name)}.png"
        pixels = read_pixels(image)
        # The card shows a thumbnail; a full 4K dump through Python zlib costs seconds and tens of MB.
        step = max(1, -(-max(image.size) // TEXTURE_PREVIEW))
        write_png(out / file, np.round(np.clip(pixels[::step, ::step], 0, 1) * 255).astype(np.uint8), level=1)
        textures.append({"name": image.name, "size": list(image.size), "file": file.as_posix()})
    return textures


def model_stats(objects, meshes, textures, fps):
    points = S.evaluated_points(meshes)
    lo, hi = points.min(axis=0), points.max(axis=0)
    depsgraph = bpy.context.evaluated_depsgraph_get()
    vertices = 0
    for obj in meshes:
        evaluated = obj.evaluated_get(depsgraph)
        vertices += len(evaluated.to_mesh().vertices)
        evaluated.to_mesh_clear()
    materials = sorted({s.material.name for o in meshes for s in o.material_slots if s.material})
    actions = list(dict.fromkeys(a for entries in clips(objects).values() for _, a, _ in entries))
    actions += [o.animation_data.action for o in objects if o.animation_data and o.animation_data.action
                and o.animation_data.action not in actions]
    return {"objects": len(objects), "meshes": len(meshes), "vertices": vertices,
            "triangles": S.tri_count(meshes), "materials": materials, "textures": textures,
            "bounds": {"min": [round(float(v), 5) for v in (lo[0], lo[2], -hi[1])],
                       "max": [round(float(v), 5) for v in (hi[0], hi[2], -lo[1])]},
            "armatures": [{"name": o.name, "bones": len(o.data.bones)} for o in objects if o.type == "ARMATURE"],
            "animations": [{"name": a.name, "seconds": round((a.frame_range[1] - a.frame_range[0]) / fps, 4)}
                           for a in actions]}


def shots(views, points, prefix="", fitted_game=False):
    """(file stem, camera setup returning the image aspect, image record fields) per position.
    `fitted_game` frames the game view on `points` instead of the game's own target."""
    centre = (points.min(axis=0) + points.max(axis=0)) / 2
    for spec in views:
        view = spec["view"]
        fov = spec.get("fov_y", 30.0)
        if view in ORTHO:
            yield prefix + view, lambda cam, v=view: fit_ortho(cam, points, ORTHO[v], 1.0) or 1.0, {"view": view}
        elif view == "three_quarter":
            dr = orbit(spec.get("azimuth", 35.0), spec.get("elevation", 25.0))
            yield (prefix + view, lambda cam, dr=dr, fov=fov: fit_perspective(cam, points, dr, 1.0, fov) and 1.0,
                   {"view": view})
        elif view == "game_angle" or view == "game" and fitted_game:
            yield prefix + view, lambda cam, sp=spec: fit_game(cam, points, sp)["aspect"], {"view": view}
        elif view == "game":
            yield prefix + view, lambda cam, sp=spec: game_camera(cam, sp)["aspect"], {"view": view}
        else:
            frames = int(spec.get("frames", 8))
            directions = [orbit(i * 360.0 / frames, spec.get("elevation", 20.0)) for i in range(frames)]
            # One distance for the whole turn, so the model keeps its size between frames.
            distance = max(perspective_distance(points, centre, *basis(dr), math.radians(fov), 1.0)[0]
                           for dr in directions)
            for i, dr in enumerate(directions):
                yield (f"{prefix}turntable_{i:02d}",
                       lambda cam, dr=dr, fov=fov: fit_perspective(cam, points, dr, 1.0, fov, distance) and 1.0,
                       {"view": "turntable", "frame": i, "azimuth": round(i * 360.0 / frames, 3)})


# --- rendering --------------------------------------------------------------------------------

def review_set(objects):
    """What a review covers for `objects`: render, export and checks all use this one set."""
    return _descendants(objects)


def _descendants(objects):
    """The objects, their children, and the rigs that deform them (a clip lives on the rig)."""
    out = {}
    for obj in objects:
        for member in (obj, *obj.children_recursive):
            out.setdefault(member, None)
    for obj in list(out):
        for mod in getattr(obj, "modifiers", []):
            if mod.type == "ARMATURE" and mod.object:
                out.setdefault(mod.object, None)
    return list(out)


def _roots(objects):
    roots = {}
    for obj in objects:
        while obj.parent in objects:
            obj = obj.parent
        roots.setdefault(obj, None)
    return list(roots)


def _visible_meshes(objects):
    return [o for o in objects if o.type == "MESH" and o.visible_get()]


def reference_spec(entry):
    """{"path", "scale", "atlas", "tint"} from 'ref.glb', 'ref.glb@0.5' or a dict of those keys."""
    if isinstance(entry, dict):
        return {"path": Path(entry["path"]).resolve(), "scale": float(entry.get("scale", 1.0)),
                "atlas": str(Path(entry["atlas"]).resolve()) if entry.get("atlas") else None,
                "tint": entry.get("tint")}
    text = str(entry)
    head, sep, tail = text.rpartition("@")
    path, scale = Path(text), 1.0
    if sep:
        try:
            path, scale = Path(head), float(tail)
        except ValueError:
            pass
    return {"path": path.resolve(), "scale": scale, "atlas": None, "tint": None}


def dress_materials(state, meshes, atlas=None, tint=None):
    """Bind a runtime palette atlas and tint the GLB does not carry, on copies of the materials so
    the scene's own materials stay untouched; `state` puts the originals back on exit."""
    if not atlas and not tint:
        return
    image = None
    if atlas:
        image = bpy.data.images.load(str(Path(atlas).resolve()), check_existing=True)
        image.colorspace_settings.name = "sRGB"
    copies, bare = {}, None
    for obj in meshes:
        if not obj.material_slots and obj.data not in copies:
            # A mesh without a material, whose colour the runtime binds: give it a white one to dress.
            bare = bare or bpy.data.materials.new("review_bare")
            obj.data.materials.append(bare)
            state.undo.append(lambda mesh=obj.data: mesh.materials.pop())
            copies[obj.data] = None
        for i, slot in enumerate(obj.material_slots):
            original = slot.material
            if original is None:
                bare = bare or bpy.data.materials.new("review_bare")
            source = original or bare
            if source not in copies:
                copies[source] = _dressed(source, image, tint)
            state.undo.append(lambda obj=obj, i=i, original=original:
                              setattr(obj.material_slots[i], "material", original))
            slot.material = copies[source]


def _dressed(material, image, tint):
    mat = material.copy()
    tree = S.node_tree(mat)
    bsdf = S.find_node(tree, "BSDF_PRINCIPLED")
    if bsdf is None:
        return mat
    base = bsdf.inputs["Base Color"]
    if image is not None and S.find_node(tree, "TEX_IMAGE") is None:
        # No vector input: the texture reads the first (render) UV map, glTF TEXCOORD_0.
        texture = tree.nodes.new("ShaderNodeTexImage")
        texture.image = image
        texture.interpolation = "Closest"
        tree.links.new(texture.outputs["Color"], base)
    if tint:
        mix = tree.nodes.new("ShaderNodeMixRGB")
        mix.blend_type = "MULTIPLY"
        mix.inputs["Fac"].default_value = 1.0
        if base.is_linked:
            tree.links.new(base.links[0].from_socket, mix.inputs["Color1"])
        else:
            mix.inputs["Color1"].default_value = base.default_value
        mix.inputs["Color2"].default_value = (*as_linear(tint)[:3], 1.0)
        tree.links.new(mix.outputs["Color"], base)
    return mat


def apply_model_scale(scene, objects, scale):
    """Scale `objects` about the origin through a parent empty: the poses reset the objects' own
    transforms between frames, and the empty keeps the scale through that."""
    pivot = bpy.data.objects.new("review_model_scale", None)
    scene.collection.objects.link(pivot)
    pivot.scale = (scale, scale, scale)
    for root in _roots(objects):
        world = root.matrix_world.copy()
        root.parent = pivot
        root.matrix_parent_inverse = world @ root.matrix_basis.inverted()
    bpy.context.view_layer.update()
    return pivot


def lineup(candidate_points, references, state):
    """Import each reference GLB and stand it to the candidate's +X at true scale: bottoms on the
    candidate's bottom, depth centres aligned, gaps of LINEUP_GAP x the widest member."""
    lo, hi = candidate_points.min(axis=0), candidate_points.max(axis=0)
    members, imported = [], []
    for entry in references:
        spec = reference_spec(entry)
        path, scale = spec["path"], spec["scale"]
        objects, nodes = bpy_io.import_glb_nodes(path)
        rest_pose(objects)
        file_rest(nodes, path)
        if scale != 1.0:
            for root in _roots(objects):
                root.matrix_basis = Matrix.Scale(scale, 4) @ root.matrix_basis
            bpy.context.view_layer.update()
        meshes = _visible_meshes(objects)
        if not meshes:
            members.append({"name": path.stem, "path": str(path), "error": "no visible mesh"})
            continue
        dress_materials(state, meshes, spec["atlas"], spec["tint"])
        imported.append((spec, objects, S.evaluated_points(meshes)))
    widest = max([hi[0] - lo[0]] + [float(np.ptp(p[:, 0])) for _, _, p in imported])
    gap = LINEUP_GAP * widest
    cursor = hi[0] + gap
    objects_all, points_all = [], [candidate_points]
    for spec, objects, points in imported:
        path, scale = spec["path"], spec["scale"]
        plo, phi = points.min(axis=0), points.max(axis=0)
        shift = Vector((cursor - plo[0], (lo[1] + hi[1] - plo[1] - phi[1]) / 2, lo[2] - plo[2]))
        for root in _roots(objects):
            root.location += shift
        cursor += float(phi[0] - plo[0]) + gap
        points_all.append(points + np.array(shift))
        objects_all += objects
        members.append({"name": path.stem, "path": str(path), "scale": scale,
                        **{k: spec[k] for k in ("atlas", "tint") if spec[k]},
                        "offset_gltf": [round(v, 5) for v in S.blender_to_gltf(shift)],
                        "size_gltf": [round(abs(float(v)), 5) for v in S.blender_to_gltf(phi - plo)]})
    bpy.context.view_layer.update()
    return objects_all, np.vstack(points_all), members, gap


def render_objects(objects, job, asset=None, name=None, rest=False):
    """Render `objects` (and their children) of the current scene per `job` (render_views keys,
    `out` required) and write views.json; returns the report. Nothing is imported for the model
    and the scene is not reset: references and clip poses are added, rendered and removed, and
    render settings, camera, world, visibility, poses and selection are put back afterwards.
    `rest` renders the bind pose instead of the scene's current pose (when no `animation`)."""
    started = time.monotonic()
    job = normalize(job)
    out = Path(job["out"]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    scene = bpy.context.scene
    members = _descendants(objects)
    meshes = _visible_meshes(members)
    if not meshes:
        raise Skip("no visible mesh to render (an animation-only clip?)")
    name = name or (Path(asset).stem if asset else "model")
    fps = scene.render.fps / scene.render.fps_base
    timing = {}
    with SceneState(scene) as state:
        member_set = set(members)
        for obj in scene.objects:
            if obj not in member_set:
                obj.hide_render = True
        setup_render(scene, int(job["samples"]))
        scene.world = bpy.data.worlds.new("review_world")
        light = S.studio_world(scene, job["light"])
        # With only the HDR world nothing casts a shadow, yet the shadow system costs EEVEE about
        # half of every image; it runs only when a lamp renders (the no-HDR fallback suns).
        scene.world.use_sun_shadow = False
        scene.eevee.use_shadows = any(o.type == "LIGHT" and not o.hide_render for o in scene.objects)
        posed = "current"
        if rest:
            rest_pose(members)
            posed = "rest"
        if job.get("animation"):
            posed = pose(members, job["animation"], job.get("time"), fps)
        # Stats name the model's own materials, so they are taken before the dressed copies.
        stats = model_stats(members, meshes, [], fps)
        atlas = str(Path(job["atlas"]).resolve()) if job["atlas"] else None
        dress_materials(state, meshes, atlas, job["tint"])
        stats["textures"] = textures = save_textures(out, material_images(meshes))
        if job["model_scale"] != 1.0:
            apply_model_scale(scene, members, job["model_scale"])
        points = S.evaluated_points(meshes)
        passes = Passes(scene, int(job["samples"]), job.get("eevee_passes", False))
        cam = make_camera(scene)
        size = int(job["size"])
        timing["setup"] = round(time.monotonic() - started, 3)
        render_started = time.monotonic()
        floor = Floor(scene, framing_with_floor(points)) if job["floor"] else None
        images = render_shots(scene, cam, passes, list(shots(job["views"], points)), job["passes"], size,
                              job["background"], out, floor if job["floor"] == "all" else None)

        lineup_report = None
        if job["beside"]:
            refs, group_points, ref_members, gap = lineup(points, job["beside"], state)
            for obj in refs:
                obj.hide_render = False
            lineup_shots = [(f"lineup_{stem}", setup, {"view": "lineup", "base_view": extra["view"],
                                                       **{k: v for k, v in extra.items() if k != "view"}})
                            for stem, setup, extra in shots(job["lineup_views"], group_points, fitted_game=True)]
            group = [name] + [m["name"] for m in ref_members if "error" not in m]
            for record in render_shots(scene, cam, passes, lineup_shots, job["lineup_passes"], size,
                                       job["background"], out):
                images.append({**record, "members": group})
            for obj in refs:
                obj.hide_render = True
            lineup_report = {"members": [{"name": name}] + ref_members, "gap": round(float(gap), 5)}

        pose_report = []
        if job["poses"]:
            pose_report, pose_images = render_poses(state, members, meshes, job, scene, cam, passes,
                                                    int(job["pose_size"] or size), out, fps, floor)
            images += pose_images
        timing["render"] = round(time.monotonic() - render_started, 3)
        timing["images"] = len(images)
        timing["per_image"] = round(timing["render"] / max(1, len(images)), 4)

    sheet = contact_sheet(out, images) if job.get("sheet", True) and images else None
    asset_path = Path(asset).resolve() if asset else None
    report = {"tool": "render_views", "asset": str(asset_path) if asset_path else name,
              "sha256": hashlib.sha256(asset_path.read_bytes()).hexdigest() if asset_path else None,
              "blender": bpy.app.version_string, "seconds": round(time.monotonic() - started, 2),
              "timing": timing, "light": light, "pose": posed, "background": job["background"],
              "engines": {p: passes.engine(p) for p in dict.fromkeys(
                  job["passes"] + job["lineup_passes"] * bool(job["beside"]) + job["pose_passes"] * bool(job["poses"]))},
              "stats": stats, "images": images, "sheet": sheet}
    if lineup_report:
        report["lineup"] = lineup_report
    if job["poses"]:
        report["poses"] = pose_report
    if job["model_scale"] != 1.0:
        report["model_scale"] = job["model_scale"]
    if job["atlas"]:
        report["atlas"] = str(Path(job["atlas"]).resolve())
    if job["tint"]:
        report["tint"] = job["tint"]
    if job["floor"]:
        report["floor"] = {"views": job["floor"], "passes": ["beauty"], "grid_m": FLOOR_GRID}
    (out / "views.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def render_shots(scene, cam, passes, shot_list, pass_names, size, background, out, floor=None):
    """Pass-major, so the engine switches once per pass instead of once per image; the records
    come back view-major, the order the sheet and card read them. A floor shows in beauty only:
    the other passes override every material, the floor's included."""
    records = {}
    for k, pass_name in enumerate(pass_names):
        passes.apply(pass_name, size)
        if floor:
            floor.show(pass_name == "beauty")
        for i, (stem, setup, extra) in enumerate(shot_list):
            aspect = float(setup(cam) or 1.0)
            scene.render.resolution_x, scene.render.resolution_y = round(size * aspect), size
            file = out / f"{stem}_{pass_name}.png"
            scene.render.filepath = str(file)
            bpy.ops.render.render(write_still=True)
            if background != "transparent" and pass_name != "silhouette":
                composite_background(file, background)
            records[(i, k)] = {**extra, "pass": pass_name, "file": file.name,
                               "size": [scene.render.resolution_x, scene.render.resolution_y]}
    if floor:
        floor.show(False)
    return [records[key] for key in sorted(records)]


def framing_with_floor(points):
    """`points` plus the floor patch under them (a margin past the footprint and below y=0), so a
    framing fitted to them shows where the model meets the ground."""
    lo, hi = points.min(axis=0), points.max(axis=0)
    margin = 0.08 * float(max(hi - lo)) or 0.05
    z = min(float(lo[2]), 0.0) - margin
    corners = [(x, y, z) for x in (lo[0] - margin, hi[0] + margin) for y in (lo[1] - margin, hi[1] + margin)]
    return np.vstack([points, np.array(corners)])


def render_poses(state, members, meshes, job, scene, cam, passes, size, out, fps, floor=None):
    """Every pose framed alike (the union of all posed vertices, plus the ground patch under them
    with a floor), so poses and filmstrip frames compare at one scale."""
    report, posed = [], []
    for spec in job["poses"]:
        state.restore_pose()
        # Nodes a clip does not key stay at rest, as in the engine, not where the last pose left them.
        rest_pose(members)
        try:
            label = pose(members, spec["animation"], spec["time"], fps)
        except Skip as reason:
            report.append({**spec, "error": str(reason)})
            continue
        points = S.evaluated_points(meshes)
        posed.append((len(report), (spec, label, points)))
        lo, hi = points.min(axis=0) / job["model_scale"], points.max(axis=0) / job["model_scale"]
        report.append({**spec, "pose": label,
                       "bounds": {"min": [round(float(v), 5) for v in (lo[0], lo[2], -hi[1])],
                                  "max": [round(float(v), 5) for v in (hi[0], hi[2], -lo[1])]}})
    images = []
    if not posed:
        return report, images
    union = np.vstack([p for _, (_, _, p) in posed])
    if floor:
        union = framing_with_floor(union)
    for k, (spec, label, _) in posed:
        state.restore_pose()
        rest_pose(members)
        pose(members, spec["animation"], spec["time"], fps)
        # The ordinal keeps files apart when names sanitise alike or times print alike under :g.
        prefix = f"pose{k:02d}_{safe(spec['animation'])}_{spec['time']:g}_"
        pose_shots = [(stem, setup, {"view": "pose", "base_view": extra["view"],
                                     **{k: v for k, v in extra.items() if k != "view"},
                                     "animation": spec["animation"], "time": spec["time"]})
                      for stem, setup, extra in shots(job["pose_views"], union, prefix)]
        images += render_shots(scene, cam, passes, pose_shots, job["pose_passes"], size, job["background"], out,
                               floor)
    state.restore_pose()
    return report, images


def render_one(job):
    """Fresh scene, import the model (with grafted clips when `clip_source` is given), render."""
    model, out = Path(job["model"]).resolve(), Path(job["out"]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    S.reset_scene()
    source, graft = model, None
    if job.get("clip_source"):
        graft = Path(tempfile.mkdtemp(prefix="render_views_")) / f"{model.stem}_clips.glb"
        grafted = bpy_io.graft_clips(model, Path(job["clip_source"]).resolve(), graft)
        source = graft
    try:
        objects, nodes = bpy_io.import_glb_nodes(source)
        rest_pose(objects)
        file_rest(nodes, source)
    finally:
        if graft:
            graft.unlink(missing_ok=True)
            graft.parent.rmdir()
    if not _visible_meshes(objects):
        raise Skip(f"{model.name} has no visible mesh to render (an animation-only clip?)")
    report = render_objects(_roots(objects), {**job, "out": out}, asset=model, rest=True)
    if graft:
        report["clip_source"] = {k: v for k, v in grafted.items() if k != "path"}
        (out / "views.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"render_views: {len(report['images'])} images in {report['seconds']} s -> {out}")
    return report


def main(argv):
    """Render every --model; returns {"tool", "seconds", "models": [views.json report, ...]}.
    Safe to call repeatedly in one Blender: each model starts from a factory-empty scene."""
    started = time.monotonic()
    job = parse(argv)
    models = job["model"] if isinstance(job["model"], list) else [job["model"]]
    out = Path(job["out"])
    reports = []
    for model in models:
        target = out / Path(model).stem if len(models) > 1 else out
        try:
            reports.append(render_one({**job, "model": model, "out": target}))
        except Skip as reason:
            target.mkdir(parents=True, exist_ok=True)
            report = {"tool": "render_views", "asset": str(Path(model).resolve()), "error": str(reason),
                      "blender": bpy.app.version_string, "stats": {}, "images": []}
            (target / "views.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
            reports.append(report)
            print(f"render_views: skipped {model}: {reason}")
    return {"tool": "render_views", "seconds": round(time.monotonic() - started, 2), "models": reports}


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
