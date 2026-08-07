"""Fail-closed final gate for Blender review bundles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


REFERENCE_COMPARISON_DIMENSIONS = {
    "composition_camera",
    "silhouette_mass",
    "architecture_genealogy",
    "scale_proportions",
    "rhythm_exceptions",
    "materials_palette_surface",
    "lighting_context",
    "detail_hierarchy",
    "originality_copy_risk",
    "completion_scope",
}
M03_SELECTED_PEOPLE_SHA256 = "6fdfa96691dd1e924c03b7f0056a4e611146ab4f7275a0ed29361a1a4178f076"


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
            "topology.duplicate_faces",
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
        "reference_comparison_required": True,
    },
    "m02_v004_lookdev": {
        "technical": {
            "scene.units",
            "camera.contract",
            "topology.degenerate_faces",
            "topology.non_manifold_edges",
            "topology.boundary_edges",
            "topology.duplicate_faces",
            "modifier.boolean_stages",
            "geometry.intersections",
            "shading.bevel_policy",
        },
        "art": {
            "reference.composition",
            "reference.architecture",
            "style.material_palette",
            "style.material_surface",
            "style.material_scale",
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
            "color_three_quarter_left",
            "color_three_quarter_right",
            "rear_three_quarter",
            "roof_top",
            "roof_side_left",
            "roof_side_right",
            "roof_section",
            "wireframe_complete_building",
            "material_facade_closeup",
            "material_oblique",
            "material_master_crop",
            "material_master_640_crop",
            "material_swatches",
            "master_640",
        },
        "render_hashes": True,
        "independent_review_required": True,
        "review_lanes": {"visual", "material", "technical"},
        "review_claims": {
            "visual": {
                "roof.side_planes_continuous",
                "roof.eave_ridge_read_intact",
            },
            "material": {
                "surface.not_accidentally_flat",
                "surface.scale_calm",
                "surface.family_separation",
                "surface.visible_at_master",
                "surface.visible_at_640",
                "surface.front_side_consistent",
            },
        },
        "distinct_reviewers": True,
        "review_sources_required": True,
        "quality": {"QART_001", "QASSET_002", "QTECH_001"},
        "reference_comparison_required": True,
    },
    "m03_v001_full_scene": {
        "technical": {
            "scene.units",
            "camera.contract",
            "scene.naming",
            "scene.transforms",
            "scene.dependencies",
            "topology.degenerate_faces",
            "topology.non_manifold_edges",
            "topology.boundary_edges",
            "topology.duplicate_faces",
            "modifier.boolean_stages",
            "geometry.intersections",
            "shading.bevel_policy",
        },
        "art": {
            "reference.composition",
            "reference.architecture",
            "style.material_palette",
            "style.material_surface",
            "style.material_scale",
            "style.detail_hierarchy",
            "camera.master_frame",
            "silhouette",
            "scale",
            "lighting.context",
            "originality",
            "completion",
        },
        "renders": {
            "master_clay",
            "master_color",
            "master_640",
            "master_grayscale",
            "master_silhouette",
            "master_thumbnail",
            "three_quarter_left",
            "three_quarter_right",
            "rear_three_quarter",
            "roof_top",
            "roof_section",
            "side_elevation_left",
            "side_elevation_right",
            "depth_wireframe",
            "rear_left_clay",
            "rear_left_color",
            "rear_left_depth",
            "rear_left_wireframe",
            "rear_left_silhouette",
            "rear_right_clay",
            "rear_right_color",
            "rear_right_depth",
            "rear_right_wireframe",
            "rear_right_silhouette",
            "side_left_far_clay",
            "side_left_far_color",
            "side_left_far_depth",
            "side_left_far_wireframe",
            "side_left_far_silhouette",
            "side_left_near_clay",
            "side_left_near_color",
            "side_left_near_depth",
            "side_left_near_wireframe",
            "side_left_near_silhouette",
            "side_right_far_clay",
            "side_right_far_color",
            "side_right_far_depth",
            "side_right_far_wireframe",
            "side_right_far_silhouette",
            "side_right_near_clay",
            "side_right_near_color",
            "side_right_near_depth",
            "side_right_near_wireframe",
            "side_right_near_silhouette",
            "interface_wall_to_roof",
            "interface_rear_to_side",
            "interface_door_backing",
            "central_r29_nonregression_crop",
            "central_r29_silhouette_compare",
            "eight_entrance_sheet",
            "donor_tram_front_oblique",
            "donor_lever_three_states",
            "donor_people_ab",
            "donor_people_selected_sheet",
            "donor_people_master_crop",
            "donor_ground_contact",
            "tram_rail_contact",
            "lever_pivot_contact",
            "material_facade_closeup",
            "material_oblique",
            "light_ab",
            "crowd_30_60",
            "crowd_280_320",
            "negative_space_overlay",
            "reference_comparison_sheet",
        },
        "claims": {
            "m03.source.exact",
            "m03.scene.camera_and_roots",
            "m03.arch.seven_buildings",
            "m03.arch.eight_residential_entrances",
            "m03.arch.interfaces",
            "m03.donor.hash_and_attributes",
            "m03.donor.skin_sidecar",
            "m03.donor.people_decision",
            "m03.material.target_scale",
            "m03.crowd.range_and_space",
            "m03.review.independent_lanes",
            "m03.evidence.fresh",
        },
        "certificates": {
            "source_lock",
            "donor_import_manifest",
            "people_skin_sidecar",
            "people_donor_decision",
            "composition_contract",
            "crowd_layout_contract",
            "render_manifest",
            "reference_comparison",
            "independent_architecture_review",
            "independent_material_review",
            "independent_technical_donor_review",
        },
        "render_hashes": True,
        "independent_review_required": True,
        "review_lanes": {"architecture_visual", "material", "technical_donor"},
        "distinct_reviewers": True,
        "review_sources_required": True,
        "quality": {"QART_001", "QASSET_001", "QASSET_002", "QTECH_001"},
        "reference_comparison_required": True,
    },
    "handoff": {
        "technical": {
            "scene.units",
            "camera.contract",
            "topology.degenerate_faces",
            "topology.non_manifold_edges",
            "topology.boundary_edges",
            "topology.duplicate_faces",
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
        "reference_comparison_required": True,
    },
    "m02_v003_lookdev": {
        "technical": {
            "scene.units",
            "camera.contract",
            "camera.matrix",
            "scene.naming",
            "scene.transforms",
            "scene.dependencies",
            "scene.scope",
            "topology.degenerate_faces",
            "topology.non_manifold_edges",
            "topology.boundary_edges",
            "topology.duplicate_faces",
            "topology.normals",
            "modifier.boolean_stages",
            "geometry.intersections",
            "geometry.opening_closure",
            "geometry.component_semantics",
            "geometry.interface_contacts",
            "geometry.declared_contacts",
            "geometry.baluster_layout",
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
        "certificates": {
            "reference_lock",
            "raw_audit",
            "technical_report",
            "overlap_candidates",
            "overlap_ledger",
            "render_manifest",
            "branch_manifest",
            "branch_ledger",
        },
        "render_hashes": True,
        "component_manifests_required": True,
        "art_source_required": True,
        "art_observations": True,
        "independent_review_required": True,
        "review_lanes": {"architecture_style", "geometry_topology", "approved_camera_goal", "blind_variability"},
        "distinct_reviewers": True,
        "review_sources_required": True,
        "reference_comparison_required": True,
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
        if item.get("status") in {"pass", "closed", "accepted", "resolved"}:
            continue
        if item.get("severity") in {"critical", "high"}:
            blockers.append(str(item.get("id") or "unnamed"))
    return blockers


def _claim_metrics(claims: dict[str, dict[str, Any]], claim_id: str) -> dict[str, Any]:
    claim = claims.get(claim_id)
    if not isinstance(claim, dict):
        return {}
    metrics = claim.get("metrics")
    return metrics if isinstance(metrics, dict) else {}


def _metric_number(metrics: Any, name: str) -> float | None:
    if not isinstance(metrics, dict):
        return None
    value = metrics.get(name)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


def _nonnegative_count(value: Any) -> int | None:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        return None
    return value


def _validate_m03_claims(claims: dict[str, dict[str, Any]], blockers: list[str]) -> None:
    def metric(claim_id: str, name: str) -> float | None:
        return _metric_number(_claim_metrics(claims, claim_id), name)

    exact = "m03.source.exact"
    if metric(exact, "mismatched_files") != 0:
        blockers.append(f"claims.{exact}.mismatched_files")

    roots = "m03.scene.camera_and_roots"
    for name in ("camera_matrix_max_error", "root_matrix_max_error"):
        value = metric(roots, name)
        if value is None or value > 1e-6:
            blockers.append(f"claims.{roots}.{name}")

    buildings = "m03.arch.seven_buildings"
    if metric(buildings, "building_count") != 7:
        blockers.append(f"claims.{buildings}.building_count")

    entrances = "m03.arch.eight_residential_entrances"
    if metric(entrances, "residential_entrances") != 8:
        blockers.append(f"claims.{entrances}.residential_entrances")
    if metric(entrances, "civic_residential_entrances") != 0:
        blockers.append(f"claims.{entrances}.civic_residential_entrances")

    interfaces = "m03.arch.interfaces"
    for name in ("missing_contacts", "undeclared_intersections"):
        if metric(interfaces, name) != 0:
            blockers.append(f"claims.{interfaces}.{name}")

    donor = "m03.donor.hash_and_attributes"
    for name in ("mismatched_donor_hashes", "attribute_diff_failures"):
        if metric(donor, name) != 0:
            blockers.append(f"claims.{donor}.{name}")

    sidecar = "m03.donor.skin_sidecar"
    sidecar_metrics = _claim_metrics(claims, sidecar)
    if sidecar_metrics.get("selected_donor_sha256") != sidecar_metrics.get("sidecar_donor_sha256"):
        blockers.append(f"claims.{sidecar}.donor_sha256")
    if sidecar_metrics.get("selected_donor_sha256") != M03_SELECTED_PEOPLE_SHA256:
        blockers.append(f"claims.{sidecar}.approved_donor_sha256")
    if sidecar_metrics.get("joint_order_match") is not True:
        blockers.append(f"claims.{sidecar}.joint_order_match")
    weight_error = metric(sidecar, "max_weight_error")
    if weight_error is None or weight_error > 1.0 / 255.0:
        blockers.append(f"claims.{sidecar}.max_weight_error")

    people = "m03.donor.people_decision"
    people_metrics = _claim_metrics(claims, people)
    if people_metrics.get("selected_family") != "B":
        blockers.append(f"claims.{people}.selected_family")
    selected_hash = people_metrics.get("selected_donor_sha256")
    if selected_hash != M03_SELECTED_PEOPLE_SHA256:
        blockers.append(f"claims.{people}.selected_donor_sha256")

    material = "m03.material.target_scale"
    if metric(material, "unresolved_materials") != 0:
        blockers.append(f"claims.{material}.unresolved_materials")

    crowd = "m03.crowd.range_and_space"
    dense_count = metric(crowd, "dense_count")
    if dense_count is None or not 280 <= dense_count <= 320:
        blockers.append(f"claims.{crowd}.dense_count")
    sparse_count = metric(crowd, "sparse_count")
    if sparse_count is None or not 30 <= sparse_count <= 60:
        blockers.append(f"claims.{crowd}.sparse_count")
    if metric(crowd, "clearance_mask_failures") != 0:
        blockers.append(f"claims.{crowd}.clearance_mask_failures")
    ratio_error = metric(crowd, "max_zone_ratio_error")
    if ratio_error is None or ratio_error > 0.10:
        blockers.append(f"claims.{crowd}.max_zone_ratio_error")

    review = "m03.review.independent_lanes"
    if metric(review, "missing_lanes") != 0:
        blockers.append(f"claims.{review}.missing_lanes")

    fresh = "m03.evidence.fresh"
    if metric(fresh, "stale_evidence_count") != 0:
        blockers.append(f"claims.{fresh}.stale_evidence_count")


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
        elif not record.get("evidence") and not (profile.get("art_observations") and record.get("observation")):
            blockers.append(f"art_direction.evidence.{check_id}")
    for finding_id in _open_blocking_findings(art.get("mismatches")):
        blockers.append(f"art_direction.mismatch.{finding_id}")
    if profile.get("art_source_required"):
        source = art.get("source_review") if isinstance(art.get("source_review"), dict) else {}
        if not source.get("path") or not isinstance(source.get("sha256"), str) or len(source.get("sha256", "")) != 64:
            blockers.append("art_direction.source_review")

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
    if profile.get("render_hashes"):
        for item in evidence.get("renders", []):
            if not isinstance(item, dict) or not item.get("role"):
                continue
            sha256 = item.get("sha256")
            if not isinstance(sha256, str) or len(sha256) != 64:
                blockers.append(f"evidence.render.sha256.{item['role']}")
    if profile.get("reference_comparison_required"):
        comparisons = art.get("reference_comparison") if isinstance(art.get("reference_comparison"), list) else []
        comparison_map = {
            item.get("dimension"): item
            for item in comparisons
            if isinstance(item, dict) and item.get("dimension")
        }
        if len(comparison_map) != len(comparisons):
            blockers.append("art_direction.reference_comparison.duplicates")
        for dimension in sorted(REFERENCE_COMPARISON_DIMENSIONS):
            record = comparison_map.get(dimension)
            if record is None:
                blockers.append(f"art_direction.reference_comparison.missing.{dimension}")
                continue
            if record.get("status") != "pass":
                blockers.append(f"art_direction.reference_comparison.{dimension}")
            for field in ("criterion", "reference_observation", "candidate_observation"):
                if not record.get(field):
                    blockers.append(f"art_direction.reference_comparison.{dimension}.{field}")
            roles = set(record.get("evidence_roles") or [])
            if not roles or not roles.issubset(render_roles):
                blockers.append(f"art_direction.reference_comparison.{dimension}.evidence_roles")
            citations = record.get("spec_citations") if isinstance(record.get("spec_citations"), list) else []
            if not citations:
                blockers.append(f"art_direction.reference_comparison.{dimension}.spec_citations")
            for citation in citations:
                if (
                    not isinstance(citation, dict)
                    or not citation.get("path")
                    or not citation.get("section")
                    or not isinstance(citation.get("sha256"), str)
                    or len(citation.get("sha256", "")) != 64
                ):
                    blockers.append(f"art_direction.reference_comparison.{dimension}.spec_citation")
                    break

        known_rejections = lock.get("known_rejections") if isinstance(lock.get("known_rejections"), list) else []
        closures = art.get("rejection_closures") if isinstance(art.get("rejection_closures"), list) else []
        closure_map = {
            item.get("id"): item
            for item in closures
            if isinstance(item, dict) and item.get("id")
        }
        for rejection in known_rejections:
            rejection_id = rejection.get("id") if isinstance(rejection, dict) else None
            if not rejection_id:
                blockers.append("reference_lock.known_rejections")
                continue
            closure = closure_map.get(rejection_id)
            if closure is None:
                blockers.append(f"art_direction.rejection_closure.missing.{rejection_id}")
                continue
            if closure.get("status") != "pass" or not closure.get("candidate_observation"):
                blockers.append(f"art_direction.rejection_closure.{rejection_id}")
            closure_roles = set(closure.get("evidence_roles") or [])
            if not closure_roles or not closure_roles.issubset(render_roles):
                blockers.append(f"art_direction.rejection_closure.{rejection_id}.evidence_roles")
    if profile.get("component_manifests_required"):
        component_manifests = evidence.get("component_manifests")
        if not isinstance(component_manifests, list) or not component_manifests:
            blockers.append("evidence.component_manifests")
        else:
            for index, item in enumerate(component_manifests):
                if (
                    not isinstance(item, dict)
                    or not item.get("path")
                    or not item.get("component")
                    or not isinstance(item.get("sha256"), str)
                    or len(item.get("sha256", "")) != 64
                    or item.get("scene_sha256") != scene_hash
                ):
                    blockers.append(f"evidence.component_manifests.{index}")

    required_certificates = profile.get("certificates", set())
    certificates = (
        bundle.get("certificates")
        if isinstance(bundle.get("certificates"), dict)
        else {}
    )
    for name in sorted(required_certificates):
        certificate = certificates.get(name)
        if not isinstance(certificate, dict):
            blockers.append(f"certificates.{name}")
            continue
        if not certificate.get("path"):
            blockers.append(f"certificates.{name}.path")
        sha256 = certificate.get("sha256")
        if not isinstance(sha256, str) or len(sha256) != 64:
            blockers.append(f"certificates.{name}.sha256")
        if certificate.get("scene_sha256") != scene_hash:
            blockers.append(f"certificates.{name}.scene_sha256")

    claims = _check_map(bundle.get("claims"))
    for claim_id in sorted(profile.get("claims", set())):
        claim = claims.get(claim_id)
        if claim is None:
            blockers.append(f"claims.missing.{claim_id}")
        elif claim.get("status") != "pass":
            blockers.append(f"claims.{claim_id}")
        elif not claim.get("evidence"):
            blockers.append(f"claims.{claim_id}.evidence")
        elif claim.get("scene_sha256") != scene_hash:
            blockers.append(f"claims.{claim_id}.scene_sha256")
    if profile_name == "m03_v001_full_scene":
        _validate_m03_claims(claims, blockers)

    independent = (
        bundle.get("independent_review")
        if isinstance(bundle.get("independent_review"), dict)
        else {}
    )
    if profile.get("independent_review_required") and not independent.get("required"):
        blockers.append("independent_review.required")
    if independent.get("required"):
        if independent.get("status") != "pass":
            blockers.append("independent_review.status")
        required_lanes = profile.get("review_lanes")
        if required_lanes:
            details = independent.get("reviewer_details") if isinstance(independent.get("reviewer_details"), list) else []
            lanes = {item.get("lane") for item in details if isinstance(item, dict)}
            if lanes != required_lanes or len(details) != len(required_lanes):
                blockers.append("independent_review.lanes")
            reviewers = [item.get("reviewer") for item in details if isinstance(item, dict) and item.get("reviewer")]
            if profile.get("distinct_reviewers") and len(set(reviewers)) != len(required_lanes):
                blockers.append("independent_review.distinct_reviewers")
            for item in details:
                if not isinstance(item, dict):
                    blockers.append("independent_review.detail")
                    continue
                lane = item.get("lane", "unknown")
                evidence_items = item.get("evidence") if isinstance(item.get("evidence"), list) else []
                critical_count = _nonnegative_count(item.get("critical", 0))
                high_count = _nonnegative_count(item.get("high", 0))
                if (
                    item.get("verdict") != "pass"
                    or item.get("scene_sha256") != scene_hash
                    or critical_count is None
                    or high_count is None
                    or critical_count > 0
                    or high_count > 0
                    or not evidence_items
                ):
                    blockers.append(f"independent_review.detail.{lane}")
            for lane, required_claims in profile.get("review_claims", {}).items():
                detail = next(
                    (item for item in details if isinstance(item, dict) and item.get("lane") == lane),
                    {},
                )
                claims = set(detail.get("claims") or [])
                for claim in sorted(required_claims - claims):
                    blockers.append(f"independent_review.claim.{lane}.{claim}")
            if profile.get("review_sources_required"):
                sources = independent.get("source_reviews") if isinstance(independent.get("source_reviews"), list) else []
                valid_sources = [
                    item for item in sources
                    if isinstance(item, dict)
                    and item.get("path")
                    and isinstance(item.get("sha256"), str)
                    and len(item.get("sha256", "")) == 64
                ]
                if len(valid_sources) != len(required_lanes):
                    blockers.append("independent_review.source_reviews")
        else:
            reviewer_roles = set(independent.get("reviewers") or [])
            required_reviewer_roles = profile.get("reviewer_roles", {"visual", "technical"})
            if not required_reviewer_roles.issubset(reviewer_roles):
                blockers.append("independent_review.reviewers")
        for finding_id in _open_blocking_findings(independent.get("findings")):
            blockers.append(f"independent_review.finding.{finding_id}")
    else:
        reviews.append("independent_review.not_required")

    quality = bundle.get("quality") if isinstance(bundle.get("quality"), dict) else {}
    for rule_id in sorted(profile.get("quality", set())):
        record = quality.get(rule_id) if isinstance(quality.get(rule_id), dict) else {}
        if record.get("status") != "pass":
            blockers.append(f"quality.{rule_id}")
        elif not record.get("evidence"):
            blockers.append(f"quality.evidence.{rule_id}")

    for finding_id in _open_blocking_findings(bundle.get("open_findings")):
        blockers.append(f"open_findings.{finding_id}")

    unique_blockers = list(dict.fromkeys(blockers))
    return {
        "status": "block" if unique_blockers else "pass",
        "profile": profile_name,
        "scene_sha256": scene_hash,
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
