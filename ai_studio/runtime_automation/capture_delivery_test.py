import json
import math
import unittest
from pathlib import Path

from capture_contracts import CaptureContractError
from capture_delivery import validate_descriptor


CONSTRAINT_ROOT = (
    Path(__file__).with_name("policies") / "delivery_constraints"
)


def universal_descriptor():
    return {
        "schema": "ai_studio.capture.delivery_media_descriptor.v1",
        "container": "mp4",
        "width": 1080,
        "height": 1920,
        "fps": {"numerator": 60, "denominator": 1},
        "video_codec": "h264",
        "audio_codec": "aac-lc",
        "sample_rate": 48000,
        "duration_seconds": 30,
        "bytes": 80_000_000,
    }


class DeliveryConstraintTest(unittest.TestCase):
    def setUp(self):
        self.constraints = [
            json.loads(path.read_text(encoding="utf-8"))
            for path in sorted(CONSTRAINT_ROOT.glob("*.json"))
        ]

    def test_one_vertical_descriptor_passes_all_four_constraint_sets(self):
        results = [
            validate_descriptor(universal_descriptor(), constraint)
            for constraint in self.constraints
        ]

        self.assertEqual(len(results), 4)
        self.assertTrue(all(result["status"] == "pass" for result in results))

    def test_platform_specific_failure_names_the_responsible_constraint(self):
        descriptor = universal_descriptor()
        descriptor["video_codec"] = "vp9"
        results = [
            validate_descriptor(descriptor, constraint)
            for constraint in self.constraints
        ]
        failed = [result for result in results if result["status"] == "fail"]

        self.assertEqual([result["constraint_id"] for result in failed], ["youtube-shorts-v1"])
        self.assertIn("video_codec", failed[0]["failures"])

    def test_resolution_and_frame_rate_limits_are_checked(self):
        youtube = next(
            constraint
            for constraint in self.constraints
            if constraint["id"] == "youtube-shorts-v1"
        )
        descriptor = universal_descriptor()
        descriptor["width"] = 2160
        descriptor["height"] = 3840
        descriptor["fps"] = {"numerator": 120, "denominator": 1}

        result = validate_descriptor(descriptor, youtube)

        self.assertEqual(result["status"], "fail")
        self.assertEqual(
            set(result["failures"]), {"width_above_max", "height_above_max", "fps_above_max"}
        )

    def test_invalid_descriptor_dimensions_fail_without_division_error(self):
        descriptor = universal_descriptor()
        descriptor["width"] = 0
        descriptor["height"] = 0

        with self.assertRaises(CaptureContractError):
            validate_descriptor(descriptor, self.constraints[0])

    def test_malformed_media_facts_are_rejected_before_platform_rules(self):
        broken_values = (
            ("fps", "nan"),
            ("fps", {"numerator": 0, "denominator": 1}),
            ("duration_seconds", -1),
            ("duration_seconds", math.nan),
            ("bytes", -1),
        )
        for field, value in broken_values:
            with self.subTest(field=field, value=value):
                descriptor = universal_descriptor()
                descriptor[field] = value
                with self.assertRaises(CaptureContractError) as raised:
                    validate_descriptor(descriptor, self.constraints[0])
                self.assertEqual(raised.exception.code, "CONTRACT_MISMATCH")

    def test_missing_media_fact_is_rejected_instead_of_becoming_zero(self):
        descriptor = universal_descriptor()
        del descriptor["duration_seconds"]

        with self.assertRaises(CaptureContractError):
            validate_descriptor(descriptor, self.constraints[0])


if __name__ == "__main__":
    unittest.main()
