"""Blender side of the Kimodo tool: SOMA-30 motion preview and retarget onto a character.

Run through client.mjs, not by hand:
  blender -b --factory-startup --python blender_kimodo.py -- preview --motion-dir D --kimodo-scripts S
  blender -b --factory-startup --python blender_kimodo.py -- retarget --bvh B --character C.glb --map M.json --out-dir D

Retarget assumes both rigs rest in a T-pose: a bone's world rotation delta from its
rest transfers to the mapped target bone once the two rest frames (forward, left,
up) are aligned. Foot lock then pins planted ankles with analytic two-bone IK.
"""
import argparse
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Euler, Matrix, Quaternion, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from motion_math import detect_contacts, foot_slide, lock_plan, remove_twist, two_bone_ik  # noqa: E402

FPS = 30
SOMA_FACING = {"left": "LeftArm", "right": "RightArm", "foot": "LeftFoot", "toe": "LeftToeBase"}
SHEET_FRAMES = 8


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(prog="blender_kimodo.py")
    sub = parser.add_subparsers(dest="mode", required=True)
    preview = sub.add_parser("preview")
    preview.add_argument("--motion-dir", required=True)
    preview.add_argument("--kimodo-scripts", required=True)
    retarget = sub.add_parser("retarget")
    retarget.add_argument("--bvh", required=True)
    retarget.add_argument("--character", required=True)
    retarget.add_argument("--map", required=True)
    retarget.add_argument("--out-dir", required=True)
    retarget.add_argument("--clip-name", default="kimodo")
    retarget.add_argument("--offsets", default="{}", help="JSON {bone: [x, y, z] degrees}, merged over the map")
    retarget.add_argument("--no-foot-lock", action="store_true")
    return parser.parse_args(argv)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = FPS
    return scene


def import_bvh(path):
    before = set(bpy.data.objects)
    bpy.ops.import_anim.bvh(filepath=path, global_scale=0.01, update_scene_fps=False, update_scene_duration=False)
    arm = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
    start, end = (int(round(v)) for v in arm.animation_data.action.frame_range)
    return arm, start, end


def rest_world(arm, name):
    return arm.matrix_world @ arm.data.bones[name].matrix_local


def facing_frame(arm, left, right, foot, toe):
    """Rotation whose columns are the character's forward, left and up in world space."""
    up = Vector((0, 0, 1))
    side = rest_world(arm, left).translation - rest_world(arm, right).translation
    side = (side - side.project(up)).normalized()
    fwd = rest_world(arm, toe).translation - rest_world(arm, foot).translation
    fwd = (fwd - fwd.project(up) - fwd.project(side)).normalized()
    return Matrix((fwd, side, up)).transposed()


# ---------------------------------------------------------------- pose sheet

def render_sheet(scene, focus, frame_axes, start, end, out_png, height=1.8):
    """Orthographic front and side renders of SHEET_FRAMES frames, tiled into one PNG."""
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("sheet")
    world.color = (1.0, 1.0, 1.0)
    scene.world = world

    picks = [start + round(i * (end - start) / (SHEET_FRAMES - 1)) for i in range(SHEET_FRAMES)]
    points = []
    for f in picks:
        scene.frame_set(f)
        points.append(focus())
    lo = Vector((min(p.x for p in points), min(p.y for p in points), 0))
    hi = Vector((max(p.x for p in points), max(p.y for p in points), 0))
    center = (lo + hi) / 2
    center.z = height * 0.5

    tile = 256
    rows = []
    tmp_dir = os.path.join(os.path.dirname(out_png), "_frames")
    os.makedirs(tmp_dir, exist_ok=True)
    for view, axis in (("front", frame_axes.col[0]), ("side", frame_axes.col[1])):
        cam_data = bpy.data.cameras.new(view)
        cam_data.type = "ORTHO"
        cam = bpy.data.objects.new(view, cam_data)
        scene.collection.objects.link(cam)
        scene.camera = cam
        scene.render.resolution_x = tile
        scene.render.resolution_y = tile
        row = []
        for f, p in zip(picks, points):
            scene.frame_set(f)
            target = Vector((p.x, p.y, center.z))
            cam.location = target + axis * 8.0
            cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
            cam_data.ortho_scale = height * 1.25
            path = os.path.join(tmp_dir, f"{view}_{f:04d}.png")
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            image = bpy.data.images.load(path)
            pixels = np.array(image.pixels[:], dtype=np.float32).reshape(tile, tile, 4)
            bpy.data.images.remove(image)
            os.remove(path)
            row.append(pixels)
        rows.append(np.concatenate(row, axis=1))
    os.rmdir(tmp_dir)
    # Blender image rows run bottom-up, so the front row goes last to appear on top.
    sheet = np.concatenate(list(reversed(rows)), axis=0)
    out = bpy.data.images.new("sheet", width=sheet.shape[1], height=sheet.shape[0], alpha=True)
    out.pixels = sheet.ravel()
    out.filepath_raw = out_png
    out.file_format = "PNG"
    out.save()
    return picks


