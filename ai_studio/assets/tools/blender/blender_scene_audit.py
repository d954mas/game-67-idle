"""Headless Blender scene audit for topology and construction feedback gates.

Run with Blender:
  blender --background scene.blend --python blender_scene_audit.py -- \
    --output audit.json --prefix Hero_ --camera-name CAM_Master --focal-mm 160
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from mesh_metrics import measure_mesh


def _script_args() -> list[str]:
    return sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []


def _args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--prefix", default="")
    parser.add_argument("--collection")
    parser.add_argument("--camera-name")
    parser.add_argument("--focal-mm", type=float)
    parser.add_argument("--shift-y", type=float)
    parser.add_argument("--area-epsilon", type=float, default=1.0e-10)
    parser.add_argument("--max-bevel-policies", type=int, default=3)
    return parser.parse_args(_script_args())


def _scene_hash() -> str:
    path = Path(bpy.data.filepath)
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else ""


def _object_scope(args: argparse.Namespace) -> list[bpy.types.Object]:
    if args.collection:
        collection = bpy.data.collections.get(args.collection)
        if collection is None:
            raise ValueError(f"collection not found: {args.collection}")
        objects = list(collection.all_objects)
    else:
        objects = list(bpy.context.scene.objects)
    return sorted(
        (
            obj
            for obj in objects
            if obj.type == "MESH"
            and not obj.hide_render
            and (not args.prefix or obj.name.startswith(args.prefix))
            and obj.get("audit_topology") != "skip"
        ),
        key=lambda item: item.name,
    )


def _mesh_payload(obj: bpy.types.Object, depsgraph, area_epsilon: float) -> dict:
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh(preserve_all_data_layers=False, depsgraph=depsgraph)
    try:
        matrix = evaluated.matrix_world
        vertices = [tuple(matrix @ vertex.co) for vertex in mesh.vertices]
        polygons = [tuple(polygon.vertices) for polygon in mesh.polygons]
        metrics = measure_mesh(vertices, polygons, area_epsilon=area_epsilon)
        dimensions = tuple(float(value) for value in evaluated.dimensions)
        policy = obj.get("audit_topology", "auto")
        if policy == "auto":
            policy = "volume" if all(value > 1.0e-6 for value in dimensions) else "surface"
        metrics.update({"policy": policy, "dimensions": dimensions})
        return metrics
    finally:
        evaluated.to_mesh_clear()


def _modifier_stages(obj: bpy.types.Object, depsgraph, area_epsilon: float) -> list[dict]:
    modifiers = list(obj.modifiers)
    if not modifiers:
        return []
    original = [(modifier.show_viewport, modifier.show_render) for modifier in modifiers]
    stages = []
    try:
        for stage_index, modifier in enumerate(modifiers):
            for index, candidate in enumerate(modifiers):
                candidate.show_viewport = original[index][0] and index <= stage_index
                candidate.show_render = original[index][1] and index <= stage_index
            depsgraph.update()
            metrics = _mesh_payload(obj, depsgraph, area_epsilon)
            stages.append(
                {
                    "index": stage_index,
                    "name": modifier.name,
                    "type": modifier.type,
                    "metrics": metrics,
                }
            )
    finally:
        for modifier, state in zip(modifiers, original):
            modifier.show_viewport, modifier.show_render = state
        depsgraph.update()
    return stages


def _camera_check(args: argparse.Namespace) -> dict:
    scene = bpy.context.scene
    camera = bpy.data.objects.get(args.camera_name) if args.camera_name else scene.camera
    problems = []
    if camera is None or camera.type != "CAMERA":
        problems.append("camera missing")
    else:
        if args.focal_mm is not None and not math.isclose(camera.data.lens, args.focal_mm, abs_tol=1.0e-4):
            problems.append(f"lens={camera.data.lens}, expected={args.focal_mm}")
        if args.shift_y is not None and not math.isclose(camera.data.shift_y, args.shift_y, abs_tol=1.0e-6):
            problems.append(f"shift_y={camera.data.shift_y}, expected={args.shift_y}")
    return {
        "id": "camera.contract",
        "status": "block" if problems else "pass",
        "evidence": "; ".join(problems) if problems else f"camera={camera.name}",
    }


def _check(check_id: str, count: int, evidence: str) -> dict:
    return {
        "id": check_id,
        "status": "pass" if count == 0 else "block",
        "evidence": evidence,
        "count": count,
    }


def audit(args: argparse.Namespace) -> dict:
    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()
    objects = _object_scope(args)
    records = []
    bevel_policies = set()

    for obj in objects:
        final = _mesh_payload(obj, depsgraph, args.area_epsilon)
        stages = _modifier_stages(obj, depsgraph, args.area_epsilon)
        for modifier in obj.modifiers:
            if modifier.type == "BEVEL":
                bevel_policies.add(
                    (round(float(modifier.width), 6), int(modifier.segments), modifier.limit_method)
                )
        records.append(
            {
                "name": obj.name,
                "final": final,
                "modifier_stages": stages,
                "negative_scale": any(value < 0.0 for value in obj.scale),
            }
        )

    degenerate = sum(item["final"]["degenerate_faces"] for item in records)
    duplicates = sum(item["final"]["duplicate_faces"] for item in records)
    boundary = sum(
        item["final"]["boundary_edges"]
        for item in records
        if item["final"]["policy"] == "volume"
    )
    non_manifold = sum(item["final"]["non_manifold_edges"] for item in records)
    stage_failures = []
    for item in records:
        for stage in item["modifier_stages"]:
            metrics = stage["metrics"]
            if (
                metrics["degenerate_faces"]
                or metrics["duplicate_faces"]
                or metrics["non_manifold_edges"]
                or (metrics["policy"] == "volume" and metrics["boundary_edges"])
            ):
                stage_failures.append(f"{item['name']}:{stage['name']}")

    unit_ok = scene.unit_settings.system == "METRIC" and math.isclose(
        scene.unit_settings.scale_length, 1.0, abs_tol=1.0e-6
    )
    checks = [
        {
            "id": "scene.units",
            "status": "pass" if unit_ok else "block",
            "evidence": f"system={scene.unit_settings.system}, scale={scene.unit_settings.scale_length}",
        },
        _camera_check(args),
        _check("topology.degenerate_faces", degenerate, f"degenerate={degenerate}"),
        _check("topology.non_manifold_edges", non_manifold, f"non_manifold={non_manifold}"),
        _check("topology.boundary_edges", boundary, f"volume_boundary={boundary}"),
        _check("topology.duplicate_faces", duplicates, f"duplicate_faces={duplicates}"),
        _check(
            "modifier.boolean_stages",
            len(stage_failures),
            "clean" if not stage_failures else ", ".join(stage_failures[:40]),
        ),
        {
            "id": "shading.bevel_policy",
            "status": "pass" if len(bevel_policies) <= args.max_bevel_policies else "block",
            "evidence": f"policies={sorted(bevel_policies)}",
            "count": len(bevel_policies),
        },
        {
            "id": "geometry.intersections",
            "status": "review",
            "evidence": "Use depth-three-quarter and internal/wireframe proof; generic overlap is not auto-approved.",
        },
    ]
    status = "block" if any(item["status"] == "block" for item in checks) else "review"
    return {
        "schema_version": 1,
        "status": status,
        "scene": bpy.data.filepath,
        "scene_sha256": _scene_hash(),
        "scope": {"prefix": args.prefix, "collection": args.collection},
        "objects_checked": len(records),
        "checks": checks,
        "objects": records,
        "remediation": {
            "degenerate_faces": "Replace tangent/coplanar cutter joins; audit after every modifier stage.",
            "boundary_edges": "Close volumetric mesh caps or mark intentional planes audit_topology=surface.",
            "boolean_stages": "Fix the first failing stage before adding downstream modifiers.",
            "bevel_policy": "Reduce the scene to two or three named bevel tiers.",
            "intersections": "Render and inspect a depth-three-quarter/wireframe proof; whitelist only intentional overlaps.",
        },
    }


def main() -> int:
    args = _args()
    try:
        report = audit(args)
    except Exception as exc:
        report = {"schema_version": 1, "status": "block", "fatal_error": str(exc)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("BLENDER_SCENE_AUDIT", report.get("status"), args.output)
    return 0 if report.get("status") in {"pass", "review"} else 1


if __name__ == "__main__":
    raise SystemExit(main())
