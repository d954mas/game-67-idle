import copy
import unittest

from jsonschema.exceptions import ValidationError

from capture_contracts import validate_document
from capture_safe_area import (
    SafeAreaError,
    UnsafeMask,
    derive_policy,
    evaluate_critical_regions,
    source_record_hash,
)


def source_record(
    *,
    direction,
    rectangles,
    placement_class="organic_standard",
    normalized_transform=None,
):
    return {
        "schema": "ai_studio.capture.safe_area_source.v1",
        "platform": "example",
        "surface": "vertical_feed",
        "placement_class": placement_class,
        "ui_variant_id": "playback-default-visible.v1",
        "caption_variant_id": "collapsed-standard-max.v1",
        "direction": direction,
        "locale": "en" if direction == "LTR" else "ar",
        "source": {
            "url": "https://example.invalid/safe-zone",
            "retrieved_at": "2026-07-26",
            "reviewed_at": "2026-07-26",
            "origin": "platform-published-template",
            "acquisition_method": "manual-download",
            "authority": (
                "platform_official"
                if placement_class == "organic_standard"
                else "first_party_measurement"
            ),
            "license": "platform-reference",
            "license_review": "reviewed",
            "redistribution": "external-only",
            "sha256": "a" * 64,
        },
        "surface_version": "example-app-1",
        "caption_bound": {
            "state": "collapsed",
            "max_visible_lines": 4,
            "obstruction_geometry_sha256": "b" * 64,
        },
        "original_dimensions": {"width": 10, "height": 10},
        "normalized_dimensions": {"width": 10, "height": 10},
        "normalized_transform": normalized_transform
        or {"scale_x": 1, "scale_y": 1, "offset_x": 0, "offset_y": 0},
        "geometry": {
            "polarity": "unsafe",
            "kind": "rectangles",
            "rectangles": rectangles,
        },
    }


class UnsafeMaskTest(unittest.TestCase):
    def test_rectangles_use_conservative_floor_ceil_rasterization(self):
        mask = UnsafeMask.from_normalized_rectangles(
            10, 10, [[0.21, 0.21, 0.39, 0.39]]
        )

        self.assertTrue(mask.is_unsafe(2, 2))
        self.assertTrue(mask.is_unsafe(3, 3))
        self.assertFalse(mask.is_unsafe(1, 2))
        self.assertFalse(mask.is_unsafe(4, 3))

    def test_any_touched_unsafe_pixel_rejects_critical_rectangle(self):
        mask = UnsafeMask.from_normalized_rectangles(
            10, 10, [[0.8, 0.0, 1.0, 1.0]]
        )

        self.assertTrue(mask.contains_critical_rect([0.1, 0.1, 0.7, 0.9]))
        self.assertFalse(mask.contains_critical_rect([0.1, 0.1, 0.81, 0.9]))

    def test_union_of_unsafe_masks_is_intersection_of_safe_regions(self):
        left = UnsafeMask.from_normalized_rectangles(
            10, 10, [[0.0, 0.0, 0.2, 1.0]]
        )
        right = UnsafeMask.from_normalized_rectangles(
            10, 10, [[0.8, 0.0, 1.0, 1.0]]
        )

        combined = left.union(right)

        self.assertFalse(combined.contains_critical_rect([0.0, 0.2, 0.3, 0.8]))
        self.assertTrue(combined.contains_critical_rect([0.2, 0.2, 0.8, 0.8]))


