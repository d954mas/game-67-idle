"""GLB import/export with presets, pivot placement and a report read back from the written file.

The report measures the GLB itself (not the Blender scene), so it states what the engine will
load: triangles, bounds in glTF axes, and the bounds centre kept apart from the origin, because
a generator that read the centre as the origin once left props floating above the floor.
"""
import os
from pathlib import Path

import bpy
from mathutils import Vector

from ai_studio.assets.tools.blender.lib import glb as G
from ai_studio.assets.tools.blender.lib.bpy_scene import gltf_to_blender, world_bounds

PIVOT_PROP = "nt_pivot"
PIVOT_RULES = ("floor", "center", "top", "keep")

BASE_OPTIONS = dict(export_format="GLB", use_selection=True, export_yup=True, export_extras=False,
                    export_materials="EXPORT", export_normals=True, export_texcoords=True,
                    export_tangents=False, export_cameras=False, export_lights=False)
PRESETS = {
    "static": dict(export_apply=True, export_animations=False, export_skins=False,
                   export_vertex_color="ACTIVE", export_attributes=False),
    "skinned": dict(export_apply=False, export_animations=False, export_skins=True),
    "animated": dict(export_apply=False, export_skins=True, export_animations=True,
                     export_animation_mode="ACTIONS", export_force_sampling=True,
                     export_def_bones=True, export_anim_single_armature=True,
                     export_reset_pose_bones=True, export_optimize_animation_size=False),
}


def import_glb(path, drop_bone_shapes=True, **options):
    """Import a GLB; returns the objects it created.

    The importer parks an Icosphere bone shape in an excluded collection; it is dropped by
    default so object lists, triangle counts and exports see only the asset.
    """
    known = _operator_options(bpy.ops.import_scene.gltf)
    if drop_bone_shapes and "disable_bone_shape" in known:
        options.setdefault("disable_bone_shape", True)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path), **{k: v for k, v in options.items() if k in known})
    created = [o for o in bpy.data.objects if o not in before]
    if drop_bone_shapes:
        hidden = bpy.data.collections.get("glTF_not_exported")
        for obj in [o for o in created if hidden and hidden in o.users_collection]:
            created.remove(obj)
            bpy.data.objects.remove(obj, do_unlink=True)
        if hidden and not hidden.objects:
            bpy.data.collections.remove(hidden)
    return created


def _operator_options(op):
    return {p.identifier for p in op.get_rna_type().properties}


def _with_rig(objects):
    """Add the armatures and ancestors a skinned mesh needs to export with its skin."""
    out = list(objects)
    for obj in objects:
        rigs = [m.object for m in getattr(obj, "modifiers", []) if m.type == "ARMATURE" and m.object]
        parent = obj.parent
        while parent:
            rigs.append(parent)
            parent = parent.parent
        out += [r for r in rigs if r not in out]
    return out


def export_glb(path, objects=None, preset="static", **overrides):
    """Export `objects` (default: every visible mesh/armature/empty) as Y-up GLB; returns a report.

    Presets: static (modifiers applied, active colour attribute, no skins/animation), skinned
    (skin, no animation), animated (actions sampled, deform bones only, one armature). Any
    exporter option can be overridden; options this Blender's exporter lacks are dropped and
    listed in the report instead of failing.
    """
    if preset not in PRESETS:
        raise ValueError(f"preset must be one of {sorted(PRESETS)}, not {preset!r}")
    if objects is None:
        objects = [o for o in bpy.context.scene.objects
                   if o.visible_get() and o.type in ("MESH", "ARMATURE", "EMPTY")]
    if not objects:
        raise ValueError("nothing to export: no objects given and no visible mesh/armature/empty")
    if preset != "static":
        objects = _with_rig(objects)
    known = _operator_options(bpy.ops.export_scene.gltf)
    wanted = {**BASE_OPTIONS, **PRESETS[preset], **overrides}
    options = {k: v for k, v in wanted.items() if k in known}
    dropped = sorted(set(wanted) - set(options))
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    layer = bpy.context.view_layer
    # Objects linked since the last update are missing from layer.objects, so nothing gets
    # selected and the exporter silently writes an empty scene.
    layer.update()
    for obj in layer.objects:
        obj.select_set(obj in objects)
    layer.objects.active = next((o for o in objects if o.type == "ARMATURE"), objects[0] if objects else None)
    bpy.ops.export_scene.gltf(filepath=str(path), **options)
    pivots = sorted({str(o.get(PIVOT_PROP)) for o in objects if o.get(PIVOT_PROP)})
    report = glb_report(path)
    report.update(preset=preset, objects=[o.name for o in objects],
                  pivot=pivots[0] if len(pivots) == 1 else (pivots or None), dropped_options=dropped)
    return report


def glb_report(path):
    """sha256, triangles and rest-pose bounds of a written GLB, in glTF axes."""
    doc = G.Glb.load(path)
    positions, faces, _, _ = doc.merged()
    lo = positions.min(axis=0) if len(positions) else [0.0, 0.0, 0.0]
    hi = positions.max(axis=0) if len(positions) else [0.0, 0.0, 0.0]
    return {"path": str(path), "sha256": G.sha256(path), "bytes": os.path.getsize(path),
            "triangles": int(len(faces)),
            "bounds": {"min": [round(float(v), 6) for v in lo], "max": [round(float(v), 6) for v in hi]},
            "bounds_centre": [round((float(a) + float(b)) / 2, 6) for a, b in zip(lo, hi)]}


def _top(obj):
    while obj.parent:
        obj = obj.parent
    return obj


def apply_pivot(objs=None, rule="floor", offset=(0, 0, 0)):
    """Move the asset so the chosen point of its bounds lands on the world origin.

    floor = bottom centre (props standing on the ground), center, top = top centre (hanging
    things), keep = origin unchanged. `offset` is in glTF axes and is added to that point.
    Whole hierarchies move, so skins and parented parts stay where they were relative to
    each other. The rule is stored on the objects and shows up in the export report.
    """
    if rule not in PIVOT_RULES:
        raise ValueError(f"pivot rule must be one of {PIVOT_RULES}, not {rule!r}")
    if objs is None:
        objs = [o for o in bpy.context.scene.objects if o.visible_get()]
    point = Vector((0.0, 0.0, 0.0))
    if rule != "keep":
        lo, hi = world_bounds(objs)
        point = (lo + hi) / 2
        point.z = {"floor": lo.z, "center": point.z, "top": hi.z}[rule]
    point += Vector(gltf_to_blender(offset))
    roots = {_top(o) for o in objs}
    for root in roots:
        root.location -= point
    bpy.context.view_layer.update()
    for obj in set(objs) | roots:
        obj[PIVOT_PROP] = rule
    return {"rule": rule, "moved_by_gltf": [round(-point.x, 6), round(-point.z, 6), round(point.y, 6)]}
