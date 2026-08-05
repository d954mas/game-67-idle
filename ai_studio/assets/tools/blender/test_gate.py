import unittest

from ai_studio.assets.tools.blender.gate import evaluate_bundle


SCENE_HASH = "a" * 64


def passing_bundle():
    technical_ids = (
        "scene.units",
        "camera.contract",
        "topology.degenerate_faces",
        "topology.non_manifold_edges",
        "topology.boundary_edges",
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


class GateTests(unittest.TestCase):
    def test_complete_lookdev_bundle_passes(self):
        result = evaluate_bundle(passing_bundle())
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["blockers"], [])

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


if __name__ == "__main__":
    unittest.main()
