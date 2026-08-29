from __future__ import annotations

import json
import subprocess
import tempfile
import threading
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from ai_studio.runtime_automation.record_game import (
    CapturePaths,
    CaptureSettings,
    build_edit_command,
    build_freezedetect_command,
    build_launch_command,
    build_master_command,
    build_obs_launch_command,
    build_window_capture_settings,
    inspect_master,
    parse_args,
    resolve_capture_settings,
    resolve_obs_capture_settings,
    window_descriptor,
    _prepare_outputs,
    _preflight_obs_source,
    _publish_take,
    _record_audio_with_driver,
    _show_window_for_obs,
    _settle_game_capture,
    _extract_health_frame,
    _extract_preflight_frame,
    _assert_final_temporal_health,
    _content_start_offset_after_prepare,
    _obs_wgc_service_failure,
    _parse_freezedetect_log,
    _parse_content_marker,
    _scene_collection,
    _stop_obs,
    _stop_obs_and_detect_service_failure,
    _wait_for_finalized_recording,
    _write_obs_configuration,
    _should_tag_window,
    _tag_window_title,
)


class MediaInspectionTest(unittest.TestCase):
    def _runner(self, streams):
        probe = {
            "streams": streams,
            "format": {"duration": "5.0", "size": "1000"},
        }
        return lambda command, **kwargs: subprocess.CompletedProcess(
            command, 0, json.dumps(probe), ""
        )

    def test_accepts_expected_video_and_audio(self) -> None:
        streams = [
            {
                "codec_type": "video", "codec_name": "h264",
                "width": 1080, "height": 1920, "avg_frame_rate": "30/1",
                "nb_read_packets": "150",
            },
            {
                "codec_type": "audio", "codec_name": "aac",
                "sample_rate": "48000", "channels": 2,
            },
        ]

        result = inspect_master(
            Path("ffprobe.exe"),
            Path("master.mkv"),
            runner=self._runner(streams),
            expected_width=1080,
            expected_height=1920,
            expected_fps=30,
            expected_duration_seconds=5,
            expected_audio_codec="aac",
        )

        self.assertEqual(result["video"]["decodedFrames"], 150)
        self.assertEqual(result["audio"]["sampleRate"], 48_000)

    def test_rejects_missing_audio(self) -> None:
        streams = [{
            "codec_type": "video", "codec_name": "h264",
            "width": 1080, "height": 1920, "avg_frame_rate": "30/1",
            "nb_read_packets": "150",
        }]

        with self.assertRaisesRegex(RuntimeError, "one video and one audio"):
            inspect_master(
                Path("ffprobe.exe"),
                Path("master.mkv"),
                runner=self._runner(streams),
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

    def test_obs_capture_keeps_full_delivery_resolution(self) -> None:
        capture = resolve_obs_capture_settings(
            resolve_capture_settings("social", None, None)
        )

        self.assertEqual((capture.width, capture.height), (1080, 1920))
        self.assertEqual(capture.fps, 30)


class ObsContractTest(unittest.TestCase):
    def test_hidden_capture_settles_with_the_game_in_background(self) -> None:
        events: list[object] = []

        _settle_game_capture(
            123,
            hide_game_window=True,
            bring_window_forward=lambda hwnd: events.append(("restore", hwnd)),
            background_window=lambda hwnd: events.append(("background", hwnd)),
            sleep=lambda seconds: events.append(("sleep", seconds)),
        )

        self.assertEqual(events, [("background", 123), ("sleep", 0.25)])

    def test_hidden_capture_stays_foreground_until_preflight_has_passed(self) -> None:
        events: list[object] = []
        _show_window_for_obs(123, lambda hwnd: events.append(("bring", hwnd)))
        _settle_game_capture(
            123,
            hide_game_window=True,
            bring_window_forward=lambda hwnd: events.append(("restore", hwnd)),
            background_window=lambda hwnd: events.append(("background", hwnd)),
            sleep=lambda seconds: events.append(("sleep", seconds)),
        )

        self.assertEqual(
            events,
            [("bring", 123), ("background", 123), ("sleep", 0.25)],
        )

    def test_obs_source_uses_one_bounded_readiness_probe_per_restart(self) -> None:
        healthy = {"uniqueColors": 500}
        with patch(
            "ai_studio.runtime_automation.record_game._extract_preflight_frame",
            side_effect=[RuntimeError("black"), RuntimeError("black"), healthy],
        ) as extract, patch(
            "ai_studio.runtime_automation.record_game.time.sleep"
        ) as sleep:
            result = _preflight_obs_source(
                Path("ffmpeg.exe"),
                Path("recording.mkv"),
                Path("health.png"),
            )

        self.assertEqual(result, healthy)
        self.assertEqual(
            [call.args[3] for call in extract.call_args_list],
            [1.0, 2.5, 4.0],
        )
        self.assertEqual([call.args[0] for call in sleep.call_args_list], [1.0, 1.5, 1.5])

    def test_unique_window_suffix_is_restorable_and_uses_the_pid(self) -> None:
        changed: list[tuple[int, str]] = []

        original = _tag_window_title(
            123,
            456,
            read_title=lambda hwnd: "Planet Eater",
            write_title=lambda hwnd, title: changed.append((hwnd, title)),
        )

        self.assertEqual(original, "Planet Eater")
        self.assertEqual(changed, [(123, "Planet Eater [capture-456]")])

    def test_unique_window_keeps_its_original_title(self) -> None:
        should_tag = _should_tag_window(
            123,
            title="Template",
            class_name="GLFW30",
            visible_windows=lambda: [123],
            identity_for_window=lambda hwnd: ("Template", "GLFW30"),
        )

        self.assertFalse(should_tag)

    def test_duplicate_window_gets_a_unique_title_suffix(self) -> None:
        identities = {
            123: ("Template", "GLFW30"),
            456: ("Template", "GLFW30"),
        }
        should_tag = _should_tag_window(
            123,
            title="Template",
            class_name="GLFW30",
            visible_windows=lambda: [123, 456],
            identity_for_window=lambda hwnd: identities[hwnd],
        )

        self.assertTrue(should_tag)

    def test_wgc_missing_service_reports_required_host_context(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            portable_root = Path(directory)
            logs = portable_root / "config" / "obs-studio" / "logs"
            logs.mkdir(parents=True)
            (logs / "obs.txt").write_text(
                "[window-capture: 'Other'] CreateForWindow (0x80004005)\n"
                "[window-capture: 'Game'] CreateForWindow (0x80070424)",
                encoding="utf-8",
            )

            failure = _obs_wgc_service_failure(portable_root)

        self.assertIsNotNone(failure)
        self.assertIn("active console user", failure)
        self.assertIn("CaptureService availability", failure)

    def test_missing_obs_log_does_not_invent_a_host_context_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            failure = _obs_wgc_service_failure(Path(directory))

        self.assertIsNone(failure)

    def test_other_wgc_errors_do_not_claim_a_service_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            portable_root = Path(directory)
            logs = portable_root / "config" / "obs-studio" / "logs"
            logs.mkdir(parents=True)
            (logs / "obs.txt").write_text(
                "[window-capture: 'Game'] CreateForWindow (0x80004005)",
                encoding="utf-8",
            )

            failure = _obs_wgc_service_failure(portable_root)

        self.assertIsNone(failure)

    def test_obs_is_stopped_before_service_failure_log_is_classified(self) -> None:
        events: list[str] = []
        with patch(
            "ai_studio.runtime_automation.record_game._stop_obs",
            side_effect=lambda *_args, **_kwargs: events.append("stop"),
        ), patch(
            "ai_studio.runtime_automation.record_game._obs_wgc_service_failure",
            side_effect=(
                lambda _root: events.append("inspect") or "service failure"
            ),
        ):
            failure = _stop_obs_and_detect_service_failure(
                SimpleNamespace(),
                Path("portable"),
            )

        self.assertEqual(failure, "service failure")
        self.assertEqual(events, ["stop", "inspect"])

    def test_portable_profile_skips_the_first_run_wizard(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            portable_root = Path(directory)
            _write_obs_configuration(
                portable_root,
                descriptor="Title:GLFW30:game.exe",
                settings=CaptureSettings(720, 1280, 30),
                recording_directory=portable_root / "recordings",
            )
            global_ini = (
                portable_root / "config" / "obs-studio" / "global.ini"
            ).read_text(encoding="utf-8")

        self.assertIn("FirstRun=true", global_ini)
        self.assertIn("LastVersion=503382018", global_ini)
        self.assertIn("SysTrayEnabled=false", global_ini)
        self.assertIn("SysTrayWhenStarted=false", global_ini)

    def test_obs_starts_in_isolated_portable_mode_without_preview(self) -> None:
        command = build_obs_launch_command(Path("portable/bin/64bit/obs64.exe"))

        self.assertEqual(Path(command[0]), Path("portable/bin/64bit/obs64.exe"))
        self.assertIn("--portable", command)
        self.assertIn("--multi", command)
        self.assertNotIn("--minimize-to-tray", command)
        self.assertIn("--startrecording", command)

    def test_normal_shutdown_exits_after_a_short_close(self) -> None:
        process = SimpleNamespace(
            poll=lambda: None,
            pid=42,
            returncode=0,
            wait=lambda timeout: 0,
        )
        with patch(
            "ai_studio.runtime_automation.record_game._request_obs_close",
            return_value=1,
        ), patch(
            "ai_studio.runtime_automation.record_game._stop_process"
        ) as stop:
            result = _stop_obs(process, timeout_seconds=2.0, force=True)

        self.assertEqual(result, 0)
        stop.assert_not_called()

    def test_isolated_shutdown_forces_a_stuck_obs_after_short_close(self) -> None:
        process = SimpleNamespace(
            poll=lambda: None,
            pid=42,
            returncode=9,
            wait=lambda timeout: (_ for _ in ()).throw(subprocess.TimeoutExpired("obs", timeout)),
        )
        with patch(
            "ai_studio.runtime_automation.record_game._request_obs_close",
            return_value=1,
        ), patch(
            "ai_studio.runtime_automation.record_game._stop_process"
        ) as stop:
            result = _stop_obs(process, timeout_seconds=2.0, force=True)

        self.assertEqual(result, 9)
        stop.assert_called_once_with(process)

    def test_finalized_recording_rejects_a_missing_or_empty_mkv(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "missing.mkv"
            with self.assertRaisesRegex(RuntimeError, "not finalized"):
                _wait_for_finalized_recording(missing, timeout_seconds=0.0, sleep=lambda _seconds: None)
            missing.write_bytes(b"")
            with self.assertRaisesRegex(RuntimeError, "not finalized"):
                _wait_for_finalized_recording(missing, timeout_seconds=0.0, sleep=lambda _seconds: None)

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
        self.assertEqual(source["settings"]["window"], "Title:GLFW30:game.exe")

class OutputTest(unittest.TestCase):
    def test_recording_health_accepts_a_high_range_pale_game_frame(self) -> None:
        health = SimpleNamespace(
            unique_colors=500,
            unique_buckets=40,
            luma_range=250.0,
            luma_stdev=9.5,
        )
        with patch(
            "ai_studio.runtime_automation.record_game._run_media"
        ), patch(
            "ai_studio.runtime_automation.record_game.assert_pixel_health",
            return_value=health,
        ) as check:
            _extract_health_frame(
                Path("ffmpeg.exe"),
                Path("take.mkv"),
                Path("frame.png"),
                0.5,
            )

        check.assert_called_once_with(
            "frame.png",
            min_luma_stdev=8.0,
        )

    def test_preflight_accepts_a_simple_light_game_frame(self) -> None:
        health = SimpleNamespace(
            unique_colors=3,
            unique_buckets=1,
            luma_range=0.0,
            luma_stdev=0.0,
            luma_mean=255.0,
            luma_max=255.0,
        )
        with patch("ai_studio.runtime_automation.record_game._run_media"), patch(
            "ai_studio.runtime_automation.record_game.analyze_png",
            return_value=health,
        ):
            result = _extract_preflight_frame(
                Path("ffmpeg.exe"),
                Path("take.mkv"),
                Path("frame.png"),
                1.0,
            )

        self.assertEqual(result["uniqueColors"], 3)
        self.assertEqual(result["lumaMean"], 255.0)

    def test_preflight_rejects_a_near_black_wgc_frame(self) -> None:
        health = SimpleNamespace(
            unique_colors=1,
            unique_buckets=1,
            luma_range=0.0,
            luma_stdev=0.0,
            luma_mean=2.0,
            luma_max=2.0,
        )
        with patch("ai_studio.runtime_automation.record_game._run_media"), patch(
            "ai_studio.runtime_automation.record_game.analyze_png",
            return_value=health,
        ), self.assertRaisesRegex(RuntimeError, "near-black"):
            _extract_preflight_frame(
                Path("ffmpeg.exe"),
                Path("take.mkv"),
                Path("frame.png"),
                1.0,
            )

    def test_final_health_keeps_the_strict_pixel_contract(self) -> None:
        with patch("ai_studio.runtime_automation.record_game._run_media"), patch(
            "ai_studio.runtime_automation.record_game.assert_pixel_health",
            side_effect=RuntimeError("strict final health"),
        ), self.assertRaisesRegex(RuntimeError, "strict final health"):
            _extract_health_frame(
                Path("ffmpeg.exe"),
                Path("edit.mp4"),
                Path("frame.png"),
                1.0,
            )

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

        self.assertNotIn("-ss", command)
        self.assertEqual(command[command.index("-t") + 1], "5.000")
        self.assertEqual(command[command.index("-c:v") + 1], "h264_nvenc")
        self.assertEqual(command[command.index("-c:a") + 1], "aac")
        self.assertIn("game.wav", command)
        self.assertIn("1:a:0", command)
        video_filter = command[command.index("-vf") + 1]
        audio_filter = command[command.index("-af") + 1]
        self.assertIn("trim=start=2.000", video_filter)
        self.assertIn("setpts=PTS-STARTPTS", video_filter)
        self.assertIn("scale=1080:1920", video_filter)
        self.assertIn("apad=whole_dur=5.000", audio_filter)
        self.assertIn("atrim=duration=5.000", audio_filter)
        self.assertIn("asetpts=PTS-STARTPTS", audio_filter)
        self.assertEqual(command[-1], "master.mkv")

    def test_edit_export_is_a_lossless_container_remux(self) -> None:
        command = build_edit_command(
            Path("ffmpeg.exe"),
            Path("master.mkv"),
            Path("edit.mp4"),
        )

        self.assertEqual(command.count("copy"), 2)
        self.assertEqual(command[-1], "edit.mp4")

    def test_freeze_command_has_a_small_motion_noise_threshold(self) -> None:
        command = build_freezedetect_command(Path("ffmpeg.exe"), Path("edit.mp4"))

        self.assertIn("freezedetect=n=0.0001:d=0.100", command)
        self.assertEqual(command[-2:], ["null", "-"])

    def test_freeze_log_reports_the_longest_confirmed_still_period(self) -> None:
        result = _parse_freezedetect_log(
            "freeze_start: 1.2\nfreeze_duration: 0.067\n"
            "freeze_start: 3.5\nfreeze_duration: 5.000\n"
        )

        self.assertEqual(result["maxFreezeSeconds"], 5.0)
        self.assertEqual(result["freezeCount"], 2)

    def test_content_marker_uses_media_duration_and_rejects_missing_value(self) -> None:
        self.assertEqual(_parse_content_marker("11.533\n"), 11.533)
        with self.assertRaisesRegex(RuntimeError, "marker"):
            _parse_content_marker("N/A\n")

    def test_freeze_log_counts_an_unclosed_still_until_media_end(self) -> None:
        result = _parse_freezedetect_log(
            "freeze_start: 3.5\n",
            media_duration_seconds=8.0,
        )

        self.assertEqual(result["maxFreezeSeconds"], 4.5)
        self.assertEqual(result["freezeCount"], 1)

    def test_temporal_gate_rejects_a_freeze_above_its_limit(self) -> None:
        completed = subprocess.CompletedProcess(
            ["ffmpeg"],
            0,
            "",
            "freeze_start: 1.2\nfreeze_duration: 1.500\n",
        )
        with patch(
            "ai_studio.runtime_automation.record_game._run_media",
            return_value=completed,
        ):
            with self.assertRaisesRegex(RuntimeError, "1.500s"):
                _assert_final_temporal_health(
                    Path("ffmpeg.exe"),
                    Path("edit.mp4"),
                    0.5,
                )


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
    def test_prepare_finishes_before_the_content_offset_is_measured(self) -> None:
        events: list[str] = []

        offset = _content_start_offset_after_prepare(
            10.0,
            lambda: events.append("prepare"),
            monotonic=lambda: events.append("measure") or 12.5,
        )

        self.assertEqual(events, ["prepare", "measure"])
        self.assertEqual(offset, 2.5)

    def test_audio_warmup_completes_before_the_timeline_starts(self) -> None:
        helper_ready = threading.Event()
        capture_started = threading.Event()
        events: list[str] = []

        def capture_audio() -> dict:
            events.append("audio-launch")
            helper_ready.set()
            self.assertTrue(capture_started.wait(timeout=1))
            events.append("audio-capture")
            return {"sampleFrames": 48000}

        result = _record_audio_with_driver(
            capture_audio,
            lambda: events.append("timeline"),
            wait_audio_ready=lambda: helper_ready.wait(timeout=1) and events.append("ready"),
            before_driver=lambda: events.append("prepare"),
            start_audio=lambda: events.append("start") or capture_started.set(),
        )

        self.assertEqual(result, {"sampleFrames": 48000})
        self.assertLess(events.index("audio-launch"), events.index("ready"))
        self.assertLess(events.index("ready"), events.index("prepare"))
        self.assertLess(events.index("prepare"), events.index("start"))
        self.assertLess(events.index("start"), events.index("timeline"))

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
