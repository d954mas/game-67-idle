#!/usr/bin/env python3
"""Generate compact C mesh data for the KayKit Ozz skinning proof."""

from __future__ import annotations

from pathlib import Path

import bpy
from mathutils import Matrix


ROOT = Path(__file__).resolve().parents[2]
KAYKIT_GLB = ROOT / "tmp/mine-cards/external/kaykit/free_1_1/KayKit_Character_Animations_1.1/Animations/gltf/Rig_Medium/Rig_Medium_Tools.glb"
PICKAXE_GLTF = Path(r"C:\Users\ROG\YandexDisk\gamedev\assets\my\tanki\Cube World - Aug 2023\Tools\glTF\Pickaxe_Stone.gltf")
OUT_HEADER = ROOT / "src/mine_cards_kaykit_mesh.gen.h"


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def import_gltf(path: Path) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    return [obj for obj in bpy.data.objects if obj not in before]


def find_armature(objects: list[bpy.types.Object]) -> bpy.types.Object:
    for obj in objects:
        if obj.type == "ARMATURE":
            return obj
    raise RuntimeError("missing armature")


def color_for_object(name: str) -> tuple[float, float, float, float]:
    low = name.lower()
    if "head" in low:
        return (0.92, 0.63, 0.42, 1.0)
    if "arm" in low:
        return (0.88, 0.58, 0.38, 1.0)
    if "body" in low:
        return (0.15, 0.55, 0.72, 1.0)
    if "leg" in low:
        return (0.13, 0.20, 0.38, 1.0)
    return (0.42, 0.42, 0.42, 1.0)


def mat_col_major(matrix: Matrix) -> list[float]:
    return [float(matrix[row][col]) for col in range(4) for row in range(4)]


def triangulated_indices(poly_vertices: list[int]) -> list[tuple[int, int, int]]:
    if len(poly_vertices) < 3:
        return []
    return [(poly_vertices[0], poly_vertices[i], poly_vertices[i + 1]) for i in range(1, len(poly_vertices) - 1)]


def vertex_influences(obj: bpy.types.Object, vertex: bpy.types.MeshVertex, joint_index_by_name: dict[str, int]) -> list[tuple[int, float]]:
    influences: list[tuple[int, float]] = []
    for group in vertex.groups:
        name = obj.vertex_groups[group.group].name
        joint_index = joint_index_by_name.get(name)
        if joint_index is not None and group.weight > 0.0:
            influences.append((joint_index, float(group.weight)))
    influences.sort(key=lambda item: item[1], reverse=True)
    influences = influences[:4]
    total = sum(weight for _, weight in influences)
    if total <= 0.0:
        influences = [(0, 1.0)]
        total = 1.0
    normalized = [(joint, weight / total) for joint, weight in influences]
    while len(normalized) < 4:
        normalized.append((0, 0.0))
    return normalized


def gather_character(objects: list[bpy.types.Object], armature: bpy.types.Object):
    mesh_objects = [obj for obj in objects if obj.type == "MESH" and not obj.name.startswith("Icosphere")]
    group_names: list[str] = []
    for obj in mesh_objects:
        for group in obj.vertex_groups:
            if group.name not in group_names:
                group_names.append(group.name)

    bone_names = [bone.name for bone in armature.data.bones]
    joint_names: list[str] = []
    for name in bone_names + group_names:
        if name not in joint_names:
            joint_names.append(name)
    joint_index_by_name = {name: i for i, name in enumerate(joint_names)}

    inv_bind = []
    for name in joint_names:
        bone = armature.data.bones.get(name)
        if bone is None:
            inv_bind.append(mat_col_major(Matrix.Identity(4)))
        else:
            inv_bind.append(mat_col_major((armature.matrix_world @ bone.matrix_local).inverted()))

    vertices = []
    indices = []
    for obj in mesh_objects:
        mesh = obj.data
        color = color_for_object(obj.name)
        vertex_base = len(vertices)
        for vertex in mesh.vertices:
            pos = obj.matrix_world @ vertex.co
            influences = vertex_influences(obj, vertex, joint_index_by_name)
            vertices.append(
                {
                    "pos": (float(pos.x), float(pos.y), float(pos.z)),
                    "joints": tuple(joint for joint, _ in influences),
                    "weights": tuple(weight for _, weight in influences),
                    "color": color,
                }
            )
        for poly in mesh.polygons:
            for tri in triangulated_indices(list(poly.vertices)):
                indices.extend(vertex_base + idx for idx in tri)
    return joint_names, inv_bind, vertices, indices


