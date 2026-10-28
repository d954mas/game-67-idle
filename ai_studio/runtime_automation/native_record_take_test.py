from contextlib import contextmanager
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from ai_studio.runtime_automation.capture_game import (
    prepare_shot, record_native_take, validate_catalog, validate_native_record_receipt,
)


class Game:
    def __init__(self):
        self.calls = []
        self.view = {"fb_width": 640, "fb_height": 360}
        self.receipt = {"frames": 60, "audio_frames": 48000, "max_drift_ms": 5, "tail_delta_ms": 0, "valid": True}
        self.fail_stop = False

    def result(self, method, params=None):
        self.calls.append((method, params or {}))
        if method == "view":
            return self.view
        if method == "game.record.stop":
            if self.fail_stop:
                raise RuntimeError("stop disconnected")
            return self.receipt
        return {}

    @contextmanager
    def player_gated(self):
        self.result("input.set_player_enabled", {"enabled": False})
        try:
            yield self
        finally:
            self.result("input.set_player_enabled", {"enabled": True})


class NativeTakeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.game = Game()

    def record(self, **kwargs):
        with patch("ai_studio.runtime_automation.capture_game.validate_native_media", return_value={"decoded_frames": 60}) as validate:
            result = record_native_take(self.game, output_root=Path(self.directory.name), fps=60,
                                        width=640, height=360, duration_seconds=1,
                                        sleep=lambda _: None, **kwargs)
        return result, validate

    def test_live_unlocks_then_restores_input_and_validates_saved_media(self):
        result, validate = self.record()
        calls = self.game.calls
        unlock = next(i for i, row in enumerate(calls) if row[0] == "input.key")
        start = next(i for i, row in enumerate(calls) if row[0] == "game.record.start")
        self.assertLess(unlock, start)
        self.assertIn(("input.set_player_enabled", {"enabled": True}), calls[unlock:start])
        validate.assert_called_once()
        metadata = json.loads((Path(self.directory.name) / "capture.json").read_text())
        self.assertEqual(metadata["backend"], "engine")
        self.assertEqual(metadata["media"], result["media"])

    def test_shot_unlocks_before_fixture_and_keeps_player_input_disabled(self):
        self.record(recording_prepare=lambda: self.game.result("fixture.setup"))
        methods = [row[0] for row in self.game.calls]
        self.assertLess(methods.index("input.key"), methods.index("fixture.setup"))
        self.assertNotIn(("input.set_player_enabled", {"enabled": True}), self.game.calls)

    def test_rejects_empty_or_wrong_framebuffer_before_encoder_start(self):
        for width in (0, 320):
            self.game.view["fb_width"] = width
            with self.assertRaises(RuntimeError):
                self.record()
        self.assertNotIn("game.record.start", [row[0] for row in self.game.calls])

    def test_stops_on_driver_error_and_preserves_it_if_stop_also_fails(self):
        def broken():
            raise RuntimeError("scenario failed")
        for fail_stop in (False, True):
            self.game.fail_stop = fail_stop
            with self.assertRaisesRegex(RuntimeError, "scenario failed"):
                self.record(recording_driver=broken)
            self.assertEqual(self.game.calls[-1][0], "game.record.stop")

    def test_receipt_rejects_false_success_and_inconsistent_clocks(self):
        for change in ({"valid": False}, {"max_drift_ms": 250}, {"tail_delta_ms": 2000},
                       {"audio_frames": 1}, {"frames": 0}, {"frames": 60.0}):
            with self.subTest(change=change), self.assertRaises(RuntimeError):
                validate_native_record_receipt({**self.game.receipt, **change}, fps=60)

    def test_accepts_bounded_live_frame_boundary_overrun(self):
        self.game.receipt.update(frames=62, audio_frames=49600)
        self.record()
        (Path(self.directory.name) / "capture.json").unlink()
        self.game.receipt.update(frames=90, audio_frames=72000)
        with self.assertRaisesRegex(RuntimeError, "duration"):
            self.record()

    def test_existing_take_is_preserved_instead_of_pairing_old_receipt_with_new_video(self):
        manifest = Path(self.directory.name) / "capture.json"
        manifest.write_text('{"previous":true}')
        with self.assertRaisesRegex(RuntimeError, "new --out"):
            self.record()
        self.assertEqual(manifest.read_text(), '{"previous":true}')
        self.assertEqual(self.game.calls, [])

    def test_setup_converts_legacy_steps_to_wall_waits(self):
        waits = []
        prepare_shot(self.game, {"setup": [{"method": "time.step", "params": {"count": 120}}], "warmup_ticks": 30},
                     output_fps=60, ticks_per_frame=1, sleep=waits.append)
        self.assertEqual(waits, [2, .5])
        self.assertNotIn("time.step", [row[0] for row in self.game.calls])
        with self.assertRaisesRegex(ValueError, "manual"):
            prepare_shot(self.game, {"setup": [{"method": "time.set_mode", "params": {"mode": "manual"}}]})

    def test_catalog_validates_activity_expectations_and_unlock(self):
        for defaults in ({"expect_motion": 1}, {"expect_audio": "yes"}, {"audio_unlock": None},
                         {"max_freeze_seconds": float("nan")}):
            with self.subTest(defaults=defaults), self.assertRaises(ValueError):
                validate_catalog({"version": 1, "defaults": defaults})


if __name__ == "__main__":
    unittest.main()
