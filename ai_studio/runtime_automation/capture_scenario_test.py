import math
import unittest

from capture_scenario import (
    PROTOTYPE_CAPTURE_TIMING,
    PROTOTYPE_HANDOFF_STATUS,
    CaptureScenarioError,
    expand_schedule,
    parse_scenario,
    validate_against_describe,
)


def manifest():
    return {
        "schema": "ai_studio.capture_scenario", "version": 1, "api_version": 1,
        "game": "example-game",
        "scene": {"id": "fake.scene", "contract_version": 1, "seed": 42},
        "viewport": {"orientation": "vertical", "output_width": 1080, "output_height": 1920,
                     "min_framebuffer_width": 1080, "min_framebuffer_height": 1920,
                     "prefer_supersample": True},
        "clock": {"fixed_tick_hz": 60, "output_fps": 30, "ticks_per_output_frame": 2,
                  "warmup_ticks": 2, "duration_frames": 4},
        "events": [{"frame": 1, "action": {"id": "go", "arguments": {}}}],
        "ramps": [{"start_frame": 0, "end_frame": 3, "parameter": "scale",
                   "from": 1.0, "to": 2.0, "curve": "smoothstep"}],
        "evidence": {"boundary_radius_frames": 1, "uniform_contact_sheet_samples": 4},
    }


def describe():
    return {"apiVersion": 1, "gameId": "example-game",
            "scene": {"id": "fake.scene", "contractVersion": 1,
                      "parameters": [{"id": "scale", "type": "float", "minimum": 0.5, "maximum": 2.0}],
                      "actions": [{"id": "go", "arguments": []}]}}


class CaptureScenarioModelTest(unittest.TestCase):
    def test_deprecated_runner_cannot_publish_ready_exact_contract(self):
        self.assertEqual(PROTOTYPE_CAPTURE_TIMING, "after-first-step-of-batch")
        self.assertEqual(PROTOTYPE_HANDOFF_STATUS, "prototype-rejected")

    def test_smoothstep_expansion_has_exact_endpoints(self):
        scenario = parse_scenario(manifest())
        schedule = expand_schedule(scenario)
        self.assertEqual(schedule[0][0]["set"]["value"], 1.0)
        self.assertEqual(schedule[3][0]["set"]["value"], 2.0)
        frame_one_set = next(event["set"] for event in schedule[1] if "set" in event)
        self.assertTrue(math.isclose(frame_one_set["value"], 1.2592592592592593))

    def test_unknown_key_is_rejected_at_nested_level(self):
        value = manifest()
        value["scene"]["extra"] = True
        with self.assertRaisesRegex(CaptureScenarioError, "unknown key"):
            parse_scenario(value)

    def test_non_empty_action_arguments_are_rejected(self):
        value = manifest()
        value["events"][0]["action"]["arguments"] = {"force": True}
        with self.assertRaisesRegex(CaptureScenarioError, "empty arguments"):
            parse_scenario(value)

    def test_duplicate_same_frame_parameter_write_is_rejected(self):
        value = manifest()
        value["events"].append({"frame": 0, "set": {"parameter": "scale", "value": 1.0}})
        with self.assertRaisesRegex(CaptureScenarioError, "duplicate parameter write"):
            parse_scenario(value)

    def test_cadence_must_be_exact_integer_relation(self):
        value = manifest()
        value["clock"]["fixed_tick_hz"] = 59
        with self.assertRaisesRegex(CaptureScenarioError, "fixed_tick_hz"):
            parse_scenario(value)

    def test_describe_validates_scene_parameters_and_actions(self):
        scenario = parse_scenario(manifest())
        validate_against_describe(scenario, describe())
        bad = describe()
        bad["scene"]["actions"] = []
        with self.assertRaisesRegex(CaptureScenarioError, "unknown action"):
            validate_against_describe(scenario, bad)


if __name__ == "__main__":
    unittest.main()