def gather_pickaxe(objects: list[bpy.types.Object]):
    mesh_objects = [obj for obj in objects if obj.type == "MESH"]
    raw_positions = []
    raw_indices = []
    for obj in mesh_objects:
        vertex_base = len(raw_positions)
        for vertex in obj.data.vertices:
            pos = obj.matrix_world @ vertex.co
            raw_positions.append((float(pos.x), float(pos.y), float(pos.z)))
        for poly in obj.data.polygons:
            for tri in triangulated_indices(list(poly.vertices)):
                raw_indices.extend(vertex_base + idx for idx in tri)

    if not raw_positions:
        raise RuntimeError("pickaxe has no vertices")
    mins = [min(p[i] for p in raw_positions) for i in range(3)]
    maxs = [max(p[i] for p in raw_positions) for i in range(3)]
    center = [(mins[i] + maxs[i]) * 0.5 for i in range(3)]
    extent = max(maxs[i] - mins[i] for i in range(3))
    scale = 1.0 / extent if extent > 0.0 else 1.0
    positions = [tuple((p[i] - center[i]) * scale for i in range(3)) for p in raw_positions]
    return positions, raw_indices


def write_float_list(f, values: tuple[float, ...] | list[float]) -> None:
    f.write("{")
    f.write(", ".join(f"{value:.8f}f" for value in values))
    f.write("}")


def main() -> None:
    clear_scene()
    kaykit_objects = import_gltf(KAYKIT_GLB)
    armature = find_armature(kaykit_objects)
    joint_names, inv_bind, vertices, indices = gather_character(kaykit_objects, armature)

    pickaxe_objects = import_gltf(PICKAXE_GLTF)
    pickaxe_vertices, pickaxe_indices = gather_pickaxe(pickaxe_objects)

    OUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    with OUT_HEADER.open("w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by tools/assets/generate_kaykit_skinned_mesh_header.py. */\n")
        f.write("#ifndef MINE_CARDS_KAYKIT_MESH_GEN_H\n#define MINE_CARDS_KAYKIT_MESH_GEN_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("typedef struct MineCardsKayKitSkinnedVertex {\n")
        f.write("    float position[3];\n    uint16_t joints[4];\n    float weights[4];\n    float color[4];\n")
        f.write("} MineCardsKayKitSkinnedVertex;\n\n")
        f.write("typedef struct MineCardsKayKitStaticVertex {\n    float position[3];\n} MineCardsKayKitStaticVertex;\n\n")
        f.write(f"#define MINE_CARDS_KAYKIT_JOINT_COUNT {len(joint_names)}\n")
        f.write(f"#define MINE_CARDS_KAYKIT_VERTEX_COUNT {len(vertices)}\n")
        f.write(f"#define MINE_CARDS_KAYKIT_INDEX_COUNT {len(indices)}\n")
        f.write(f"#define MINE_CARDS_PICKAXE_VERTEX_COUNT {len(pickaxe_vertices)}\n")
        f.write(f"#define MINE_CARDS_PICKAXE_INDEX_COUNT {len(pickaxe_indices)}\n\n")
        f.write("static const char *const k_mine_cards_kaykit_joint_names[MINE_CARDS_KAYKIT_JOINT_COUNT] = {\n")
        for name in joint_names:
            f.write(f'    "{name}",\n')
        f.write("};\n\n")
        f.write("static const float k_mine_cards_kaykit_inverse_bind[MINE_CARDS_KAYKIT_JOINT_COUNT][16] = {\n")
        for matrix in inv_bind:
            f.write("    ")
            write_float_list(f, matrix)
            f.write(",\n")
        f.write("};\n\n")
        f.write("static const MineCardsKayKitSkinnedVertex k_mine_cards_kaykit_vertices[MINE_CARDS_KAYKIT_VERTEX_COUNT] = {\n")
        for vertex in vertices:
            f.write("    {")
            write_float_list(f, vertex["pos"])
            f.write(", {")
            f.write(", ".join(str(joint) for joint in vertex["joints"]))
            f.write("}, ")
            write_float_list(f, vertex["weights"])
            f.write(", ")
            write_float_list(f, vertex["color"])
            f.write("},\n")
        f.write("};\n\n")
        f.write("static const uint32_t k_mine_cards_kaykit_indices[MINE_CARDS_KAYKIT_INDEX_COUNT] = {\n")
        for i in range(0, len(indices), 12):
            f.write("    " + ", ".join(str(v) for v in indices[i : i + 12]) + ",\n")
        f.write("};\n\n")
        f.write("static const MineCardsKayKitStaticVertex k_mine_cards_pickaxe_vertices[MINE_CARDS_PICKAXE_VERTEX_COUNT] = {\n")
        for pos in pickaxe_vertices:
            f.write("    {")
            write_float_list(f, pos)
            f.write("},\n")
        f.write("};\n\n")
        f.write("static const uint32_t k_mine_cards_pickaxe_indices[MINE_CARDS_PICKAXE_INDEX_COUNT] = {\n")
        for i in range(0, len(pickaxe_indices), 12):
            f.write("    " + ", ".join(str(v) for v in pickaxe_indices[i : i + 12]) + ",\n")
        f.write("};\n\n")
        f.write("#endif /* MINE_CARDS_KAYKIT_MESH_GEN_H */\n")

    print(
        f"wrote {OUT_HEADER} with {len(vertices)} skinned vertices, "
        f"{len(indices)} indices, {len(pickaxe_vertices)} pickaxe vertices"
    )


if __name__ == "__main__":
    main()
