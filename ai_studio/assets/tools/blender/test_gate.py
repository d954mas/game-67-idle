import unittest

from ai_studio.assets.tools.blender.gate import M03_SELECTED_PEOPLE_SHA256, PROFILES, REFERENCE_COMPARISON_DIMENSIONS, evaluate_bundle


SCENE_HASH = "a" * 64


def passing_bundle():
    technical_ids = (
        "scene.units",
        "camera.contract",
        "topology.degenerate_faces",
        "topology.non_manifold_edges",
        "topology.boundary_edges",
        "topology.duplicate_faces",
        "modifier.boolean_stages",
        "geometry.intersections",
        "shading.bevel_policy",
    )
    art_ids = (
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
    )
    roles = (
        "clay_front",
        "color_front",
        "depth_three_quarter",
        "master_frame",
        "grayscale",
        "silhouette",
    )
    return {
        "schema_version": 1,
        "profile": "lookdev",
        "scene": {"path": "scene.blend", "sha256": SCENE_HASH},
        "reference_lock": {
            "spec_path": "SPEC.md",
            "approved_references": ["approved.png"],
            "success_criteria": ["Central axis reads", "No visible intersections"],
            "camera": {"name": "CAM_Master", "focal_mm": 160.0},
        },
        "technical": {
            "status": "pass",
            "scene_sha256": SCENE_HASH,
            "checks": [
                {"id": check_id, "status": "pass", "evidence": "measured"}
                for check_id in technical_ids
            ],
        },
        "art_direction": {
            "status": "pass",
            "scene_sha256": SCENE_HASH,
            "checks": [
                {"id": check_id, "status": "pass", "evidence": ["review.png"]}
                for check_id in art_ids
            ],
            "mismatches": [],
            "reference_comparison": [
                {
                    "dimension": dimension,
                    "status": "pass",
                    "criterion": f"criterion for {dimension}",
                    "reference_observation": f"observed reference {dimension}",
                    "candidate_observation": f"observed candidate {dimension}",
                    "evidence_roles": ["master_frame"],
                    "spec_citations": [
                        {
                            "path": "SPEC.md",
                            "sha256": "9" * 64,
                            "section": dimension,
                        }
                    ],
                }
                for dimension in sorted(REFERENCE_COMPARISON_DIMENSIONS)
            ],
            "rejection_closures": [],
        },
        "evidence": {
            "scene_sha256": SCENE_HASH,
            "renders": [{"role": role, "path": f"{role}.png"} for role in roles],
        },
        "independent_review": {
            "required": True,
            "status": "pass",
            "reviewers": ["visual", "technical"],
            "findings": [],
        },
        "open_findings": [],
    }


def passing_m02_v003_bundle():
    bundle = passing_bundle()
    bundle["profile"] = "m02_v003_lookdev"
    bundle["technical"]["checks"].extend(
        {"id": check_id, "status": "pass", "evidence": "measured"}
        for check_id in (
            "scene.naming",
            "scene.transforms",
            "scene.dependencies",
            "scene.scope",
            "camera.matrix",
            "topology.normals",
            "geometry.opening_closure",
            "geometry.component_semantics",
            "geometry.interface_contacts",
            "geometry.declared_contacts",
            "geometry.baluster_layout",
        )
    )
    bundle["certificates"] = {
        name: {
            "path": f"{name}.json",
            "sha256": "b" * 64,
            "scene_sha256": SCENE_HASH,
        }
        for name in (
            "reference_lock",
            "raw_audit",
            "technical_report",
            "overlap_candidates",
            "overlap_ledger",
            "render_manifest",
            "branch_manifest",
            "branch_ledger",
        )
    }
    bundle["evidence"]["renders"] = [
        {"role": item["role"], "path": item["path"], "sha256": "c" * 64}
        for item in bundle["evidence"]["renders"]
    ]
    bundle["independent_review"]["reviewers"] = ["visual", "technical", "variant", "goal"]
    bundle["independent_review"]["reviewer_details"] = [
        {
            "lane": lane,
            "reviewer": f"agent-{index}",
            "verdict": "pass",
            "critical": 0,
            "high": 0,
            "scene_sha256": SCENE_HASH,
            "evidence": [{"path": f"review-{index}.png", "sha256": "d" * 64}],
        }
        for index, lane in enumerate(("architecture_style", "geometry_topology", "approved_camera_goal", "blind_variability"))
    ]
    bundle["independent_review"]["source_reviews"] = [
        {"path": f"source-{index}.json", "sha256": "e" * 64}
        for index in range(4)
    ]
    bundle["art_direction"]["source_review"] = {"path": "art-audit.json", "sha256": "f" * 64}
    bundle["evidence"]["component_manifests"] = [
        {"path": "C03/component_manifest.json", "sha256": "1" * 64, "scene_sha256": SCENE_HASH, "component": "C03"}
    ]
    return bundle


