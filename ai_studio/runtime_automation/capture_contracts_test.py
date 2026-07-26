import math
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator
from jsonschema.exceptions import ValidationError

from capture_contracts import (
    ATTEMPT_TRANSITIONS,
    CaptureContractError,
    canonical_hash,
    canonical_json_bytes,
    load_schema,
    transition,
    validate_document,
)


SCHEMA_DIR = Path(__file__).with_name("schemas")


def valid_target():
    return {
        "schema": "ai_studio.capture.target.v1",
        "id": "vertical-social-1080p60",
        "version": 1,
        "width": 1080,
        "height": 1920,
        "fps": {"numerator": 60, "denominator": 1},
        "framing": "portrait",
        "capture_profile": "realtime-av-h264-aac-v1",
        "safe_area_policy": "universal-social-v1",
        "delivery_preset": "social-mp4-v1",
    }


def valid_audio_policy():
    return {
        "schema": "ai_studio.capture.audio_policy.v1",
        "id": "none",
        "expectation": "any",
        "source_ids": [],
    }


class CanonicalJsonTest(unittest.TestCase):
    def test_hash_is_independent_of_mapping_insertion_order(self):
        left = {"z": [3, {"b": True, "a": "тест"}], "a": 1}
        right = {"a": 1, "z": [3, {"a": "тест", "b": True}]}

        self.assertEqual(canonical_json_bytes(left), canonical_json_bytes(right))
        self.assertEqual(canonical_hash(left), canonical_hash(right))

    def test_non_finite_number_is_rejected_with_stable_error_code(self):
        for value in (math.nan, math.inf, -math.inf):
            with self.subTest(value=value):
                with self.assertRaises(CaptureContractError) as raised:
                    canonical_json_bytes({"value": value})
                self.assertEqual(raised.exception.code, "CONTRACT_MISMATCH")
                self.assertIn("remediation", raised.exception.safe_details)

    def test_schema_boundary_rejects_non_finite_number(self):
        target = valid_target()
        target["width"] = math.nan

        with self.assertRaises(CaptureContractError):
            validate_document(target, "target.v1.schema.json")

    def test_mathematically_equal_integral_numbers_have_one_identity(self):
        self.assertEqual(canonical_json_bytes({"value": 1}), canonical_json_bytes({"value": 1.0}))
        self.assertEqual(canonical_hash({"value": 0.0}), canonical_hash({"value": -0.0}))