# ---------------------------------------------------------------- preview

def bone_stick(name, length, radius):
    """Four-sided spike along -Y ending at the origin: a bone-parented child sits at the bone tail."""
    mesh = bpy.data.meshes.new(name)
    r = radius
    verts = [(0, -length, 0), (r, -length * 0.85, 0), (0, -length * 0.85, r), (-r, -length * 0.85, 0), (0, -length * 0.85, -r), (0, 0, 0)]
    faces = [(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 1), (5, 2, 1), (5, 3, 2), (5, 4, 3), (5, 1, 4)]
    mesh.from_pydata(verts, [], faces)
    return mesh


def run_preview(args):
    sys.path.insert(0, args.kimodo_scripts)
    from pathlib import Path
    from export_bvh import convert_motion_to_bvh

    motion_dir = Path(args.motion_dir)
    bvh = motion_dir / "motion.bvh"
    convert_motion_to_bvh(motion_dir, bvh, fps=float(FPS), scale=100.0)

    scene = reset_scene()
    arm, start, end = import_bvh(str(bvh))
    scene.frame_start, scene.frame_end = start, end
    material = bpy.data.materials.new("stick")
    material.diffuse_color = (0.25, 0.3, 0.4, 1.0)
    for bone in arm.data.bones:
        if bone.length < 1e-4:
            continue
        side = (0.8, 0.2, 0.2, 1.0) if "Right" in bone.name else (0.2, 0.4, 0.9, 1.0) if "Left" in bone.name else None
        mat = material
        if side:
            mat = bpy.data.materials.new(bone.name)
            mat.diffuse_color = side
        stick = bpy.data.objects.new(bone.name, bone_stick(bone.name, bone.length, max(bone.length * 0.12, 0.012)))
        stick.data.materials.append(mat)
        scene.collection.objects.link(stick)
        stick.parent = arm
        stick.parent_type = "BONE"
        stick.parent_bone = bone.name

    axes = facing_frame(arm, **SOMA_FACING)
    focus = lambda: (arm.matrix_world @ arm.pose.bones["Hips"].matrix).translation
    picks = render_sheet(scene, focus, axes, start, end, str(motion_dir / "preview.png"))
    print(json.dumps({"kimodo_preview": {"bvh": str(bvh), "sheet": str(motion_dir / "preview.png"), "frames": [start, end], "sheet_frames": picks}}))


# ---------------------------------------------------------------- retarget

def set_world_rotation(arm, pose_bone, rotation, location=None):
    current = arm.matrix_world @ pose_bone.matrix
    # Keeping the current world scale leaves a scaled armature object's bones unscaled in pose space.
    world = Matrix.LocRotScale(location if location is not None else current.translation, rotation, current.to_scale())
    pose_bone.matrix = arm.matrix_world.inverted() @ world
    bpy.context.view_layer.update()


def world_head(arm, name):
    return (arm.matrix_world @ arm.pose.bones[name].matrix).translation.copy()


def untwist(arm, pose_bone):
    """Drop spin about the bone's own axis and return the world rotation that removed it.

    The bone's tail and bend stay put. Descendants apply the returned correction so a
    spin the source inherited through this joint does not survive in the foot.
    """
    before = (arm.matrix_world @ pose_bone.matrix).to_quaternion()
    pose_bone.rotation_quaternion = Quaternion(remove_twist(tuple(pose_bone.rotation_quaternion)))
    bpy.context.view_layer.update()
    return (arm.matrix_world @ pose_bone.matrix).to_quaternion() @ before.inverted()


def nearest_in(bone, table):
    parent = bone.parent
    while parent is not None:
        if parent.name in table:
            return table[parent.name]
        parent = parent.parent
    return None