def passing_m02_v004_bundle():
    bundle = passing_bundle()
    bundle["profile"] = "m02_v004_lookdev"
    bundle["art_direction"]["checks"].extend(
        {"id": check_id, "status": "pass", "evidence": ["material_facade_closeup.png", "master_640.png"]}
        for check_id in ("style.material_surface", "style.material_scale")
    )
    present = {item["role"] for item in bundle["evidence"]["renders"]}
    for role in (
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
    ):
        if role not in present:
            bundle["evidence"]["renders"].append({"role": role, "path": f"{role}.png"})
    for item in bundle["evidence"]["renders"]:
        item["sha256"] = "c" * 64
    bundle["independent_review"] = {
        "required": True,
        "status": "pass",
        "reviewer_details": [
            {
                "lane": lane,
                "reviewer": f"agent-{lane}",
                "verdict": "pass",
                "critical": 0,
                "high": 0,
                "scene_sha256": SCENE_HASH,
                "evidence": [{"path": f"{lane}.png", "sha256": "d" * 64}],
            }
            for lane in ("visual", "material", "technical")
        ],
        "source_reviews": [
            {"path": f"{lane}.json", "sha256": "e" * 64}
            for lane in ("visual", "material", "technical")
        ],
        "findings": [],
    }
    bundle["independent_review"]["reviewer_details"][1]["claims"] = [
        "surface.not_accidentally_flat",
        "surface.scale_calm",
        "surface.family_separation",
        "surface.visible_at_master",
        "surface.visible_at_640",
        "surface.front_side_consistent",
    ]
    bundle["independent_review"]["reviewer_details"][0]["claims"] = [
        "roof.side_planes_continuous",
        "roof.eave_ridge_read_intact",
    ]
    bundle["quality"] = {
        rule_id: {"status": "pass", "evidence": ["review.json"]}
        for rule_id in ("QART_001", "QASSET_002", "QTECH_001")
    }
    return bundle