class JsonSchemaContractTest(unittest.TestCase):
    def test_all_wp0_schemas_are_valid_draft_2020_12(self):
        expected = {
            "target.v1.schema.json",
            "audio-policy.v1.schema.json",
            "recording-job.v1.schema.json",
            "attempt.v1.schema.json",
            "realtime-master-take.v1.schema.json",
            "delivery-preset.v1.schema.json",
            "encode-job.v1.schema.json",
            "delivery-artifact.v1.schema.json",
            "safe-area-source.v1.schema.json",
            "safe-area-policy.v1.schema.json",
            "safe-area-requirements.v1.schema.json",
            "delivery-constraint.v1.schema.json",
            "delivery-media-descriptor.v1.schema.json",
            "critical-region-result.v1.schema.json",
        }
        self.assertTrue(expected.issubset({path.name for path in SCHEMA_DIR.glob("*.json")}))
        for name in expected:
            with self.subTest(schema=name):
                Draft202012Validator.check_schema(load_schema(name))

    def test_target_schema_accepts_approved_vertical_target(self):
        validate_document(valid_target(), "target.v1.schema.json")

    def test_target_schema_rejects_unknown_fields(self):
        target = valid_target()
        target["platform_magic"] = True
        with self.assertRaises(ValidationError):
            validate_document(target, "target.v1.schema.json")

    def test_none_audio_policy_cannot_hide_a_selected_source(self):
        policy = valid_audio_policy()
        policy["source_ids"] = ["default-microphone"]
        with self.assertRaises(ValidationError):
            validate_document(policy, "audio-policy.v1.schema.json")

    def test_combination_audio_policy_requires_two_explicit_sources(self):
        policy = valid_audio_policy()
        policy["id"] = "game+mic"
        policy["source_ids"] = ["game-process"]
        with self.assertRaises(ValidationError):
            validate_document(policy, "audio-policy.v1.schema.json")

    def test_safe_area_source_rejects_invalid_provenance_url_and_date(self):
        source = {
            "schema": "ai_studio.capture.safe_area_source.v1",
            "platform": "example",
            "surface": "vertical_feed",
            "placement_class": "measured_organic",
            "ui_variant_id": "playback-default-visible.v1",
            "caption_variant_id": "collapsed-standard-max.v1",
            "direction": "LTR",
            "locale": "en",
            "source": {
                "url": "not a URL",
                "retrieved_at": "yesterday",
                "reviewed_at": "also yesterday",
                "origin": "measured-device",
                "acquisition_method": "screenshot",
                "license": "project-evidence-only",
                "authority": "first_party_measurement",
                "license_review": "reviewed",
                "redistribution": "external-only",
                "sha256": "a" * 64,
            },
            "surface_version": "example-1",
            "caption_bound": {
                "state": "collapsed",
                "max_visible_lines": 4,
                "obstruction_geometry_sha256": "b" * 64,
            },
            "original_dimensions": {"width": 1080, "height": 1920},
            "normalized_dimensions": {"width": 1080, "height": 1920},
            "normalized_transform": {
                "scale_x": 1,
                "scale_y": 1,
                "offset_x": 0,
                "offset_y": 0,
            },
            "geometry": {
                "polarity": "unsafe",
                "kind": "rectangles",
                "rectangles": [[0, 0, 0.1, 1]],
            },
        }

        with self.assertRaises(ValidationError):
            validate_document(source, "safe-area-source.v1.schema.json")

    def test_terminal_attempt_states_enforce_terminal_fields(self):
        base = {
            "schema": "ai_studio.capture.attempt.v1",
            "id": "attempt-1",
            "job_id": "job-1",
            "number": 1,
            "state": "created",
            "staging_path": "tmp/attempt-1",
            "stop_reason": None,
            "failure_code": None,
        }
        validate_document(base, "attempt.v1.schema.json")

        promoted = dict(base, state="promoted", stop_reason="requested")
        validate_document(promoted, "attempt.v1.schema.json")

        with self.assertRaises(ValidationError):
            validate_document(dict(base, state="promoted"), "attempt.v1.schema.json")
        with self.assertRaises(ValidationError):
            validate_document(
                dict(base, state="failed", failure_code=None),
                "attempt.v1.schema.json",
            )
        with self.assertRaises(ValidationError):
            validate_document(
                dict(base, state="recording", stop_reason="requested"),
                "attempt.v1.schema.json",
            )

    def test_promoted_artifacts_require_structured_probe_and_provenance(self):
        probe = {
            "schema": "ai_studio.capture.media_probe.v1",
            "probe_sha256": "d" * 64,
            "container": "matroska",
            "width": 1920,
            "height": 1080,
            "fps": {"numerator": 60, "denominator": 1},
            "video_codec": "h264",
            "duration_seconds": 30,
            "audio_streams": [
                {
                    "track_index": 0,
                    "source_id": "game-process",
                    "role": "game",
                    "codec": "pcm_s16le",
                    "sample_rate": 48000,
                    "channels": 2,
                    "channel_layout": "stereo",
                }
            ],
            "audio_policy_id": "game",
        }
        provenance = {
            "schema": "ai_studio.capture.provenance.v1",
            "created_at": "2026-07-26T12:00:00Z",
            "job_hash": "e" * 64,
            "capability_hash": "f" * 64,
            "environment_hash": "1" * 64,
            "media_probe_hash": "d" * 64,
            "terminal_status": "promoted",
            "artifacts": [
                {"path": "recording.mkv", "sha256": "a" * 64, "bytes": 1}
            ],
        }
        master = {
            "schema": "ai_studio.capture.realtime_master_take.v1",
            "id": "take-1",
            "job_id": "job-1",
            "attempt_id": "attempt-1",
            "kind": "realtime_av",
            "artifact": {"path": "recording.mkv", "sha256": "a" * 64, "bytes": 1},
            "media_probe": probe,
            "provenance": provenance,
        }
        validate_document(master, "realtime-master-take.v1.schema.json")

        broken_master = dict(master, media_probe={})
        with self.assertRaises(ValidationError):
            validate_document(broken_master, "realtime-master-take.v1.schema.json")

        missing_audio = dict(master, media_probe=dict(probe, audio_streams=[]))
        with self.assertRaises(ValidationError):
            validate_document(missing_audio, "realtime-master-take.v1.schema.json")

        silent_probe = dict(probe, audio_policy_id="none", audio_streams=[])
        validate_document(
            dict(master, media_probe=silent_probe),
            "realtime-master-take.v1.schema.json",
        )

        duplicate_combo = dict(
            probe,
            audio_policy_id="game+mic",
            audio_streams=[
                dict(probe["audio_streams"][0], role="microphone", track_index=0),
                dict(probe["audio_streams"][0], role="microphone", track_index=1),
                dict(
                    probe["audio_streams"][0],
                    role="compatibility_mix",
                    track_index=2,
                ),
            ],
        )
        with self.assertRaises(ValidationError):
            validate_document(
                dict(master, media_probe=duplicate_combo),
                "realtime-master-take.v1.schema.json",
            )

        valid_combo = dict(
            duplicate_combo,
            audio_streams=[
                dict(
                    probe["audio_streams"][0],
                    source_id="compatibility-mix",
                    role="compatibility_mix",
                    track_index=0,
                ),
                dict(probe["audio_streams"][0], role="game", track_index=1),
                dict(probe["audio_streams"][0], role="microphone", track_index=2),
            ],
        )
        validate_document(
            dict(master, media_probe=valid_combo),
            "realtime-master-take.v1.schema.json",
        )

        delivery = {
            "schema": "ai_studio.capture.delivery_artifact.v1",
            "id": "delivery-1",
            "encode_job_id": "encode-1",
            "master_take_id": "take-1",
            "master_take_hash": "b" * 64,
            "preset_id": "social-mp4-v1",
            "artifact": {"path": "clip.mp4", "sha256": "c" * 64, "bytes": 1},
            "media_probe": dict(probe, container="mp4"),
            "constraint_results": [
                {
                    "schema": "ai_studio.capture.delivery_constraint_result.v1",
                    "constraint_id": "youtube-shorts-v1",
                    "platform": "youtube",
                    "surface": "shorts",
                    "constraint_hash": "2" * 64,
                    "coverage": "official",
                    "status": "pass",
                    "failures": [],
                }
            ],
            "provenance": provenance,
        }
        validate_document(delivery, "delivery-artifact.v1.schema.json")

        broken_delivery = dict(delivery, constraint_results=[])
        with self.assertRaises(ValidationError):
            validate_document(broken_delivery, "delivery-artifact.v1.schema.json")

        contradictory_pass = dict(delivery)
        contradictory_pass["constraint_results"] = [
            dict(delivery["constraint_results"][0], failures=["video_codec"])
        ]
        with self.assertRaises(ValidationError):
            validate_document(
                contradictory_pass, "delivery-artifact.v1.schema.json"
            )

        contradictory_fail = dict(delivery)
        contradictory_fail["constraint_results"] = [
            dict(delivery["constraint_results"][0], status="fail", failures=[])
        ]
        with self.assertRaises(ValidationError):
            validate_document(
                contradictory_fail, "delivery-artifact.v1.schema.json"
            )