def max_rotation_step(arm, names, start, end):
    """Largest world rotation change of any named bone between neighbouring frames, in degrees."""
    worst = (0.0, start, "")
    prev = None
    for f in range(start, end + 1):
        bpy.context.scene.frame_set(f)
        cur = {n: (arm.matrix_world @ arm.pose.bones[n].matrix).to_quaternion() for n in names}
        if prev:
            for n, q in cur.items():
                angle = math.degrees(prev[n].rotation_difference(q).angle)
                angle = min(angle, 360.0 - angle)
                if angle > worst[0]:
                    worst = (angle, f, n)
        prev = cur
    return {"degrees": worst[0], "frame": worst[1], "bone": worst[2]}


def run_retarget(args):
    rig_map = json.load(open(args.map, encoding="utf-8"))
    offsets = dict(rig_map.get("offsets", {}))
    offsets.update(json.loads(args.offsets))
    pairs = rig_map["bones"]
    legs = rig_map["legs"]
    hinges = set(rig_map.get("hinges", []))

    scene = reset_scene()
    bpy.ops.import_scene.gltf(filepath=args.character)
    tgt = next(o for o in scene.objects if o.type == "ARMATURE")
    for obj in list(scene.objects):
        if obj.type == "MESH" and obj.parent is None:
            bpy.data.objects.remove(obj)  # unskinned helpers some packs ship next to the rig
    for action in list(bpy.data.actions):
        bpy.data.actions.remove(action)
    # Removing the pack's actions keeps their last evaluated pose; unmapped bones must export at rest.
    for pose_bone in tgt.pose.bones:
        pose_bone.rotation_mode = "QUATERNION"
        pose_bone.location = (0.0, 0.0, 0.0)
        pose_bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        pose_bone.scale = (1.0, 1.0, 1.0)
    bpy.context.view_layer.update()
    missing = [b for b in [t for t, _ in pairs] + list(offsets) + list(hinges) if b not in tgt.data.bones]
    if missing:
        raise SystemExit(f"rig map names bones the character lacks: {missing}")
    if not any(s == "Hips" for _, s in pairs):
        raise SystemExit("rig map must map a target bone to the SOMA Hips, which carries root motion")

    src, start, end = import_bvh(args.bvh)
    scene.frame_start, scene.frame_end = start, end

    src_axes = facing_frame(src, **SOMA_FACING)
    tgt_axes = facing_frame(tgt, rig_map["arms"]["left"], rig_map["arms"]["right"], legs["L"]["foot"], legs["L"]["toe"])
    align = (tgt_axes @ src_axes.transposed()).to_quaternion()

    # BVH rest puts Hips at the origin, so compare hip-to-toe heights, not absolute hip z.
    src_leg = rest_world(src, "Hips").translation.z - rest_world(src, "LeftToeBase").translation.z
    tgt_leg = rest_world(tgt, "Hips").translation.z - rest_world(tgt, legs["L"]["toe"]).translation.z
    scale = tgt_leg / src_leg

    src_rest = {s: rest_world(src, s).to_quaternion() for _, s in pairs}
    tgt_rest = {t: rest_world(tgt, t).to_quaternion() for t, _ in pairs}
    offset_q = {b: Euler([math.radians(v) for v in xyz], "XYZ").to_quaternion() for b, xyz in offsets.items()}

    tgt.animation_data_create()
    action = bpy.data.actions.new(args.clip_name)
    tgt.animation_data.action = action

    for f in range(start, end + 1):
        scene.frame_set(f)
        carried = {}
        for t_name, s_name in pairs:
            spb = src.pose.bones[s_name]
            s_world = src.matrix_world @ spb.matrix
            delta = align @ (s_world.to_quaternion() @ src_rest[s_name].inverted()) @ align.inverted()
            rotation = delta @ tgt_rest[t_name]
            correction = nearest_in(tgt.pose.bones[t_name], carried)
            if correction is not None:
                rotation = correction @ rotation
            if t_name in offset_q:
                rotation = rotation @ offset_q[t_name]
            location = None
            if s_name == "Hips":
                p = s_world.translation
                location = align @ Vector((p.x, p.y, 0.0)) * scale
                location.z = p.z * scale
            tpb = tgt.pose.bones[t_name]
            set_world_rotation(tgt, tpb, rotation, location)
            if t_name in hinges:
                fix = untwist(tgt, tpb)
                carried[t_name] = fix if correction is None else fix @ correction
            tpb.keyframe_insert("rotation_quaternion", frame=f)
            if location is not None:
                tpb.keyframe_insert("location", frame=f)

    bpy.data.objects.remove(src)
    for extra in list(bpy.data.actions):
        if extra != action:
            bpy.data.actions.remove(extra)

    metrics = {"scale": scale, "frames": [start, end], "fps": FPS, "legs": {}}
    fk = {side: [] for side in legs}
    for f in range(start, end + 1):
        scene.frame_set(f)
        for side, leg in legs.items():
            fk[side].append({key: (tgt.matrix_world @ tgt.pose.bones[leg[key]].matrix).copy() for key in ("upper", "lower", "foot")})

    plans = {}
    for side, leg in legs.items():
        ankles = [tuple(m["foot"].translation) for m in fk[side]]
        upper_len = (rest_world(tgt, leg["lower"]).translation - rest_world(tgt, leg["upper"]).translation).length
        lower_len = (rest_world(tgt, leg["foot"]).translation - rest_world(tgt, leg["lower"]).translation).length
        flags = detect_contacts(ankles, FPS, tgt_leg)
        plans[side] = (flags, lock_plan(ankles, flags), upper_len, lower_len)
        metrics["legs"][side] = {"planted_frames": sum(flags), "slide_before_m": foot_slide(ankles, flags)}

    if not args.no_foot_lock:
        for i, f in enumerate(range(start, end + 1)):
            scene.frame_set(f)
            for side, leg in legs.items():
                _, plan, upper_len, lower_len = plans[side]
                weight, anchor = plan[i]
                if anchor is None:
                    continue
                m = fk[side][i]
                hip, knee_fk, ankle_fk = m["upper"].translation, m["lower"].translation, m["foot"].translation
                target = ankle_fk.lerp(Vector((anchor[0], anchor[1], ankle_fk.z)), weight)
                knee, ankle = (Vector(v) for v in two_bone_ik(tuple(hip), tuple(knee_fk), tuple(target), upper_len, lower_len))
                upper_pb, lower_pb, foot_pb = (tgt.pose.bones[leg[k]] for k in ("upper", "lower", "foot"))
                swing = (knee_fk - hip).rotation_difference(knee - hip)
                set_world_rotation(tgt, upper_pb, swing @ m["upper"].to_quaternion())
                lower_now = tgt.matrix_world @ lower_pb.matrix
                swing = (world_head(tgt, leg["foot"]) - lower_now.translation).rotation_difference(ankle - lower_now.translation)
                set_world_rotation(tgt, lower_pb, swing @ lower_now.to_quaternion())
                if leg["lower"] in hinges:
                    untwist(tgt, lower_pb)
                set_world_rotation(tgt, foot_pb, m["foot"].to_quaternion())
                for pb in (upper_pb, lower_pb, foot_pb):
                    pb.keyframe_insert("rotation_quaternion", frame=f)

    for side, leg in legs.items():
        flags, plan = plans[side][:2]
        ankles = []
        pin_error = 0.0
        for i, f in enumerate(range(start, end + 1)):
            scene.frame_set(f)
            ankle = world_head(tgt, leg["foot"])
            ankles.append(tuple(ankle))
            if flags[i] and not args.no_foot_lock:
                pin_error = max(pin_error, math.hypot(ankle.x - plan[i][1][0], ankle.y - plan[i][1][1]))
        metrics["legs"][side]["slide_after_m"] = foot_slide(ankles, flags)
        metrics["legs"][side]["max_pin_error_m"] = pin_error
    metrics["foot_lock"] = not args.no_foot_lock
    metrics["max_rotation_step"] = max_rotation_step(tgt, [t for t, _ in pairs], start, end)

    os.makedirs(args.out_dir, exist_ok=True)
    clip = os.path.join(args.out_dir, "clip.glb")
    bpy.ops.export_scene.gltf(filepath=clip, export_format="GLB", export_animations=True,
                              export_animation_mode="ACTIONS", export_force_sampling=True)
    hips = next(t for t, s in pairs if s == "Hips")
    focus = lambda: world_head(tgt, hips)
    height = max(v.z for obj in scene.objects if obj.type == "MESH" for v in (obj.matrix_world @ Vector(c) for c in obj.bound_box))
    render_sheet(scene, focus, tgt_axes, start, end, os.path.join(args.out_dir, "sheet.png"), height=max(height, 0.5))
    with open(os.path.join(args.out_dir, "metrics.json"), "w", encoding="utf-8") as fh:
        json.dump(metrics, fh, indent=2)
    print(json.dumps({"kimodo_retarget": {"clip": clip, "metrics": metrics}}))


def main():
    args = parse_args()
    if args.mode == "preview":
        run_preview(args)
    else:
        run_retarget(args)


if __name__ == "__main__":
    main()
