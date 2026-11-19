"""Render a GLB for review: fitted views x passes as PNGs, plus views.json with model stats.

Run inside Blender through the runner:

    node ai_studio/dev_environment/python_run.mjs ai_studio/assets/tools/blender/run_blender.py \
        ai_studio/assets/tools/blender/render_views.py -- --model m.glb --out tmp/review

Flags: --views front right three_quarter game turntable (or comma-separated), --passes beauty
clay wire silhouette, --size 768, --light studio_hdr|flat, --background transparent|#hex,
--animation NAME --time SECONDS, --samples 16, --game '{"height": 1.8, ...}',
--turntable-frames 8, --no-sheet. Several --model files render in one Blender start into
<out>/<model stem>/ each, which saves the start-up and first-frame cost per model. `--job job.json` takes the same keys as JSON; a view there may be an
object such as {"view": "game", "height": 1.8, "behind": 2.0, "offset": 0, "pitch": 16,
"fov_y": 55, "yaw": 0, "target": [0, 0, 0], "aspect": 1.78} or {"view": "turntable", "frames": 8}.

Views use glTF axes: +Z is the model's front, so `front` looks at it along -Z and `right`
sees its +X side (Blender's numpad views). front/back/left/right/top are orthographic and fitted
to the vertices; three_quarter and turntable are perspective and fitted; `game` reproduces an
engine follow camera (yaw 0 = behind the model, looking along +Z) and is not fitted.
"""
import argparse
import hashlib
import json
import math
import re
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

from ai_studio.assets.tools.blender.lib import bpy_io
from ai_studio.assets.tools.blender.lib import bpy_scene as S
from ai_studio.assets.tools.blender.lib.color import as_linear, linear_to_srgb, write_png

VIEWS = ("front", "back", "left", "right", "top", "three_quarter", "game", "turntable")
PASSES = ("beauty", "clay", "wire", "silhouette")
DEFAULTS = {"views": ["front", "right", "three_quarter"], "passes": ["beauty", "clay", "wire"],
            "size": 768, "light": "studio_hdr", "background": "transparent", "samples": 16}
# Direction from the model toward the camera, Blender axes (glTF +Z front = Blender -Y).
ORTHO = {"front": (0, -1, 0), "back": (0, 1, 0), "right": (1, 0, 0), "left": (-1, 0, 0), "top": (0, 0, 1)}
GAME = {"height": 1.8, "behind": 2.0, "offset": 0.0, "pitch": 16.0, "fov_y": 55.0, "yaw": 0.0,
        "target": [0.0, 0.0, 0.0], "aspect": 16 / 9}
MARGIN = 1.08
CLAY = (0.55, 0.55, 0.55)


def parse(argv):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
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
    args = p.parse_args(argv)
    job = dict(DEFAULTS)
    if args.job:
        job.update(json.loads(Path(args.job).read_text(encoding="utf-8")))
    if args.no_sheet:
        job["sheet"] = False
    for key in ("model", "out", "size", "light", "background", "animation", "time", "samples"):
        if getattr(args, key) is not None:
            job[key] = getattr(args, key)
    for key in ("views", "passes"):
        if getattr(args, key):
            job[key] = [v for item in getattr(args, key) for v in item.split(",") if v]
    views = []
    for view in job["views"]:
        view = dict(view) if isinstance(view, dict) else {"view": view}
        view["view"] = view.get("view") or view.get("name")
        if view["view"] == "game" and args.game:
            view = {**json.loads(args.game), **view}
        if view["view"] == "turntable" and args.turntable_frames:
            view.setdefault("frames", args.turntable_frames)
        views.append(view)
    job["views"] = views
    bad = [v["view"] for v in views if v["view"] not in VIEWS] + [x for x in job["passes"] if x not in PASSES]
    if bad:
        p.error(f"unknown view/pass {bad}; views: {VIEWS}, passes: {PASSES}")
    if not job.get("model") or not job.get("out"):
        p.error("--model and --out (or a job with both) are required")
    return job


# --- scene ------------------------------------------------------------------------------------

