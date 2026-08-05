"""Fail-closed final gate for Blender review bundles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


PROFILES = {
    "blockout": {
        "technical": {
            "scene.units",
            "camera.contract",
            "topology.degenerate_faces",
            "topology.non_manifold_edges",
            "topology.boundary_edges",
            "geometry.intersections",
        },
        "art": {
            "reference.composition",
            "camera.master_frame",
            "silhouette",
            "scale",
            "completion",
        },
        "renders": {"clay_front", "depth_three_quarter", "master_frame", "silhouette"},
    },
    "lookdev": {
        "technical": {
            "scene.units",
            "camera.contract",
            "topology.degenerate_faces",
            "topology.non_manifold_edges",
            "topology.boundary_edges",
            "modifier.boolean_stages",
            "geometry.intersections",
            "shading.bevel_policy",
        },
        "art": {
            "reference.composition",
            "reference.architecture",
            "style.material_palette",
            "style.detail_hierarchy",
            "camera.master_frame",
            "silhouette",
            "scale",
            "lighting.context",
            "originality",
            "completion",
        },
        "renders": {
            "clay_front",
            "color_front",
            "depth_three_quarter",
            "master_frame",
            "grayscale",
            "silhouette",
        },
    },
    "handoff": {
        "technical": {
            "scene.units",
            "camera.contract",
            "topology.degenerate_faces",
            "topology.non_manifold_edges",
            "topology.boundary_edges",
            "modifier.boolean_stages",
            "geometry.intersections",
            "shading.bevel_policy",
            "scene.naming",
            "scene.transforms",
            "scene.dependencies",
        },
        "art": {
            "reference.composition",
            "reference.architecture",
            "style.material_palette",
            "style.detail_hierarchy",
            "camera.master_frame",
            "silhouette",
            "scale",
            "lighting.context",
            "originality",
            "completion",
        },
        "renders": {
            "clay_front",
            "color_front",
            "depth_three_quarter",
            "master_frame",
            "grayscale",
            "silhouette",
            "wireframe",
        },
    },
}


def _check_map(records: Any) -> dict[str, dict[str, Any]]:
    if not isinstance(records, list):
        return {}
    return {
        str(item.get("id")): item
        for item in records
        if isinstance(item, dict) and item.get("id")
    }


def _open_blocking_findings(records: Any) -> list[str]:
    if not isinstance(records, list):
        return []
    blockers = []
    for item in records:
        if not isinstance(item, dict):
            continue
        if item.get("status") in {"closed", "accepted", "resolved"}:
            continue
        if item.get("severity") in {"critical", "high"}:
            blockers.append(str(item.get("id") or "unnamed"))
    return blockers


def evaluate_bundle(bundle: dict[str, Any]) -> dict[str, Any]:
    """Evaluate a Blender proof bundle without touching the filesystem."""
    blockers: list[str] = []
    reviews: list[str] = []

    if bundle.get("schema_version") != 1:
        blockers.append("schema_version")

    profile_name = bundle.get("profile")
    profile = PROFILES.get(profile_name)
    if profile is None:
        blockers.append("profile")
        profile = {"technical": set(), "art": set(), "renders": set()}

    scene = bundle.get("scene") if isinstance(bundle.get("scene"), dict) else {}
    scene_hash = scene.get("sha256")
    if not isinstance(scene_hash, str) or len(scene_hash) != 64:
        blockers.append("scene.sha256")

    lock = bundle.get("reference_lock") if isinstance(bundle.get("reference_lock"), dict) else {}
    if not lock.get("spec_path"):
        blockers.append("reference_lock.spec_path")
    if not lock.get("approved_references"):
        blockers.append("reference_lock.approved_references")
    if not lock.get("success_criteria"):
        blockers.append("reference_lock.success_criteria")
    if not lock.get("camera"):
        blockers.append("reference_lock.camera")

    technical = bundle.get("technical") if isinstance(bundle.get("technical"), dict) else {}
    if technical.get("status") != "pass":
        blockers.append("technical.status")
    if technical.get("scene_sha256") != scene_hash:
        blockers.append("technical.scene_sha256")
    technical_checks = _check_map(technical.get("checks"))
    for check_id in sorted(profile["technical"]):
        record = technical_checks.get(check_id)
        if record is None:
            blockers.append(f"technical.missing.{check_id}")
        elif record.get("status") != "pass":
            blockers.append(f"technical.{check_id}")
        elif not record.get("evidence"):
            blockers.append(f"technical.evidence.{check_id}")

    art = bundle.get("art_direction") if isinstance(bundle.get("art_direction"), dict) else {}
    if art.get("status") != "pass":
        blockers.append("art_direction.status")
    if art.get("scene_sha256") != scene_hash:
        blockers.append("art_direction.scene_sha256")
    art_checks = _check_map(art.get("checks"))
    for check_id in sorted(profile["art"]):
        record = art_checks.get(check_id)
        if record is None:
            blockers.append(f"art_direction.missing.{check_id}")
        elif record.get("status") != "pass":
            blockers.append(f"art_direction.{check_id}")
        elif not record.get("evidence"):
            blockers.append(f"art_direction.evidence.{check_id}")
    for finding_id in _open_blocking_findings(art.get("mismatches")):
        blockers.append(f"art_direction.mismatch.{finding_id}")

    evidence = bundle.get("evidence") if isinstance(bundle.get("evidence"), dict) else {}
    if evidence.get("scene_sha256") != scene_hash:
        blockers.append("evidence.scene_sha256")
    render_roles = {
        item.get("role")
        for item in evidence.get("renders", [])
        if isinstance(item, dict) and item.get("path")
    }
    for role in sorted(profile["renders"] - render_roles):
        blockers.append(f"evidence.render.{role}")

    independent = (
        bundle.get("independent_review")
        if isinstance(bundle.get("independent_review"), dict)
        else {}
    )
    if independent.get("required"):
        if independent.get("status") != "pass":
            blockers.append("independent_review.status")
        reviewer_roles = set(independent.get("reviewers") or [])
        if not {"visual", "technical"}.issubset(reviewer_roles):
            blockers.append("independent_review.reviewers")
        for finding_id in _open_blocking_findings(independent.get("findings")):
            blockers.append(f"independent_review.finding.{finding_id}")
    else:
        reviews.append("independent_review.not_required")

    for finding_id in _open_blocking_findings(bundle.get("open_findings")):
        blockers.append(f"open_findings.{finding_id}")

    unique_blockers = list(dict.fromkeys(blockers))
    return {
        "status": "block" if unique_blockers else "pass",
        "profile": profile_name,
        "blockers": unique_blockers,
        "reviews": reviews,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    bundle = json.loads(args.bundle.read_text(encoding="utf-8"))
    result = evaluate_bundle(bundle)
    payload = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload + "\n", encoding="utf-8")
    print(payload)
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