class SafeAreaPolicyTest(unittest.TestCase):
    def setUp(self):
        self.ltr = source_record(
            direction="LTR", rectangles=[[0.8, 0.0, 1.0, 1.0]]
        )
        self.rtl = source_record(
            direction="RTL", rectangles=[[0.0, 0.0, 0.2, 1.0]]
        )
        self.required = [
            {
                "platform": "example",
                "surface": "vertical_feed",
                "ui_variant_id": "playback-default-visible.v1",
                "caption_variant_id": "collapsed-standard-max.v1",
                "direction": "LTR",
            },
            {
                "platform": "example",
                "surface": "vertical_feed",
                "ui_variant_id": "playback-default-visible.v1",
                "caption_variant_id": "collapsed-standard-max.v1",
                "direction": "RTL",
            },
        ]

    def test_source_hash_excludes_identity_field_and_covers_transform_geometry(self):
        baseline = source_record_hash(self.ltr)
        self.ltr["source_record_hash"] = baseline
        self.assertEqual(source_record_hash(self.ltr), baseline)

        self.ltr["source_record_hash"] = "0" * 64
        with self.assertRaises(SafeAreaError):
            source_record_hash(self.ltr)
        self.ltr.pop("source_record_hash")

        transformed = copy.deepcopy(self.ltr)
        transformed["normalized_transform"]["offset_x"] = 1
        self.assertNotEqual(source_record_hash(transformed), baseline)

        changed_geometry = copy.deepcopy(self.ltr)
        changed_geometry["geometry"]["rectangles"] = [[0.7, 0.0, 1.0, 1.0]]
        self.assertNotEqual(source_record_hash(changed_geometry), baseline)

    def test_policy_identity_and_geometry_are_source_order_independent(self):
        first = derive_policy(
            "example-universal-v1", [self.ltr, self.rtl], self.required
        )
        second = derive_policy(
            "example-universal-v1", [self.rtl, self.ltr], self.required
        )

        self.assertEqual(first["policy_hash"], second["policy_hash"])
        self.assertEqual(first["derived_safe_mask_sha256"], second["derived_safe_mask_sha256"])
        self.assertEqual(first["status"], "official")

    def test_missing_required_direction_cannot_report_pass(self):
        policy = derive_policy("example-universal-v1", [self.ltr], self.required)

        self.assertEqual(policy["status"], "incomplete")
        self.assertEqual(policy["missing_variants"], [self.required[1]])

    def test_paid_ad_source_cannot_satisfy_standard_matrix(self):
        paid = source_record(
            direction="LTR",
            rectangles=[[0.8, 0.0, 1.0, 1.0]],
            placement_class="paid_ad",
        )
        policy = derive_policy(
            "example-universal-v1", [paid, self.rtl], self.required
        )

        self.assertEqual(policy["status"], "incomplete")
        self.assertEqual(policy["missing_variants"], [self.required[0]])

    def test_unreviewed_license_cannot_be_promoted_to_official(self):
        self.ltr["source"]["license_review"] = "unverified"

        policy = derive_policy(
            "example-universal-v1", [self.ltr, self.rtl], self.required
        )

        self.assertEqual(policy["status"], "incomplete")
        self.assertEqual(policy["missing_variants"], [self.required[0]])

    def test_measured_organic_evidence_can_only_produce_measured_status(self):
        measured_ltr = source_record(
            direction="LTR",
            rectangles=[[0.8, 0.0, 1.0, 1.0]],
            placement_class="measured_organic",
        )
        measured_rtl = source_record(
            direction="RTL",
            rectangles=[[0.0, 0.0, 0.2, 1.0]],
            placement_class="measured_organic",
        )

        policy = derive_policy(
            "example-universal-v1",
            [measured_ltr, measured_rtl],
            self.required,
        )

        self.assertEqual(policy["status"], "measured")

    def test_empty_geometry_is_not_eligible_evidence(self):
        empty = source_record(direction="LTR", rectangles=[])
        with self.assertRaises(ValidationError):
            derive_policy("example-universal-v1", [empty, self.rtl], self.required)

    def test_mismatched_domains_fail_instead_of_silently_resampling(self):
        different = copy.deepcopy(self.rtl)
        different["normalized_dimensions"] = {"width": 20, "height": 20}

        with self.assertRaises(SafeAreaError):
            derive_policy(
                "example-universal-v1", [self.ltr, different], self.required
            )

    def test_empty_or_duplicate_required_matrix_is_rejected(self):
        with self.assertRaises(SafeAreaError):
            derive_policy("example-universal-v1", [self.ltr], [])
        with self.assertRaises(SafeAreaError):
            derive_policy(
                "example-universal-v1",
                [self.ltr],
                [self.required[0], copy.deepcopy(self.required[0])],
            )


class DynamicCriticalRegionTest(unittest.TestCase):
    def setUp(self):
        self.mask = UnsafeMask.from_normalized_rectangles(
            10, 10, [[0.8, 0.0, 1.0, 1.0]]
        )

    def test_half_open_tick_intervals_pass_only_with_every_tick_measured(self):
        regions = [
            {
                "id": "hero",
                "start_tick": 2,
                "end_tick_exclusive": 5,
                "rectangle": [0.2, 0.2, 0.7, 0.8],
            }
        ]

        result = evaluate_critical_regions(self.mask, regions, measured_ticks=[2, 3, 4])

        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["required_ticks"], [2, 3, 4])
        validate_document(result, "critical-region-result.v1.schema.json")

    def test_missing_tick_returns_not_measured(self):
        regions = [
            {
                "id": "hero",
                "start_tick": 2,
                "end_tick_exclusive": 5,
                "rectangle": [0.2, 0.2, 0.7, 0.8],
            }
        ]

        result = evaluate_critical_regions(self.mask, regions, measured_ticks=[2, 4])

        self.assertEqual(result["status"], "not_measured")
        self.assertEqual(result["missing_ticks"], [3])

    def test_absent_region_declarations_are_not_measurement_evidence(self):
        result = evaluate_critical_regions(self.mask, [], measured_ticks=[])

        self.assertEqual(result["status"], "not_measured")

    def test_unsafe_region_fails_even_when_other_ticks_are_missing(self):
        regions = [
            {
                "id": "cta",
                "start_tick": 0,
                "end_tick_exclusive": 2,
                "rectangle": [0.7, 0.2, 0.9, 0.8],
            }
        ]

        result = evaluate_critical_regions(self.mask, regions, measured_ticks=[0])

        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["violations"][0]["id"], "cta")


if __name__ == "__main__":
    unittest.main()