def setup_render(scene, samples):
    scene.render.engine = S.eevee_engine()
    scene.eevee.taa_render_samples = samples
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    # AgX/Filmic shift palette colours; Standard keeps an authored '#hex' looking like itself.
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0


class Skip(Exception):
    """A model this run cannot render; the batch records why and goes on with the next one."""


def pose(objects, animation, seconds, fps):
    """Bind pose by default; otherwise the named clip at `seconds`. Returns a description."""
    animated = [o for o in objects if o.animation_data]
    if not animation:
        for obj in animated:
            obj.animation_data.action = None
        for obj in objects:
            if obj.type == "ARMATURE":
                for bone in obj.pose.bones:
                    bone.matrix_basis.identity()
        bpy.context.view_layer.update()
        return "rest"
    hits = 0
    for obj in animated:
        data = obj.animation_data
        for track in data.nla_tracks:
            track.mute = True
            strip = next((s for s in track.strips if animation in (track.name, s.action and s.action.name)), None)
            if strip and strip.action:
                data.action = strip.action
                if hasattr(data, "action_slot") and getattr(strip, "action_slot", None):
                    data.action_slot = strip.action_slot
                hits += 1
    if not hits:
        names = sorted({t.name for o in animated for t in o.animation_data.nla_tracks})
        raise Skip(f"animation {animation!r} not found; the model has {names}")
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
    # The Wireframe node draws the triangles the GPU gets, which is what a GLB ships.
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
    return {"beauty": None, "clay": clay, "wire": wire, "silhouette": black}, edge


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


def game_camera(cam, spec):
    """Engine follow camera: `behind` the target along -Z (glTF), `height` up, `offset` on X,
    pitched down by `pitch`, vertical FOV `fov_y`; `yaw` turns the rig about the target."""
    g = {**GAME, **{k: v for k, v in spec.items() if k != "view"}}
    target = Vector(S.gltf_to_blender(g["target"]))
    yaw = math.radians(g["yaw"])
    rot = lambda v: Vector((v[0] * math.cos(yaw) - v[1] * math.sin(yaw), v[0] * math.sin(yaw) + v[1] * math.cos(yaw), v[2]))
    eye = target + rot(S.gltf_to_blender((g["offset"], g["height"], -g["behind"])))
    pitch = math.radians(g["pitch"])
    forward = rot((0.0, -math.cos(pitch), -math.sin(pitch)))
    set_perspective(cam, math.radians(g["fov_y"]))
    d, _, u = basis(-forward)
    place(cam, eye, d, u)
    cam.data.clip_start, cam.data.clip_end = 0.05, 500.0
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


def save_textures(out, skip):
    textures = []
    for image in bpy.data.images:
        if image.source != "FILE" or image in skip or not image.size[0]:
            continue
        file = Path("textures") / f"{safe(image.name)}.png"
        write_png(out / file, np.round(np.clip(read_pixels(image), 0, 1) * 255).astype(np.uint8))
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
    return {"objects": len(objects), "meshes": len(meshes), "vertices": vertices,
            "triangles": S.tri_count(meshes), "materials": materials, "textures": textures,
            "bounds": {"min": [round(float(v), 5) for v in (lo[0], lo[2], -hi[1])],
                       "max": [round(float(v), 5) for v in (hi[0], hi[2], -lo[1])]},
            "armatures": [{"name": o.name, "bones": len(o.data.bones)} for o in objects if o.type == "ARMATURE"],
            "animations": [{"name": a.name, "seconds": round((a.frame_range[1] - a.frame_range[0]) / fps, 4)}
                           for a in bpy.data.actions]}


