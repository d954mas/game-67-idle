from __future__ import annotations

import tempfile
import threading
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from unittest.mock import patch

from ai_studio.runtime_automation.record_game import (
    CapturePaths,
    CaptureSettings,
    build_edit_command,
    build_launch_command,
    build_master_command,
    build_obs_launch_command,
    build_window_capture_settings,
    parse_args,
    resolve_capture_settings,
    resolve_obs_capture_settings,
    window_descriptor,
    _prepare_outputs,
    _publish_take,
    _record_audio_with_driver,
    _scene_collection,
)


class CaptureSettingsTest(unittest.TestCase):
    def test_social_is_the_default_vertical_editing_preset(self) -> None:
        settings = resolve_capture_settings("social", None, None)

        self.assertEqual((settings.width, settings.height), (1080, 1920))
        self.assertEqual(settings.fps, 30)

    def test_explicit_size_and_fps_override_the_preset(self) -> None:
        settings = resolve_capture_settings("landscape", "2560x1440", 30)

        self.assertEqual((settings.width, settings.height), (2560, 1440))
        self.assertEqual(settings.fps, 30)

    def test_invalid_size_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "WIDTHxHEIGHT"):
            resolve_capture_settings("social", "1080", None)

    def test_social_capture_is_lightweight_but_keeps_delivery_aspect(self) -> None:
        capture = resolve_obs_capture_settings(
            resolve_capture_settings("social", None, None)
        )

        self.assertEqual((capture.width, capture.height), (720, 1280))
        self.assertEqual(capture.fps, 30)


class ObsContractTest(unittest.TestCase):
    def test_obs_starts_in_isolated_portable_mode_without_preview(self) -> None:
        command = build_obs_launch_command(Path("portable/bin/64bit/obs64.exe"))

        self.assertEqual(Path(command[0]), Path("portable/bin/64bit/obs64.exe"))
        self.assertIn("--portable", command)
        self.assertIn("--multi", command)
        self.assertIn("--minimize-to-tray", command)
        self.assertIn("--startrecording", command)

    def test_window_capture_targets_exact_window_with_wgc(self) -> None:
        descriptor = window_descriptor(
            title="Example Game",
            class_name="GLFW30",
            executable_name="game.exe",
        )
        settings = build_window_capture_settings(descriptor)

        self.assertEqual(
            settings["window"],
            "Example Game:GLFW30:game.exe",
        )
        self.assertEqual(settings["method"], 2)
        self.assertFalse(settings["cursor"])
        self.assertFalse(settings["capture_audio"])

    def test_scene_collection_wires_the_wgc_window_source(self) -> None:
        collection = _scene_collection(
            "Title:GLFW30:game.exe",
            CaptureSettings(720, 1280, 30),
        )

        source = next(
            item for item in collection["sources"] if item["name"] == "Game"
        )
        self.assertEqual(source["id"], "window_capture")
        self.assertEqual(source["settings"]["method"], 2)

class OutputTest(unittest.TestCase):
    def test_capture_paths_are_predictable(self) -> None:
        root = Path("takes") / "tram-01"
        paths = CapturePaths.from_root(root)

        self.assertEqual(paths.master, root / "master.mkv")
        self.assertEqual(paths.edit, root / "edit.mp4")
        self.assertEqual(paths.metadata, root / "capture.json")

        staging = paths.staging()
        self.assertEqual(staging.master, root / ".master.partial.mkv")
        self.assertEqual(staging.edit, root / ".edit.partial.mp4")

    def test_existing_take_is_never_deleted_for_a_retry(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            paths = CapturePaths.from_root(Path(directory))
            paths.master.write_bytes(b"good take")

            with self.assertRaisesRegex(RuntimeError, "already contains"):
                _prepare_outputs(paths)

            self.assertEqual(paths.master.read_bytes(), b"good take")

    def test_failed_publication_rolls_back_already_promoted_files(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            paths = CapturePaths.from_root(Path(directory))
            staging = paths.staging()
            for path in (staging.master, staging.edit, staging.metadata):
                path.write_bytes(b"complete")
            real_replace = __import__("os").replace
            calls = 0

            def flaky_replace(source: Path, target: Path) -> None:
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError("disk error")
                real_replace(source, target)

            with patch(
                "ai_studio.runtime_automation.record_game.os.replace",
                side_effect=flaky_replace,
            ), self.assertRaisesRegex(RuntimeError, "publish"):
                _publish_take(paths, staging)

            self.assertFalse(paths.master.exists())
            self.assertFalse(paths.edit.exists())
            self.assertFalse(paths.metadata.exists())

    def test_master_export_trims_upscales_and_uses_nvenc(self) -> None:
        command = build_master_command(
            Path("ffmpeg.exe"),
            Path("obs-source.mkv"),
            Path("game.wav"),
            Path("master.mkv"),
            start_seconds=2,
            duration_seconds=5,
            width=1080,
            height=1920,
            fps=30,
        )

        self.assertEqual(command[command.index("-ss") + 1], "2.000")
        self.assertEqual(command[command.index("-t") + 1], "5.000")
        self.assertEqual(command[command.index("-c:v") + 1], "h264_nvenc")
        self.assertEqual(command[command.index("-c:a") + 1], "aac")
        self.assertIn("game.wav", command)
        self.assertIn("1:a:0", command)
        self.assertIn("scale=1080:1920", command[command.index("-vf") + 1])
        self.assertEqual(command[-1], "master.mkv")

    def test_edit_export_is_a_lossless_container_remux(self) -> None:
        command = build_edit_command(
            Path("ffmpeg.exe"),
            Path("master.mkv"),
            Path("edit.mp4"),
        )

        self.assertEqual(command.count("copy"), 2)
        self.assertEqual(command[-1], "edit.mp4")


class CliTest(unittest.TestCase):
    def test_personal_defaults_need_only_a_pid_or_exe(self) -> None:
        args = parse_args(["--pid", "123"])

        self.assertEqual(args.preset, "social")
        self.assertEqual(args.seconds, 30)
        self.assertEqual(args.countdown, 3)
        self.assertIsNone(args.obs)
        self.assertFalse(hasattr(args, "helper"))

    def test_pid_and_exe_are_mutually_exclusive(self) -> None:
        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            parse_args(["--pid", "123", "--exe", "game.exe"])

    def test_launched_game_uses_a_screen_friendly_version_of_output_aspect(self) -> None:
        command = build_launch_command(
            Path("game.exe"),
            resolve_capture_settings("social", None, None),
        )

        self.assertEqual(command, ["game.exe", "--window-size", "720x1280"])


class RecordingDriverTest(unittest.TestCase):
    def test_optional_driver_runs_while_process_audio_is_active(self) -> None:
        audio_started = threading.Event()
        driver_completed = threading.Event()

        def capture_audio() -> dict:
            audio_started.set()
            self.assertTrue(driver_completed.wait(timeout=1))
            return {"sampleFrames": 48000}

        def drive_scenario() -> None:
            self.assertTrue(audio_started.wait(timeout=1))
            driver_completed.set()

        result = _record_audio_with_driver(capture_audio, drive_scenario)

        self.assertEqual(result, {"sampleFrames": 48000})

    def test_absent_driver_keeps_the_existing_synchronous_path(self) -> None:
        calls = []

        result = _record_audio_with_driver(
            lambda: calls.append("audio") or {"sampleFrames": 1},
            None,
        )

        self.assertEqual(calls, ["audio"])
        self.assertEqual(result["sampleFrames"], 1)


if __name__ == "__main__":
    unittest.main()
