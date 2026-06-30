# Headless OBJ(+MTL) -> GLB converter via Blender.
# Reusable asset-pipeline tool (any pack), not game-specific.
#
#   "<blender>" --background --python ai_studio/assets/prep/conversion/obj_to_glb.py -- <outdir> [--no-split] <a.obj> <b.obj> ...
#
# Each .obj is imported into an empty scene (reading its sibling .mtl for base
# colors) and exported as a self-contained .glb in <outdir>.
#
# split-by-material (DEFAULT ON): each material becomes its own object -> its own
# glTF mesh/node. Needed because engines that read one material per mesh (e.g.
# neotolis-engine scene import, primitives[0] only) otherwise show a multi-material
# model in a single colour. Pass --no-split to keep one multi-primitive mesh.
import bpy
import sys
import os

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argv) < 2:
    print("usage: -- <outdir> [--no-split] <obj> [obj ...]")
    sys.exit(1)
outdir = argv[0]
split = True
objs = []
for a in argv[1:]:
    if a == "--no-split":
        split = False
    else:
        objs.append(a)
os.makedirs(outdir, exist_ok=True)


def fix_materials():
    # backface culling ON -> glTF doubleSided:false (matches clean source kits;
    # double-sided makes interior/back faces show through as artifacts).
    for m in bpy.data.materials:
        m.use_backface_culling = True


def split_by_material():
    # separate every imported mesh object into one object per material
    for o in list(bpy.context.scene.objects):
        if o.type != "MESH" or len(o.material_slots) < 2:
            continue
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.separate(type="MATERIAL")
        bpy.ops.object.mode_set(mode="OBJECT")


ok = 0
for obj in objs:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    try:
        bpy.ops.wm.obj_import(filepath=obj)  # Blender 4.x
    except AttributeError:
        bpy.ops.import_scene.obj(filepath=obj)  # older Blender
    fix_materials()
    if split:
        split_by_material()
    name = os.path.splitext(os.path.basename(obj))[0]
    out = os.path.join(outdir, name + ".glb")
    meshes = sum(1 for o in bpy.context.scene.objects if o.type == "MESH")
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=False)
    print("GLB %s (%d mesh%s)" % (out, meshes, "" if meshes == 1 else "es"))
    ok += 1

print("converted %d/%d" % (ok, len(objs)))