def shots(views, points):
    """(file stem, camera setup returning the image aspect, image record fields) per position."""
    centre = (points.min(axis=0) + points.max(axis=0)) / 2
    for spec in views:
        view = spec["view"]
        fov = spec.get("fov_y", 30.0)
        if view in ORTHO:
            yield view, lambda cam, v=view: fit_ortho(cam, points, ORTHO[v], 1.0) or 1.0, {"view": view}
        elif view == "three_quarter":
            dr = orbit(spec.get("azimuth", 35.0), spec.get("elevation", 25.0))
            yield view, lambda cam, dr=dr, fov=fov: fit_perspective(cam, points, dr, 1.0, fov) and 1.0, {"view": view}
        elif view == "game":
            yield view, lambda cam, sp=spec: game_camera(cam, sp)["aspect"], {"view": view}
        else:
            frames = int(spec.get("frames", 8))
            directions = [orbit(i * 360.0 / frames, spec.get("elevation", 20.0)) for i in range(frames)]
            # One distance for the whole turn, so the model keeps its size between frames.
            distance = max(perspective_distance(points, centre, *basis(dr), math.radians(fov), 1.0)[0]
                           for dr in directions)
            for i, dr in enumerate(directions):
                yield (f"turntable_{i:02d}",
                       lambda cam, dr=dr, fov=fov: fit_perspective(cam, points, dr, 1.0, fov, distance) and 1.0,
                       {"view": "turntable", "frame": i, "azimuth": round(i * 360.0 / frames, 3)})


def main(argv):
    job = parse(argv)
    models = job["model"] if isinstance(job["model"], list) else [job["model"]]
    out = Path(job["out"])
    for model in models:
        target = out / Path(model).stem if len(models) > 1 else out
        try:
            render_one({**job, "model": model, "out": target})
        except Skip as reason:
            target.mkdir(parents=True, exist_ok=True)
            (target / "views.json").write_text(json.dumps(
                {"tool": "render_views", "asset": str(Path(model).resolve()), "error": str(reason),
                 "blender": bpy.app.version_string, "stats": {}, "images": []}, indent=2), encoding="utf-8")
            print(f"render_views: skipped {model}: {reason}")


def render_one(job):
    started = time.monotonic()
    model, out = Path(job["model"]).resolve(), Path(job["out"]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    scene = S.reset_scene()
    setup_render(scene, int(job["samples"]))
    light = S.studio_world(scene, job["light"])
    skip = set(bpy.data.images)
    objects = bpy_io.import_glb(model)
    meshes = [o for o in objects if o.type == "MESH" and o.visible_get()]
    if not meshes:
        raise Skip(f"{model.name} has no visible mesh to render (an animation-only clip?)")
    fps = scene.render.fps / scene.render.fps_base
    posed = pose(objects, job.get("animation"), job.get("time"), fps)
    points = S.evaluated_points(meshes)
    textures = save_textures(out, skip)
    overrides, wire_edge = override_materials()
    cam = make_camera(scene)
    size = int(job["size"])
    background = job["background"]
    images = []
    for name, setup, extra in shots(job["views"], points):
        aspect = float(setup(cam) or 1.0)
        scene.render.resolution_x, scene.render.resolution_y = round(size * aspect), size
        # A one-pixel wire vanishes in a large image and floods a thumbnail; scale with it.
        wire_edge.inputs["Size"].default_value = max(1.0, size / 768)
        for pass_name in job["passes"]:
            bpy.context.view_layer.material_override = overrides[pass_name]
            scene.render.film_transparent = True
            file = out / f"{name}_{pass_name}.png"
            scene.render.filepath = str(file)
            bpy.ops.render.render(write_still=True)
            if background != "transparent" and pass_name != "silhouette":
                composite_background(file, background)
            images.append({**extra, "pass": pass_name, "file": file.name,
                           "size": [scene.render.resolution_x, scene.render.resolution_y]})
    bpy.context.view_layer.material_override = None
    sheet = contact_sheet(out, images) if job.get("sheet", True) else None
    report = {"tool": "render_views", "asset": str(model),
              "sha256": hashlib.sha256(model.read_bytes()).hexdigest(),
              "blender": bpy.app.version_string, "seconds": round(time.monotonic() - started, 2),
              "light": light, "pose": posed, "background": background,
              "stats": model_stats(objects, meshes, textures, fps), "images": images, "sheet": sheet}
    (out / "views.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"render_views: {len(images)} images in {report['seconds']} s -> {out}")


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