class RecordingStateMachineTest(unittest.TestCase):
    def test_happy_path_reaches_promoted_only_through_validating(self):
        state = "created"
        for next_state in (
            "preflighted",
            "countdown",
            "recording",
            "stopping",
            "validating",
            "promoted",
        ):
            state = transition(state, next_state, ATTEMPT_TRANSITIONS)
        self.assertEqual(state, "promoted")

    def test_illegal_transition_has_stable_code_and_remediation(self):
        with self.assertRaises(CaptureContractError) as raised:
            transition("recording", "promoted", ATTEMPT_TRANSITIONS)
        self.assertEqual(raised.exception.code, "CONTRACT_MISMATCH")
        self.assertEqual(raised.exception.safe_details["from"], "recording")
        self.assertEqual(raised.exception.safe_details["to"], "promoted")
        self.assertIn("remediation", raised.exception.safe_details)

    def test_every_nonterminal_attempt_state_can_fail_or_be_abandoned(self):
        for state in (
            "created",
            "preflighted",
            "countdown",
            "recording",
            "stopping",
            "validating",
        ):
            with self.subTest(state=state):
                self.assertEqual(transition(state, "failed", ATTEMPT_TRANSITIONS), "failed")
                self.assertEqual(
                    transition(state, "abandoned", ATTEMPT_TRANSITIONS), "abandoned"
                )

    def test_stopping_transition_is_idempotent(self):
        self.assertEqual(
            transition("stopping", "stopping", ATTEMPT_TRANSITIONS), "stopping"
        )


if __name__ == "__main__":
    unittest.main()
