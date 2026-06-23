# Headless OBJ(+MTL) -> GLB converter via Blender.
# Reusable asset-pipeline tool (any pack), not game-specific.
#
#   "<blender>" --background --python tools/assets/obj_to_glb.py -- <outdir> <a.obj> <b.obj> ...
#
# Each .obj is imported into an empty scene (reading its sibling .mtl for base
# colors) and exported as a self-contained .glb in <outdir>.
import bpy
import sys
import os

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argv) < 2:
    print("usage: -- <outdir> <obj> [obj ...]")
    sys.exit(1)
outdir = argv[0]
objs = argv[1:]
os.makedirs(outdir, exist_ok=True)

ok = 0
for obj in objs:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        bpy.ops.wm.obj_import(filepath=obj)  # Blender 4.x
    except AttributeError:
        bpy.ops.import_scene.obj(filepath=obj)  # older Blender
    name = os.path.splitext(os.path.basename(obj))[0]
    out = os.path.join(outdir, name + ".glb")
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=False)
    print("GLB " + out)
    ok += 1

print("converted %d/%d" % (ok, len(objs)))
