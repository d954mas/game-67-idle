#!/usr/bin/env python3
"""
Build a small Mine Cards skeletal animation probe with Blender.

Run through Blender, for example:

    "C:\\Program Files\\Blender Foundation\\Blender 4.3\\blender.exe" ^
      --background --python tools/assets/build_mine_cards_skeletal_probe.py

The output is intentionally a probe, not final art.
"""

from __future__ import annotations

import json
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "gamedesign/projects/mine-cards/visual/skeletal_spike"
GLB_PATH = OUT_DIR / "minecards_skeletal_miner_probe.glb"
BLEND_PATH = OUT_DIR / "minecards_skeletal_miner_probe.blend"
MANIFEST_PATH = OUT_DIR / "minecards_skeletal_miner_probe_manifest.json"
PREVIEW_PATH = OUT_DIR / "minecards_skeletal_miner_probe_preview.png"


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def make_material(name: str, color: tuple[float, float, float, float]) -> bpy.types.Material:
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    return mat


def add_cube_part(
    name: str,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
    material: bpy.types.Material,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    return obj


def add_bone(
    armature_data: bpy.types.Armature,
    name: str,
    head: tuple[float, float, float],
    tail: tuple[float, float, float],
    parent: bpy.types.EditBone | None = None,
) -> bpy.types.EditBone:
    bone = armature_data.edit_bones.new(name)
    bone.head = head
    bone.tail = tail
    if parent is not None:
        bone.parent = parent
        bone.use_connect = False
    return bone


def create_armature() -> bpy.types.Object:
    armature_data = bpy.data.armatures.new("MineCardsMinerRig")
    armature_obj = bpy.data.objects.new("MineCardsMinerRig", armature_data)
    bpy.context.collection.objects.link(armature_obj)
    bpy.context.view_layer.objects.active = armature_obj
    armature_obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    root = add_bone(armature_data, "root", (0.0, 0.0, 0.0), (0.0, 0.0, 0.4))
    spine = add_bone(armature_data, "spine", (0.0, 0.0, 0.45), (0.0, 0.0, 1.45), root)
    head = add_bone(armature_data, "head", (0.0, 0.0, 1.45), (0.0, 0.0, 2.05), spine)
    add_bone(armature_data, "left_arm", (-0.42, 0.0, 1.25), (-1.0, 0.0, 0.65), spine)
    right_arm = add_bone(armature_data, "right_arm", (0.42, 0.0, 1.25), (1.05, 0.0, 0.58), spine)
    add_bone(armature_data, "pickaxe", (1.05, 0.0, 0.58), (1.45, -0.03, 0.18), right_arm)
    add_bone(armature_data, "left_leg", (-0.22, 0.0, 0.55), (-0.22, 0.0, 0.0), root)
    add_bone(armature_data, "right_leg", (0.22, 0.0, 0.55), (0.22, 0.0, 0.0), root)

    bpy.ops.object.mode_set(mode="OBJECT")
    armature_obj.show_in_front = True
    return armature_obj


def parent_part_to_bone(part: bpy.types.Object, armature_obj: bpy.types.Object, bone_name: str) -> None:
    part.parent = armature_obj
    part.parent_type = "BONE"
    part.parent_bone = bone_name


def add_animation(armature_obj: bpy.types.Object) -> None:
    bpy.context.view_layer.objects.active = armature_obj
    armature_obj.select_set(True)
    bpy.ops.object.mode_set(mode="POSE")
    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = 60
    scene.render.fps = 30

    keyframes = [
        (1, -0.15, 0.0),
        (15, 0.85, -0.18),
        (24, -0.95, 0.12),
        (32, -0.55, 0.02),
        (45, 0.4, -0.08),
        (60, -0.15, 0.0),
    ]

    for frame, arm_rot_y, spine_bob in keyframes:
        scene.frame_set(frame)
        armature_obj.location.z = spine_bob
        armature_obj.keyframe_insert(data_path="location", frame=frame)

        right_arm = armature_obj.pose.bones["right_arm"]
        right_arm.rotation_mode = "XYZ"
        right_arm.rotation_euler = (0.0, arm_rot_y, 0.0)
        right_arm.keyframe_insert(data_path="rotation_euler", frame=frame)

        left_arm = armature_obj.pose.bones["left_arm"]
        left_arm.rotation_mode = "XYZ"
        left_arm.rotation_euler = (0.0, arm_rot_y * 0.35, 0.0)
        left_arm.keyframe_insert(data_path="rotation_euler", frame=frame)

        pickaxe = armature_obj.pose.bones["pickaxe"]
        pickaxe.rotation_mode = "XYZ"
        pickaxe.rotation_euler = (0.0, arm_rot_y * 0.55, 0.0)
        pickaxe.keyframe_insert(data_path="rotation_euler", frame=frame)

        head = armature_obj.pose.bones["head"]
        head.rotation_mode = "XYZ"
        head.rotation_euler = (0.08, -arm_rot_y * 0.12, 0.0)
        head.keyframe_insert(data_path="rotation_euler", frame=frame)

    if armature_obj.animation_data and armature_obj.animation_data.action:
        armature_obj.animation_data.action.name = "mine_swing_loop"

    bpy.ops.object.mode_set(mode="OBJECT")


def setup_camera() -> None:
    bpy.ops.object.light_add(type="AREA", location=(0.0, -4.0, 4.0))
    light = bpy.context.object
    light.name = "soft_mine_light"
    light.data.energy = 500
    light.data.size = 5

    bpy.ops.object.camera_add(location=(0.0, -7.8, 1.25))
    camera = bpy.context.object
    camera.name = "preview_camera"
    direction = Vector((0.0, 0.0, 1.0)) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 28
    bpy.context.scene.camera = camera
    bpy.context.scene.render.resolution_x = 960
    bpy.context.scene.render.resolution_y = 960


def export_glb() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(
        filepath=str(GLB_PATH),
        export_format="GLB",
        export_animations=True,
        export_frame_range=True,
        export_force_sampling=True,
        export_yup=True,
        export_apply=False,
    )


def render_preview() -> None:
    bpy.context.scene.frame_set(24)
    bpy.context.scene.render.filepath = str(PREVIEW_PATH)
    bpy.ops.render.render(write_still=True)


def write_manifest(parts: list[str]) -> None:
    manifest = {
        "schema": "mine_cards.skeletal_probe",
        "status": "generated_probe_not_final_art",
        "blender_version": bpy.app.version_string,
        "outputs": {
            "glb": str(GLB_PATH.relative_to(ROOT)).replace("\\", "/"),
            "blend": str(BLEND_PATH.relative_to(ROOT)).replace("\\", "/"),
            "preview": str(PREVIEW_PATH.relative_to(ROOT)).replace("\\", "/"),
        },
        "rig": {
            "armature": "MineCardsMinerRig",
            "bones": [
                "root",
                "spine",
                "head",
                "left_arm",
                "right_arm",
                "pickaxe",
                "left_leg",
                "right_leg",
            ],
            "clip": "mine_swing_loop",
            "frames": [1, 60],
            "fps": 30,
        },
        "parts": parts,
        "decision_use": "technical probe for Blender/GLB/skeleton/animation pipeline; not runtime final art",
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    clear_scene()

    cloth = make_material("miner_warm_cloth", (0.83, 0.43, 0.20, 1.0))
    skin = make_material("miner_skin", (0.78, 0.58, 0.42, 1.0))
    glove = make_material("miner_dark_glove", (0.13, 0.12, 0.10, 1.0))
    copper = make_material("copper_pickaxe", (0.84, 0.42, 0.18, 1.0))
    steel = make_material("worn_steel", (0.43, 0.44, 0.42, 1.0))
    helmet = make_material("helmet_lamp_yellow", (0.95, 0.74, 0.22, 1.0))

    parts = [
        add_cube_part("miner_body_blockout", (0.0, 0.0, 1.0), (0.78, 0.42, 0.88), cloth),
        add_cube_part("miner_head_blockout", (0.0, 0.0, 1.72), (0.55, 0.50, 0.45), skin),
        add_cube_part("miner_helmet_blockout", (0.0, -0.02, 1.98), (0.68, 0.56, 0.18), helmet),
        add_cube_part("miner_left_arm_blockout", (-0.76, 0.0, 0.92), (0.28, 0.28, 0.78), glove),
        add_cube_part("miner_right_arm_blockout", (0.78, 0.0, 0.88), (0.28, 0.28, 0.82), glove),
        add_cube_part("miner_left_leg_blockout", (-0.24, 0.0, 0.24), (0.26, 0.32, 0.55), glove),
        add_cube_part("miner_right_leg_blockout", (0.24, 0.0, 0.24), (0.26, 0.32, 0.55), glove),
        add_cube_part("pickaxe_handle_blockout", (1.25, 0.0, 0.36), (0.12, 0.12, 0.86), steel),
        add_cube_part("pickaxe_copper_head_blockout", (1.25, 0.0, 0.78), (0.74, 0.14, 0.14), copper),
    ]

    armature_obj = create_armature()

    for part in parts:
        if "head" in part.name or "helmet" in part.name:
            parent_part_to_bone(part, armature_obj, "head")
        elif "left_arm" in part.name:
            parent_part_to_bone(part, armature_obj, "left_arm")
        elif "right_arm" in part.name:
            parent_part_to_bone(part, armature_obj, "right_arm")
        elif "left_leg" in part.name:
            parent_part_to_bone(part, armature_obj, "left_leg")
        elif "right_leg" in part.name:
            parent_part_to_bone(part, armature_obj, "right_leg")
        elif "pickaxe" in part.name:
            parent_part_to_bone(part, armature_obj, "pickaxe")
        else:
            parent_part_to_bone(part, armature_obj, "spine")

    add_animation(armature_obj)
    setup_camera()
    export_glb()
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    render_preview()
    write_manifest([part.name for part in parts])

    print(f"wrote {GLB_PATH}")
    print(f"wrote {BLEND_PATH}")
    print(f"wrote {PREVIEW_PATH}")
    print(f"wrote {MANIFEST_PATH}")


if __name__ == "__main__":
    main()