def passing_m03_v001_bundle():
    bundle = passing_m02_v004_bundle()
    bundle["profile"] = "m03_v001_full_scene"
    bundle["technical"]["checks"].extend(
        {"id": check_id, "status": "pass", "evidence": "measured"}
        for check_id in ("scene.naming", "scene.transforms", "scene.dependencies")
    )
    present_roles = {item["role"] for item in bundle["evidence"]["renders"]}
    for role in PROFILES["m03_v001_full_scene"]["renders"]:
        if role not in present_roles:
            bundle["evidence"]["renders"].append(
                {"role": role, "path": f"{role}.png", "sha256": "c" * 64}
            )
    bundle["certificates"] = {
        name: {
            "path": f"{name}.json",
            "sha256": "b" * 64,
            "scene_sha256": SCENE_HASH,
        }
        for name in PROFILES["m03_v001_full_scene"]["certificates"]
    }
    bundle["claims"] = [
        {
            "id": "m03.source.exact",
            "status": "pass",
            "evidence": ["source_lock.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"mismatched_files": 0},
        },
        {
            "id": "m03.scene.camera_and_roots",
            "status": "pass",
            "evidence": ["composition_contract.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"camera_matrix_max_error": 0.0, "root_matrix_max_error": 0.0},
        },
        {
            "id": "m03.arch.seven_buildings",
            "status": "pass",
            "evidence": ["architecture_audit.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"building_count": 7},
        },
        {
            "id": "m03.arch.eight_residential_entrances",
            "status": "pass",
            "evidence": ["entrance_census.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"residential_entrances": 8, "civic_residential_entrances": 0},
        },
        {
            "id": "m03.arch.interfaces",
            "status": "pass",
            "evidence": ["contact_audit.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"missing_contacts": 0, "undeclared_intersections": 0},
        },
        {
            "id": "m03.donor.hash_and_attributes",
            "status": "pass",
            "evidence": ["donor_import_manifest.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"mismatched_donor_hashes": 0, "attribute_diff_failures": 0},
        },
        {
            "id": "m03.donor.skin_sidecar",
            "status": "pass",
            "evidence": ["people_skin_sidecar.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {
                "selected_donor_sha256": M03_SELECTED_PEOPLE_SHA256,
                "sidecar_donor_sha256": M03_SELECTED_PEOPLE_SHA256,
                "joint_order_match": True,
                "max_weight_error": 0.0,
            },
        },
        {
            "id": "m03.donor.people_decision",
            "status": "pass",
            "evidence": ["selection.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"selected_family": "B", "selected_donor_sha256": M03_SELECTED_PEOPLE_SHA256},
        },
        {
            "id": "m03.material.target_scale",
            "status": "pass",
            "evidence": ["material_target_scale.png"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"unresolved_materials": 0},
        },
        {
            "id": "m03.crowd.range_and_space",
            "status": "pass",
            "evidence": ["crowd_layout_contract.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {
                "dense_count": 300,
                "sparse_count": 45,
                "clearance_mask_failures": 0,
                "max_zone_ratio_error": 0.0,
            },
        },
        {
            "id": "m03.review.independent_lanes",
            "status": "pass",
            "evidence": ["independent_review.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"missing_lanes": 0},
        },
        {
            "id": "m03.evidence.fresh",
            "status": "pass",
            "evidence": ["render_manifest.json"],
            "scene_sha256": SCENE_HASH,
            "metrics": {"stale_evidence_count": 0},
        },
    ]
    bundle["independent_review"] = {
        "required": True,
        "status": "pass",
        "reviewer_details": [
            {
                "lane": lane,
                "reviewer": f"agent-{lane}",
                "verdict": "pass",
                "critical": 0,
                "high": 0,
                "scene_sha256": SCENE_HASH,
                "evidence": [{"path": f"{lane}.json", "sha256": "d" * 64}],
            }
            for lane in ("architecture_visual", "material", "technical_donor")
        ],
        "source_reviews": [
            {"path": f"{lane}.json", "sha256": "e" * 64}
            for lane in ("architecture_visual", "material", "technical_donor")
        ],
        "findings": [],
    }
    bundle["quality"] = {
        rule_id: {"status": "pass", "evidence": ["review.json"]}
        for rule_id in ("QART_001", "QASSET_001", "QASSET_002", "QTECH_001")
    }
    return bundle


class GateTests(unittest.TestCase):
    def test_complete_lookdev_bundle_passes(self):
        result = evaluate_bundle(passing_bundle())
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["blockers"], [])

    def test_lookdev_requires_full_reference_comparison_matrix(self):
        bundle = passing_bundle()
        bundle["art_direction"]["reference_comparison"] = [
            item
            for item in bundle["art_direction"]["reference_comparison"]
            if item["dimension"] != "materials_palette_surface"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn(
            "art_direction.reference_comparison.missing.materials_palette_surface",
            result["blockers"],
        )

    def test_reference_comparison_requires_paired_observations(self):
        bundle = passing_bundle()
        record = bundle["art_direction"]["reference_comparison"][0]
        record["candidate_observation"] = ""
        result = evaluate_bundle(bundle)
        self.assertIn(
            f"art_direction.reference_comparison.{record['dimension']}.candidate_observation",
            result["blockers"],
        )

    def test_known_user_rejection_requires_exact_scene_closure(self):
        bundle = passing_bundle()
        bundle["reference_lock"]["known_rejections"] = [
            {"id": "surface.disappears_at_target", "source": "lead review"}
        ]
        result = evaluate_bundle(bundle)
        self.assertIn(
            "art_direction.rejection_closure.missing.surface.disappears_at_target",
            result["blockers"],
        )

    def test_missing_reference_lock_blocks(self):
        bundle = passing_bundle()
        bundle["reference_lock"]["approved_references"] = []
        result = evaluate_bundle(bundle)
        self.assertIn("reference_lock.approved_references", result["blockers"])

    def test_technical_failure_blocks_even_when_renders_exist(self):
        bundle = passing_bundle()
        bundle["technical"]["checks"][2]["status"] = "block"
        result = evaluate_bundle(bundle)
        self.assertIn("technical.topology.degenerate_faces", result["blockers"])

    def test_open_style_mismatch_blocks(self):
        bundle = passing_bundle()
        bundle["art_direction"]["mismatches"] = [
            {"id": "architecture.generic", "severity": "high", "status": "open"}
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("art_direction.mismatch.architecture.generic", result["blockers"])

    def test_stale_evidence_hash_blocks(self):
        bundle = passing_bundle()
        bundle["evidence"]["scene_sha256"] = "b" * 64
        result = evaluate_bundle(bundle)
        self.assertIn("evidence.scene_sha256", result["blockers"])

    def test_required_independent_review_cannot_be_self_declared_missing(self):
        bundle = passing_bundle()
        bundle["independent_review"]["reviewers"] = []
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.reviewers", result["blockers"])

    def test_independent_gate_requires_visual_and_technical_reviewers(self):
        bundle = passing_bundle()
        bundle["independent_review"]["reviewers"] = ["builder"]
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.reviewers", result["blockers"])

    def test_known_high_finding_blocks_completion(self):
        bundle = passing_bundle()
        bundle["open_findings"] = [
            {"id": "roof.intersection", "severity": "high", "status": "open"}
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("open_findings.roof.intersection", result["blockers"])

    def test_complete_m02_v003_bundle_passes(self):
        result = evaluate_bundle(passing_m02_v003_bundle())
        self.assertEqual(result["status"], "pass")

    def test_complete_m02_v004_bundle_passes(self):
        result = evaluate_bundle(passing_m02_v004_bundle())
        self.assertEqual(result["status"], "pass")

    def test_m02_v004_requires_visible_material_surface_checks(self):
        bundle = passing_m02_v004_bundle()
        bundle["art_direction"]["checks"] = [
            item for item in bundle["art_direction"]["checks"] if item["id"] != "style.material_surface"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("art_direction.missing.style.material_surface", result["blockers"])

    def test_m02_v004_requires_material_closeup_evidence(self):
        bundle = passing_m02_v004_bundle()
        bundle["evidence"]["renders"] = [
            item for item in bundle["evidence"]["renders"] if item["role"] != "material_facade_closeup"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("evidence.render.material_facade_closeup", result["blockers"])

    def test_m02_v004_requires_dedicated_material_review_lane(self):
        bundle = passing_m02_v004_bundle()
        bundle["independent_review"]["reviewer_details"] = [
            item for item in bundle["independent_review"]["reviewer_details"] if item["lane"] != "material"
        ]
        bundle["independent_review"]["source_reviews"] = bundle["independent_review"]["source_reviews"][:2]
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.lanes", result["blockers"])

    def test_m02_v004_material_reviewer_must_explicitly_reject_flatness(self):
        bundle = passing_m02_v004_bundle()
        material = next(item for item in bundle["independent_review"]["reviewer_details"] if item["lane"] == "material")
        material["claims"].remove("surface.not_accidentally_flat")
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.claim.material.surface.not_accidentally_flat", result["blockers"])

    def test_m02_v004_material_reviewer_must_see_target_camera_surface(self):
        bundle = passing_m02_v004_bundle()
        material = next(item for item in bundle["independent_review"]["reviewer_details"] if item["lane"] == "material")
        material["claims"].remove("surface.visible_at_master")
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.claim.material.surface.visible_at_master", result["blockers"])

    def test_m02_v004_visual_reviewer_must_resolve_side_roof_read(self):
        bundle = passing_m02_v004_bundle()
        visual = next(item for item in bundle["independent_review"]["reviewer_details"] if item["lane"] == "visual")
        visual["claims"].remove("roof.side_planes_continuous")
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.claim.visual.roof.side_planes_continuous", result["blockers"])

    def test_m02_v004_requires_dedicated_side_roof_evidence(self):
        bundle = passing_m02_v004_bundle()
        bundle["evidence"]["renders"] = [
            item for item in bundle["evidence"]["renders"] if item["role"] != "roof_side_right"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("evidence.render.roof_side_right", result["blockers"])

    def test_m02_v004_requires_target_scale_material_crop(self):
        bundle = passing_m02_v004_bundle()
        bundle["evidence"]["renders"] = [
            item for item in bundle["evidence"]["renders"] if item["role"] != "material_master_640_crop"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("evidence.render.material_master_640_crop", result["blockers"])

    def test_m02_v004_requires_passing_material_quality_outcome(self):
        bundle = passing_m02_v004_bundle()
        bundle["quality"]["QASSET_002"]["status"] = "block"
        result = evaluate_bundle(bundle)
        self.assertIn("quality.QASSET_002", result["blockers"])

    def test_m02_v004_requires_real_review_lanes(self):
        bundle = passing_m02_v004_bundle()
        bundle["independent_review"]["reviewer_details"] = []
        bundle["independent_review"]["source_reviews"] = []
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.lanes", result["blockers"])
        self.assertIn("independent_review.source_reviews", result["blockers"])

    def test_m02_v004_requires_hashed_oblique_roof_evidence(self):
        bundle = passing_m02_v004_bundle()
        bundle["evidence"]["renders"] = [
            item for item in bundle["evidence"]["renders"] if item["role"] != "roof_top"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("evidence.render.roof_top", result["blockers"])

    def test_m02_v003_requires_signed_volume_normals_check(self):
        bundle = passing_m02_v003_bundle()
        bundle["technical"]["checks"] = [
            item for item in bundle["technical"]["checks"] if item["id"] != "topology.normals"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("technical.missing.topology.normals", result["blockers"])

    def test_m02_v003_requires_sealed_opening_closure_check(self):
        bundle = passing_m02_v003_bundle()
        bundle["technical"]["checks"] = [
            item for item in bundle["technical"]["checks"] if item["id"] != "geometry.opening_closure"
        ]
        result = evaluate_bundle(bundle)
        self.assertIn("technical.missing.geometry.opening_closure", result["blockers"])

    def test_m02_v003_requires_each_hash_bound_certificate(self):
        for certificate in tuple(passing_m02_v003_bundle()["certificates"]):
            bundle = passing_m02_v003_bundle()
            del bundle["certificates"][certificate]
            result = evaluate_bundle(bundle)
            self.assertIn(f"certificates.{certificate}", result["blockers"])

    def test_m02_v003_rejects_stale_certificate(self):
        bundle = passing_m02_v003_bundle()
        bundle["certificates"]["raw_audit"]["scene_sha256"] = "d" * 64
        result = evaluate_bundle(bundle)
        self.assertIn("certificates.raw_audit.scene_sha256", result["blockers"])

    def test_m02_v003_requires_render_hashes(self):
        bundle = passing_m02_v003_bundle()
        del bundle["evidence"]["renders"][0]["sha256"]
        result = evaluate_bundle(bundle)
        self.assertIn("evidence.render.sha256.clay_front", result["blockers"])

    def test_m02_v003_cannot_disable_independent_review(self):
        bundle = passing_m02_v003_bundle()
        bundle["independent_review"] = {"required": False, "status": "missing", "reviewers": []}
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.required", result["blockers"])

    def test_m02_v003_requires_all_four_packet_review_lanes(self):
        bundle = passing_m02_v003_bundle()
        bundle["independent_review"]["reviewer_details"] = bundle["independent_review"]["reviewer_details"][:2]
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.lanes", result["blockers"])

    def test_m02_v003_rejects_role_names_without_distinct_review_artifacts(self):
        bundle = passing_m02_v003_bundle()
        bundle["independent_review"]["reviewer_details"] = []
        bundle["independent_review"]["source_reviews"] = []
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.lanes", result["blockers"])
        self.assertIn("independent_review.source_reviews", result["blockers"])

    def test_m02_v003_rejects_same_reviewer_in_multiple_lanes(self):
        bundle = passing_m02_v003_bundle()
        for detail in bundle["independent_review"]["reviewer_details"]:
            detail["reviewer"] = "same-agent"
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.distinct_reviewers", result["blockers"])

    def test_m02_v003_requires_component_manifests_and_art_source(self):
        bundle = passing_m02_v003_bundle()
        bundle["evidence"]["component_manifests"] = []
        bundle["art_direction"].pop("source_review")
        result = evaluate_bundle(bundle)
        self.assertIn("evidence.component_manifests", result["blockers"])
        self.assertIn("art_direction.source_review", result["blockers"])

    def test_complete_m03_v001_bundle_passes(self):
        result = evaluate_bundle(passing_m03_v001_bundle())
        self.assertEqual(result["status"], "pass")

    def test_m03_v001_negative_fixtures_fail_closed(self):
        fixtures = {
            "m03.reject_six_buildings": (
                "m03.arch.seven_buildings",
                "building_count",
                6,
                "claims.m03.arch.seven_buildings.building_count",
            ),
            "m03.reject_nine_entrances": (
                "m03.arch.eight_residential_entrances",
                "residential_entrances",
                9,
                "claims.m03.arch.eight_residential_entrances.residential_entrances",
            ),
            "m03.reject_civic_portal_as_residential": (
                "m03.arch.eight_residential_entrances",
                "civic_residential_entrances",
                1,
                "claims.m03.arch.eight_residential_entrances.civic_residential_entrances",
            ),
            "m03.reject_crowd_out_of_range": (
                "m03.crowd.range_and_space",
                "dense_count",
                279,
                "claims.m03.crowd.range_and_space.dense_count",
            ),
            "m03.reject_stale_donor_hash": (
                "m03.donor.hash_and_attributes",
                "mismatched_donor_hashes",
                1,
                "claims.m03.donor.hash_and_attributes.mismatched_donor_hashes",
            ),
            "m03.reject_skin_sidecar_mismatch": (
                "m03.donor.skin_sidecar",
                "sidecar_donor_sha256",
                "7" * 64,
                "claims.m03.donor.skin_sidecar.donor_sha256",
            ),
            "m03.reject_root_transform_drift": (
                "m03.scene.camera_and_roots",
                "root_matrix_max_error",
                0.000002,
                "claims.m03.scene.camera_and_roots.root_matrix_max_error",
            ),
            "m03.reject_missing_contact": (
                "m03.arch.interfaces",
                "missing_contacts",
                1,
                "claims.m03.arch.interfaces.missing_contacts",
            ),
            "m03.reject_stale_scene_evidence": (
                "m03.evidence.fresh",
                "stale_evidence_count",
                1,
                "claims.m03.evidence.fresh.stale_evidence_count",
            ),
        }
        for fixture_id, (claim_id, metric, value, blocker) in fixtures.items():
            with self.subTest(fixture=fixture_id):
                bundle = passing_m03_v001_bundle()
                claim = next(item for item in bundle["claims"] if item["id"] == claim_id)
                claim["metrics"][metric] = value
                result = evaluate_bundle(bundle)
                self.assertIn(blocker, result["blockers"])

    def test_m03_reject_duplicate_reviewer(self):
        bundle = passing_m03_v001_bundle()
        for detail in bundle["independent_review"]["reviewer_details"]:
            detail["reviewer"] = "same-agent"
        result = evaluate_bundle(bundle)
        self.assertIn("independent_review.distinct_reviewers", result["blockers"])

    def test_m03_rejects_jointly_substituted_people_hash(self):
        bundle = passing_m03_v001_bundle()
        sidecar = next(item for item in bundle["claims"] if item["id"] == "m03.donor.skin_sidecar")
        decision = next(item for item in bundle["claims"] if item["id"] == "m03.donor.people_decision")
        sidecar["metrics"]["selected_donor_sha256"] = "7" * 64
        sidecar["metrics"]["sidecar_donor_sha256"] = "7" * 64
        decision["metrics"]["selected_donor_sha256"] = "7" * 64
        result = evaluate_bundle(bundle)
        self.assertIn("claims.m03.donor.skin_sidecar.approved_donor_sha256", result["blockers"])
        self.assertIn("claims.m03.donor.people_decision.selected_donor_sha256", result["blockers"])

    def test_m03_malformed_metrics_fail_closed(self):
        bundle = passing_m03_v001_bundle()
        claim = next(item for item in bundle["claims"] if item["id"] == "m03.arch.seven_buildings")
        claim["metrics"] = None
        result = evaluate_bundle(bundle)
        self.assertIn("claims.m03.arch.seven_buildings.building_count", result["blockers"])

    def test_m03_malformed_reviewer_counts_fail_closed(self):
        bundle = passing_m03_v001_bundle()
        detail = bundle["independent_review"]["reviewer_details"][0]
        detail["critical"] = "unknown"
        detail["high"] = None
        result = evaluate_bundle(bundle)
        self.assertIn(f"independent_review.detail.{detail['lane']}", result["blockers"])


if __name__ == "__main__":
    unittest.main()
