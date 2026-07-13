# Headless GLB -> isometric thumbnail PNG (Blender). Reusable asset-pipeline tool.
#
#   "<blender>" --background --python ai_studio/assets/previews/render_thumbs.py -- <outdir> <size> <a.glb> ...
#
# Renders each glb from a fixed isometric angle (azimuth 45, elevation 30) with a
# flat neutral light + transparent background, so library thumbnails are crisp and
# match the in-viewer 3D look (model-viewer camera-orbit "45deg 60deg").
import bpy
import sys
import os
import math
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argv) < 3:
    print("usage: -- <outdir> <size> [--webp] <a.glb|a.glb::stem|@manifest.txt> ...")
    sys.exit(1)
# Shared studio environment (same file the web model-viewer uses), so the PNG
# preview and the live 3D are lit by one source. Falls back to suns if missing.
HDR_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "studio_env.hdr")
WEBP = "--webp" in argv
argv = [x for x in argv if x != "--webp"]
outdir = os.path.abspath(argv[0])
size = int(argv[1])

# Items may be a glb path, "glb::outstem" (explicit output name - avoids basename
# collisions when packs reuse names), or "@file" listing such items one per line
# (avoids the Windows command-line length limit for thousands of models).
items = []
for tok in argv[2:]:
    if tok.startswith("@"):
        with open(tok[1:], "r", encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if line:
                    items.append(line)
    else:
        items.append(tok)
glbs = items
os.makedirs(outdir, exist_ok=True)

engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
eevee = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"


def setup_render():
    s = bpy.context.scene
    s.render.engine = eevee
    s.render.resolution_x = size
    s.render.resolution_y = size
    s.render.film_transparent = True
    s.render.image_settings.file_format = "WEBP" if WEBP else "PNG"
    s.render.image_settings.color_mode = "RGBA"
    if WEBP:
        s.render.image_settings.quality = 90  # webp: small but crisp, keeps alpha
    # Blender 4.x defaults to AgX which darkens/desaturates; flat assets want
    # Standard so colours match the source (bright low-poly look).
    try:
        s.view_settings.view_transform = "Standard"
    except Exception:
        pass


def world_bounds(objs):
    mn = mathutils.Vector((1e9, 1e9, 1e9))
    mx = mathutils.Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ mathutils.Vector(c)
            for i in range(3):
                mn[i] = min(mn[i], w[i])
                mx[i] = max(mx[i], w[i])
    return mn, mx


ok = 0
for item in glbs:
    glb, _, stem = item.partition("::")        # "glb" or "glb::outstem"
    bpy.ops.wm.read_factory_settings(use_empty=True)
    setup_render()
    bpy.ops.import_scene.gltf(filepath=glb)
    scene = bpy.context.scene
    meshes = [o for o in scene.objects if o.type == "MESH"]
    if not meshes:
        continue
    mn, mx = world_bounds(meshes)
    center = (mn + mx) * 0.5
    dim = mx - mn
    maxd = max(dim.x, dim.y, dim.z) or 1.0

    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = maxd * 1.45
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    az, el = math.radians(45), math.radians(30)
    d = mathutils.Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
    cam.location = center + d * maxd * 3.0
    look = (center - cam.location).normalized()
    cam.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()

    world = bpy.data.worlds.new("w")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    if os.path.exists(HDR_PATH):
        # Shared equirect studio HDR (high warm key + dim cool fill + low
        # ambient). Same source as the web model-viewer; no mapping node so the
        # equirect maps straight, with the key near the top of the sphere.
        env = nt.nodes.new("ShaderNodeTexEnvironment")
        env.image = bpy.data.images.load(HDR_PATH, check_existing=True)
        nt.links.new(env.outputs["Color"], bg.inputs["Color"])
        bg.inputs[1].default_value = 1.0
    else:
        # fallback: strong directional key + modest fill + low ambient, so the
        # lit top stays clearly brighter than the side faces (edges readable).
        key = bpy.data.lights.new("key", "SUN")
        key.energy = 2.6
        key.angle = math.radians(15)
        ko = bpy.data.objects.new("key", key)
        scene.collection.objects.link(ko)
        ko.rotation_euler = (math.radians(40), math.radians(6), math.radians(35))
        fill = bpy.data.lights.new("fill", "SUN")
        fill.energy = 0.6
        fo = bpy.data.objects.new("fill", fill)
        scene.collection.objects.link(fo)
        fo.rotation_euler = (math.radians(62), 0, math.radians(215))
        bg.inputs[1].default_value = 0.5

    name = stem or os.path.splitext(os.path.basename(glb))[0]
    ext = ".webp" if WEBP else ".png"
    scene.render.filepath = os.path.join(outdir, name + ext)
    bpy.ops.render.render(write_still=True)
    ok += 1

print("rendered %d/%d" % (ok, len(glbs)))
