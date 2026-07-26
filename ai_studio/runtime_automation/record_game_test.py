from __future__ import annotations

import tempfile
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from unittest.mock import patch

from ai_studio.runtime_automation.record_game import (
    CapturePaths,
    build_launch_command,
    build_ddagrab_video_command,
    build_edit_command,
    parse_args,
    resolve_capture_settings,
    validate_start_delta,
    _prepare_outputs,
    _publish_take,
)


class CaptureSettingsTest(unittest.TestCase):
    def test_social_is_the_default_vertical_editing_preset(self) -> None:
        settings = resolve_capture_settings("social", None, None)

        self.assertEqual((settings.width, settings.height), (1080, 1920))
        self.assertEqual(settings.fps, 60)

    def test_explicit_size_and_fps_override_the_preset(self) -> None:
        settings = resolve_capture_settings("landscape", "2560x1440", 30)

        self.assertEqual((settings.width, settings.height), (2560, 1440))
        self.assertEqual(settings.fps, 30)

    def test_invalid_size_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "WIDTHxHEIGHT"):
            resolve_capture_settings("social", "1080", None)


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
                _publish_take(paths, staging, keep_parts=False)

            self.assertFalse(paths.master.exists())
            self.assertFalse(paths.edit.exists())
            self.assertFalse(paths.metadata.exists())

    def test_edit_export_copies_video_and_encodes_aac_audio(self) -> None:
        command = build_edit_command(
            Path("ffmpeg.exe"),
            Path("master.mkv"),
            Path("edit.mp4"),
        )

        self.assertIn("-c:v", command)
        self.assertEqual(command[command.index("-c:v") + 1], "copy")
        self.assertIn("-c:a", command)
        self.assertEqual(command[command.index("-c:a") + 1], "aac")
        self.assertEqual(command[-1], "edit.mp4")

    def test_capture_uses_desktop_duplication_for_the_window_region(self) -> None:
        command = build_ddagrab_video_command(
            Path("ffmpeg.exe"),
            x=100,
            y=50,
            source_width=1280,
            source_height=720,
            output=Path("video.mkv"),
            duration_seconds=10,
            fps=60,
            width=1080,
            height=1920,
        )

        source = command[command.index("-i") + 1]
        self.assertIn("ddagrab=", source)
        self.assertIn("offset_x=100", source)
        self.assertIn("offset_y=50", source)
        self.assertIn("video_size=1280x720", source)


class CliTest(unittest.TestCase):
    def test_personal_defaults_need_only_a_pid_or_exe(self) -> None:
        args = parse_args(["--pid", "123"])

        self.assertEqual(args.preset, "social")
        self.assertEqual(args.seconds, 30)
        self.assertEqual(args.countdown, 3)

    def test_pid_and_exe_are_mutually_exclusive(self) -> None:
        with redirect_stderr(StringIO()), self.assertRaises(SystemExit):
            parse_args(["--pid", "123", "--exe", "game.exe"])

    def test_launched_game_uses_a_screen_friendly_version_of_output_aspect(self) -> None:
        command = build_launch_command(
            Path("game.exe"),
            resolve_capture_settings("social", None, None),
        )

        self.assertEqual(
            command,
            ["game.exe", "--window-size", "720x1280"],
        )

    def test_capture_start_barrier_rejects_a_large_launcher_gap(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "start gap"):
            validate_start_delta(1_000_000_000, 1_050_000_000)


if __name__ == "__main__":
    unittest.main()
