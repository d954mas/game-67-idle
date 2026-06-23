# Headless GLB -> isometric thumbnail PNG (Blender). Reusable asset-pipeline tool.
#
#   "<blender>" --background --python tools/assets/render_thumbs.py -- <outdir> <size> <a.glb> ...
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
    print("usage: -- <outdir> <size> <a.glb> [b.glb ...]")
    sys.exit(1)
outdir = os.path.abspath(argv[0])
size = int(argv[1])
glbs = argv[2:]
os.makedirs(outdir, exist_ok=True)

engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
eevee = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"


def setup_render():
    s = bpy.context.scene
    s.render.engine = eevee
    s.render.resolution_x = size
    s.render.resolution_y = size
    s.render.film_transparent = True
    s.render.image_settings.file_format = "PNG"
    s.render.image_settings.color_mode = "RGBA"


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
for glb in glbs:
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

    sun_data = bpy.data.lights.new("sun", "SUN")
    sun_data.energy = 3.0
    sun = bpy.data.objects.new("sun", sun_data)
    scene.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(40))

    world = bpy.data.worlds.new("w")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.7

    name = os.path.splitext(os.path.basename(glb))[0]
    scene.render.filepath = os.path.join(outdir, name + ".png")
    bpy.ops.render.render(write_still=True)
    ok += 1

print("rendered %d/%d" % (ok, len(glbs)))
