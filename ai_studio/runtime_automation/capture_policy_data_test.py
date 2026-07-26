import json
import unittest
from pathlib import Path

from capture_contracts import validate_document
from capture_safe_area import derive_policy


POLICY_ROOT = Path(__file__).with_name("policies")


class BuiltinCapturePolicyDataTest(unittest.TestCase):
    def test_approved_targets_are_strict_versioned_documents(self):
        expected = {
            "landscape-1080p60.json",
            "vertical-social-1080p60.json",
            "square-1080p60.json",
        }
        paths = list((POLICY_ROOT / "targets").glob("*.json"))
        self.assertEqual({path.name for path in paths}, expected)
        targets = [json.loads(path.read_text(encoding="utf-8")) for path in paths]
        for target in targets:
            validate_document(target, "target.v1.schema.json")
        social = next(
            target for target in targets if target["id"] == "vertical-social-1080p60"
        )
        self.assertEqual(social["width"], 1080)
        self.assertEqual(social["height"], 1920)
        self.assertEqual(social["safe_area_policy"], "universal-social-v1")

    def test_universal_social_matrix_has_four_surfaces_and_both_directions(self):
        path = POLICY_ROOT / "safe_area" / "universal-social-v1.requirements.json"
        document = json.loads(path.read_text(encoding="utf-8"))
        validate_document(document, "safe-area-requirements.v1.schema.json")
        variants = document["required_variants"]

        self.assertEqual(len(variants), 8)
        surfaces = {(row["platform"], row["surface"]) for row in variants}
        self.assertEqual(
            surfaces,
            {
                ("tiktok", "feed"),
                ("youtube", "shorts"),
                ("instagram", "reels"),
                ("facebook", "reels"),
            },
        )
        for platform, surface in surfaces:
            directions = {
                row["direction"]
                for row in variants
                if row["platform"] == platform and row["surface"] == surface
            }
            self.assertEqual(directions, {"LTR", "RTL"})

        unresolved = derive_policy(
            "universal-social-v1", [], document["required_variants"]
        )
        self.assertEqual(unresolved["status"], "incomplete")
        self.assertEqual(len(unresolved["missing_variants"]), 8)

    def test_delivery_constraints_are_separate_and_never_overclaim_sources(self):
        paths = list((POLICY_ROOT / "delivery_constraints").glob("*.json"))
        self.assertEqual(len(paths), 4)
        documents = [json.loads(path.read_text(encoding="utf-8")) for path in paths]
        for document in documents:
            validate_document(document, "delivery-constraint.v1.schema.json")
        self.assertEqual(
            {(item["platform"], item["surface"]) for item in documents},
            {
                ("tiktok", "feed"),
                ("youtube", "shorts"),
                ("instagram", "reels"),
                ("facebook", "reels"),
            },
        )
        self.assertTrue(
            all(item["status"] in {"official", "official_partial"} for item in documents)
        )
        self.assertTrue(
            all(item["source"]["reviewed_at"] == "2026-07-26" for item in documents)
        )


if __name__ == "__main__":
    unittest.main()
