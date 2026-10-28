from pathlib import Path
import os
from io import StringIO
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from unittest.mock import patch

from ai_studio.runtime_automation.capture_game import (
    load_catalog,
    main,
    parse_args,
    play_timeline,
    resolve_game_executable,
    resolve_shot,
    run,
    validate_catalog,
    default_output_root,
    resolve_studio_root,
    resolve_output_root,
)
from ai_studio.runtime_automation.native_capture_preflight import (
    canonical_native_executable,
    executable_identity,
)


def capture_executable_relative() -> str:
    return str(Path("build") / "devapi-debug" / "bin" / ("game.exe" if os.name == "nt" else "game"))


def declared_executable_name() -> str:
    return "plw_client.exe" if os.name == "nt" else "plw_client"


def native_build_record(executable: Path) -> dict:
    return {
        "proofKind": "native-rebuilt",
        "sourceFingerprint": "a" * 64,
        "executable": executable_identity(executable),
    }


class CaptureGameCliTest(unittest.TestCase):
    def test_relative_output_stays_in_selected_game_despite_studio_runner_cwd(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory) / "selected-game"
            self.assertEqual(resolve_output_root(game, Path("evidence/take")), (game / "evidence/take").resolve())
            absolute = Path(directory).resolve() / "explicit-output"
            self.assertEqual(resolve_output_root(game, absolute), absolute)

    def test_catalog_controls_live_capture_defaults(self) -> None:
        args = parse_args(["games/private/example"])

        self.assertEqual(args.game, Path("games/private/example"))
        self.assertEqual(args.mode, "live")
        self.assertIsNone(args.preset)
        self.assertIsNone(args.seconds)
        self.assertEqual(args.countdown, 0)

    def test_resolves_the_standard_debug_executable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)

            self.assertEqual(resolve_game_executable(root), executable.resolve())

    def test_executable_override_accepts_only_the_canonical_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)

            self.assertEqual(
                resolve_game_executable(root, override=executable.relative_to(root)),
                executable.resolve(),
            )
            self.assertEqual(
                resolve_game_executable(root, override=executable),
                executable.resolve(),
            )
            with self.assertRaisesRegex(RuntimeError, "canonical DevAPI artifact"):
                resolve_game_executable(root, override=Path("frozen/game.exe"))

    def test_missing_executable_override_fails_before_launch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)
            executable.parent.mkdir(parents=True)
            executable.touch()
            (root / "capture").mkdir()
            (root / "capture" / "catalog.json").write_text(json.dumps({
                "version": 1, "executable": capture_executable_relative(),
                "defaults": {"size": "1280x720", "fps": 30},
                "live": {"seconds": 1}, "shots": {},
            }), encoding="utf-8")

            with patch("ai_studio.runtime_automation.capture_game.running_game") as launch:
                with self.assertRaisesRegex(RuntimeError, "canonical DevAPI artifact"):
                    run(parse_args([str(root), "live", "--executable", "missing/game.exe"]))

            launch.assert_not_called()

    def test_preflight_build_failure_prevents_launch_and_recorder(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "capture").mkdir()
            (root / "capture" / "catalog.json").write_text(json.dumps({
                "version": 1,
                "executable": capture_executable_relative(),
                "defaults": {"size": "1280x720", "fps": 30},
                "live": {"seconds": 1},
                "shots": {},
            }), encoding="utf-8")
            with patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                side_effect=RuntimeError("native build failed"),
            ), patch("ai_studio.runtime_automation.capture_game.running_game") as launch, patch(
                "ai_studio.runtime_automation.capture_game.record_native_take"
            ) as recorder:
                with self.assertRaisesRegex(RuntimeError, "native build failed"):
                    run(parse_args([str(root), "live"]))

            launch.assert_not_called()
            recorder.assert_not_called()

    def test_noncanonical_catalog_executable_reports_the_build_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()

            with self.assertRaisesRegex(RuntimeError, r"build[\\/]devapi-debug"):
                resolve_game_executable(root, "out/game.exe")

    def test_catalog_may_declare_its_own_build_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            relative = Path("build") / "native-devapi" / "bin" / declared_executable_name()

            self.assertEqual(resolve_game_executable(root, str(relative)), (root / relative).resolve())
            self.assertEqual(
                resolve_game_executable(root, override=root / relative), (root / relative).resolve()
            )

    def test_declared_artifact_must_stay_in_a_build_bin_directory(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            name = declared_executable_name()
            for rejected in (
                f"build/native-devapi/{name}",
                f"build/native-devapi/bin/tools/{name}",
                f"build/native-devapi/bin/../../../{name}",
                f"build/native-devapi/bin/../../../../outside/build/x/bin/{name}",
                f"build/{name}",
                f"out/native/bin/{name}",
                "build/native-devapi/bin/plw_client.txt" if os.name == "nt" else "build/x/bin/plw_client.sh",
            ):
                with self.subTest(rejected=rejected):
                    with self.assertRaisesRegex(RuntimeError, "canonical DevAPI artifact"):
                        resolve_game_executable(root, rejected)

            directory_artifact = root / "build" / "native-devapi" / "bin" / name
            directory_artifact.mkdir(parents=True)
            with self.assertRaisesRegex(RuntimeError, "canonical DevAPI artifact"):
                resolve_game_executable(root, f"build/native-devapi/bin/{name}")

    def test_declared_artifact_rejects_a_linked_build_directory(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve() / "game"
            foreign = Path(directory).resolve() / "foreign" / "bin"
            foreign.mkdir(parents=True)
            (foreign / declared_executable_name()).touch()
            (root / "build").mkdir(parents=True)
            link = root / "build" / "linked"
            try:
                if os.name == "nt":
                    import _winapi
                    _winapi.CreateJunction(str(foreign.parent), str(link))
                else:
                    os.symlink(foreign.parent, link, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"directory links are unavailable: {exc}")

            with self.assertRaisesRegex(RuntimeError, "without links"):
                resolve_game_executable(root, f"build/linked/bin/{declared_executable_name()}")

    def test_run_launches_the_declared_artifact_with_catalog_launch_flags(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            relative = Path("build") / "native-devapi" / "bin" / declared_executable_name()
            executable = root / relative
            executable.parent.mkdir(parents=True)
            executable.touch()
            (root / "capture").mkdir()
            (root / "capture" / "catalog.json").write_text(json.dumps({
                "version": 1, "executable": relative.as_posix(),
                "launch": {"fresh_state": False, "autosave_enabled": True, "args": ["--solo"]},
                "defaults": {"size": "1280x720", "fps": 30},
                "live": {"seconds": 1}, "shots": {"idle": {"seconds": 1}},
            }), encoding="utf-8")
            game = FakeGame()
            game.process_id = 123
            launch = {}

            class GameContext:
                def __enter__(self):
                    return game

                def __exit__(self, *_args):
                    return None

            def fake_running_game(**kwargs):
                launch.update(kwargs)
                return GameContext()

            with patch("ai_studio.runtime_automation.capture_game.running_game", fake_running_game), patch(
                "ai_studio.runtime_automation.capture_game.record_native_take",
                return_value={"edit": root / "edit.mp4", "master": root / "master.mkv"},
            ), patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                return_value=native_build_record(executable),
            ) as prepare, patch(
                "ai_studio.runtime_automation.capture_game.validate_native_capture_final",
            ):
                run(parse_args([str(root), "shot", "idle"]))

            self.assertEqual(prepare.call_args.kwargs["requested_executable"], executable.resolve())
            self.assertEqual(Path(launch["exe"]), executable.resolve())
            self.assertFalse(launch["fresh_state"])
            self.assertTrue(launch["autosave_enabled"])
            self.assertEqual(launch["extra_args"], ["--no-vsync", "--solo"])

    def test_parses_a_deterministic_shot_command(self) -> None:
        args = parse_args(["games/private/example", "shot", "intro"])

        self.assertEqual(args.mode, "shot")
        self.assertEqual(args.shot_id, "intro")

    def test_loads_the_game_owned_capture_catalog(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "executable": capture_executable_relative(),
                        "defaults": {"size": "1920x1080", "fps": 30, "ticks_per_frame": 2},
                        "live": {"seconds": 60},
                        "shots": {
                            "intro": {
                                "seconds": 2,
                                "warmup_ticks": 2,
                                "setup": [{"method": "game.planet.stage_intro"}],
                                "events": [{"frame": 0, "method": "input.key", "params": {"key": "W"}}],
                            }
                        },
                    }
                ),
                encoding="utf-8",
            )

            catalog = load_catalog(root)
            shot = resolve_shot(catalog, "intro")

        self.assertEqual(catalog["defaults"]["ticks_per_frame"], 2)
        self.assertEqual(shot["seconds"], 2)
        self.assertEqual(shot["events"][0]["frame"], 0)

    def test_run_uses_catalog_settings_and_a_driver_for_shots(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)
            executable.parent.mkdir(parents=True)
            executable.touch()
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "executable": capture_executable_relative(),
                        "defaults": {
                            "preset": "landscape",
                            "size": "1280x720",
                            "fps": 30,
                            "ticks_per_frame": 2,
                        },
                        "shots": {"intro": {"seconds": 0.1, "preset": "social", "size": "1080x1920", "fps": 60, "backend": "game", "ticks_per_frame": 1, "setup": [{"method": "game.planet.stage_intro"}], "events": []}},
                    }
                ),
                encoding="utf-8",
            )
            game = FakeGame()
            game.process_id = 123
            launch = {}
            recorded = {}
            clock = FakeClock()

            def drive_timeline(*args, **kwargs):
                return play_timeline(*args, **kwargs, monotonic=clock.monotonic, sleep=clock.sleep)

            class GameContext:
                def __enter__(self):
                    return game

                def __exit__(self, *_args):
                    return None

            def fake_running_game(**kwargs):
                launch.update(kwargs)
                return GameContext()

            def fake_record_native_take(**kwargs):
                recorded.update(kwargs)
                kwargs["recording_prepare"]()
                recorded["prepare_end"] = len(game.calls)
                kwargs["recording_driver"]()
                (root / "capture.json").write_text("{}\n", encoding="utf-8")
                return {"edit": root / "edit.mp4", "master": root / "master.mp4"}

            with patch(
                "ai_studio.runtime_automation.capture_game.running_game",
                fake_running_game,
            ), patch(
                "ai_studio.runtime_automation.capture_game.record_native_take",
                fake_record_native_take,
            ), patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                return_value=native_build_record(executable),
            ), patch(
                "ai_studio.runtime_automation.capture_game.validate_native_capture_final",
            ), patch(
                "ai_studio.runtime_automation.capture_game.play_timeline",
                side_effect=drive_timeline,
            ):
                result = run(parse_args([str(root), "shot", "intro"]))

        self.assertEqual(result["edit"].name, "edit.mp4")
        self.assertEqual(result["effectiveExecutable"]["path"], str(executable.resolve()))
        self.assertIn("sha256", result["effectiveExecutable"])
        self.assertEqual(result["nativeBuild"]["sourceFingerprint"], "a" * 64)
        self.assertEqual(launch["exe"], str(executable.resolve()))
        self.assertEqual(launch["window_size"], "1080x1920")
        self.assertEqual(recorded["duration_seconds"], 0.1)
        self.assertEqual(recorded["width"], 1080)
        self.assertEqual(recorded["height"], 1920)
        self.assertTrue(callable(recorded["recording_prepare"]))
        self.assertTrue(callable(recorded["recording_driver"]))
        self.assertIsNone(recorded["max_freeze_seconds"])
        self.assertEqual(recorded["fps"], 60)
        self.assertAlmostEqual(clock.now, recorded["duration_seconds"])

        prepare_end = recorded["prepare_end"]
        preparation = [call for call in game.calls[:prepare_end] if call[0] not in ("endpoints", "command.describe")]
        self.assertEqual(preparation, [
            ("time.set_fps", {"fps": 60}),
            ("time.set_scale", {"scale": 1.0}),
            ("time.set_mode", {"mode": "run"}),
            ("input.set_player_enabled", {"enabled": False}),
            ("game.planet.stage_intro", {}),
        ])
        self.assertNotIn(("game.planet.stage_intro", {}), game.calls[prepare_end:])
        self.assertIn(("time.set_mode", {"mode": "run"}), game.calls[prepare_end:])
        self.assertEqual(game.calls[-2:], [
            ("input.set_player_enabled", {"enabled": True}),
            ("time.set_mode", {"mode": "run"}),
        ])

    def test_shot_overrides_capture_defaults(self) -> None:
        catalog = validate_catalog({
            "version": 1,
            "defaults": {"preset": "landscape", "size": "1920x1080", "fps": 30, "ticks_per_frame": 2},
            "shots": {
                "portrait": {
                    "seconds": 2,
                    "preset": "social",
                    "size": "1080x1920",
                    "fps": 60,
                    "ticks_per_frame": 1,
                    "events": [],
                }
            },
        })

        self.assertEqual(catalog["shots"]["portrait"]["fps"], 60)


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0

    def monotonic(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


class FakeGame:
    def __init__(self) -> None:
        self.calls: list[tuple[str, dict]] = []
        self.fail_method: str | None = None

    def result(self, method: str, params: dict | None = None):
        self.calls.append((method, params or {}))
        if method == self.fail_method:
            raise RuntimeError(f"{method} failed")
        return {}


class TimelineTest(unittest.TestCase):
    def test_timeline_runs_game_and_schedules_events_on_output_frames(self) -> None:
        game = FakeGame()
        clock = FakeClock()
        shot = {
            "seconds": 0.1,
            "events": [
                {"frame": 2, "method": "input.key", "params": {"key": "D"}},
                {"frame": 0, "method": "game.planet.autopilot", "params": {"enabled": True}},
            ],
        }

        result = play_timeline(
            game,
            shot,
            output_fps=30,
            monotonic=clock.monotonic,
            sleep=clock.sleep,
        )

        self.assertEqual(result, {"frames": 3, "events": 2})
        self.assertEqual(
            game.calls,
            [
                ("time.set_mode", {"mode": "run"}),
                ("game.planet.autopilot", {"enabled": True}),
                ("input.key", {"key": "D"}),
            ],
        )
        self.assertAlmostEqual(clock.now, 0.1)

    def test_realtime_timeline_fails_when_wall_time_exceeds_duration(self) -> None:
        class SlowGame(FakeGame):
            def result(self, method: str, params: dict | None = None):
                value = super().result(method, params)
                clock.now += 0.1
                return value

        clock = FakeClock()
        with self.assertRaisesRegex(RuntimeError, "timeline exceeded"):
            play_timeline(
                SlowGame(),
                {"seconds": 0.1, "events": [{"frame": 0, "method": "event.first"}, {"frame": 1, "method": "event.second"}]},
                output_fps=30,
                monotonic=clock.monotonic,
                sleep=clock.sleep,
            )

    def test_prepare_runs_setup_before_driver_and_cleanup_restores_game(self) -> None:
        game = FakeGame()
        clock = FakeClock()
        shot = {
            "seconds": 0.1,
            "warmup_ticks": 2,
            "setup": [{"method": "game.planet.stage_intro"}],
            "events": [],
        }

        from ai_studio.runtime_automation.capture_game import cleanup_shot, prepare_shot

        prepare_shot(game, shot)
        prepared = list(game.calls)
        play_timeline(game, shot, output_fps=30, monotonic=clock.monotonic, sleep=clock.sleep)
        after_driver = list(game.calls)
        cleanup_shot(game)

        self.assertNotIn(("input.set_player_enabled", {"enabled": True}), after_driver)
        self.assertIn(("time.set_mode", {"mode": "run"}), after_driver)
        self.assertEqual(prepared, [
            ("time.set_fps", {"fps": 60}),
            ("time.set_scale", {"scale": 1.0}),
            ("time.set_mode", {"mode": "run"}),
            ("input.set_player_enabled", {"enabled": False}),
            ("game.planet.stage_intro", {}),
        ])
        self.assertNotIn(("game.planet.stage_intro", {}), game.calls[len(prepared):])
        self.assertEqual(game.calls[-2:], [
            ("input.set_player_enabled", {"enabled": True}),
            ("time.set_mode", {"mode": "run"}),
        ])

    def test_cleanup_runs_after_a_failing_timeline_step(self) -> None:
        game = FakeGame()
        game.fail_method = "input.key"
        from ai_studio.runtime_automation.capture_game import cleanup_shot, prepare_shot

        prepare_shot(game, {"seconds": 0.1, "events": []})
        with self.assertRaisesRegex(RuntimeError, "input.key failed"):
            try:
                play_timeline(game, {"seconds": 0.1, "events": [{"frame": 0, "method": "input.key"}]}, output_fps=30)
            finally:
                cleanup_shot(game)

        self.assertEqual(game.calls[-2:], [
            ("input.set_player_enabled", {"enabled": True}),
            ("time.set_mode", {"mode": "run"}),
        ])

    def test_prepare_keeps_setup_error_when_rollback_fails(self) -> None:
        setup_error = RuntimeError("setup failed")

        class SetupAndRollbackFailingGame:
            def __init__(self) -> None:
                self.calls: list[tuple[str, dict]] = []

            def result(self, method: str, params: dict | None = None):
                payload = params or {}
                self.calls.append((method, payload))
                if method == "game.planet.stage_intro":
                    raise setup_error
                if method == "input.set_player_enabled" and payload["enabled"]:
                    raise ConnectionResetError("rollback socket closed")
                return {}

        game = SetupAndRollbackFailingGame()
        from ai_studio.runtime_automation.capture_game import prepare_shot

        with self.assertRaises(RuntimeError) as failure:
            prepare_shot(game, {"setup": [{"method": "game.planet.stage_intro"}]})

        self.assertIs(failure.exception, setup_error)
        self.assertIn("rollback socket closed", "\n".join(failure.exception.__notes__))
        self.assertEqual(game.calls[-2:], [
            ("input.set_player_enabled", {"enabled": True}),
            ("time.set_mode", {"mode": "run"}),
        ])

    def test_prepare_rolls_back_when_disabling_input_fails(self) -> None:
        disable_error = ConnectionResetError("disable socket closed")

        class DisableFailingGame:
            def __init__(self) -> None:
                self.calls: list[tuple[str, dict]] = []

            def result(self, method: str, params: dict | None = None):
                payload = params or {}
                self.calls.append((method, payload))
                if method == "input.set_player_enabled" and not payload["enabled"]:
                    raise disable_error
                return {}

        game = DisableFailingGame()
        from ai_studio.runtime_automation.capture_game import prepare_shot

        with self.assertRaises(ConnectionResetError) as failure:
            prepare_shot(game, {"setup": []})

        self.assertIs(failure.exception, disable_error)
        self.assertEqual(game.calls[-2:], [
            ("input.set_player_enabled", {"enabled": True}),
            ("time.set_mode", {"mode": "run"}),
        ])

    def test_cleanup_keeps_first_restore_error_and_attempts_second(self) -> None:
        input_error = ConnectionResetError("input restore closed")
        time_error = ConnectionResetError("time restore closed")

        class DoubleFailingGame:
            def __init__(self) -> None:
                self.calls: list[tuple[str, dict]] = []

            def result(self, method: str, params: dict | None = None):
                self.calls.append((method, params or {}))
                if method == "input.set_player_enabled":
                    raise input_error
                raise time_error

        game = DoubleFailingGame()
        from ai_studio.runtime_automation.capture_game import cleanup_shot

        with self.assertRaises(ConnectionResetError) as failure:
            cleanup_shot(game)

        self.assertIs(failure.exception, input_error)
        self.assertIn("time restore closed", "\n".join(failure.exception.__notes__))
        self.assertEqual(game.calls, [
            ("input.set_player_enabled", {"enabled": True}),
            ("time.set_mode", {"mode": "run"}),
        ])

class CaptureGameAdoptionTest(unittest.TestCase):
    def test_catalog_rejects_invalid_shots_before_a_game_is_launched(self) -> None:
        with self.assertRaisesRegex(ValueError, "catalog shots must be an object"):
            validate_catalog({"version": 1, "shots": []})

    def test_missing_shot_lists_available_ids_before_build_lookup(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps({"version": 1, "live": {"seconds": 15}, "shots": {"growth": {"seconds": 2}, "shop": {"seconds": 2}}}),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "growth, shop"):
                run(parse_args([str(root), "shot"]))

    def test_shot_rejects_seconds_override_before_a_game_is_launched(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps({"version": 1, "shots": {"intro": {"seconds": 2, "events": []}}}),
                encoding="utf-8",
            )

            with self.assertRaisesRegex(ValueError, "catalog owns shot duration"):
                run(parse_args([str(root), "shot", "intro", "--seconds", "4"]))
    def test_catalog_rejects_bad_events_before_a_game_is_launched(self) -> None:
        with self.assertRaisesRegex(ValueError, "catalog shot intro event 0 method"):
            validate_catalog(
                {"version": 1, "shots": {"intro": {"seconds": 2, "events": [{"frame": 0}]}}}
            )
    def test_catalog_rejects_negative_warmup_ticks_before_a_game_is_launched(self) -> None:
        with self.assertRaisesRegex(ValueError, "warmup_ticks"):
            validate_catalog(
                {"version": 1, "shots": {"intro": {"seconds": 2, "warmup_ticks": -1}}}
            )
    def test_catalog_rejects_event_after_the_last_output_frame(self) -> None:
        with self.assertRaisesRegex(ValueError, "outside output frames"):
            validate_catalog({
                "version": 1,
                "defaults": {"fps": 30},
                "shots": {"intro": {"seconds": 0.1, "events": [{"frame": 3, "method": "input.key"}]}},
            })

    def test_catalog_rejects_fractional_output_frame_duration(self) -> None:
        with self.assertRaisesRegex(ValueError, "whole number of output frames"):
            validate_catalog({
                "version": 1,
                "defaults": {"fps": 30},
                "shots": {"intro": {"seconds": 0.05, "events": []}},
            })
    def test_default_output_roots_do_not_collide(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            first = default_output_root(root)
            second = default_output_root(root)

        self.assertNotEqual(first, second)
        self.assertEqual(first.parent, root / "tmp" / "captures")

    def test_run_keeps_recording_error_when_shot_cleanup_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)
            executable.parent.mkdir(parents=True)
            executable.touch()
            (root / "capture").mkdir()
            (root / "capture" / "catalog.json").write_text(json.dumps({
                "version": 1, "executable": capture_executable_relative(),
                "defaults": {"size": "1280x720", "fps": 30},
                "shots": {"intro": {"seconds": 1}},
            }), encoding="utf-8")

            class CleanupFailingGame:
                process_id = 123

                def result(self, method: str, _params: dict | None = None):
                    if method == "input.set_player_enabled":
                        raise ConnectionResetError("cleanup socket closed")
                    return {}

            class GameContext:
                def __enter__(self):
                    return CleanupFailingGame()

                def __exit__(self, *_args):
                    return None

            recording_error = RuntimeError("game window did not remain stable for 2 seconds")
            with patch("ai_studio.runtime_automation.capture_game.running_game", return_value=GameContext()), patch(
                "ai_studio.runtime_automation.capture_game.record_native_take",
                side_effect=recording_error,
            ), patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                return_value=native_build_record(executable),
            ):
                with self.assertRaises(RuntimeError) as failure:
                    run(parse_args([str(root), "shot", "intro"]))

        self.assertIs(failure.exception, recording_error)
        self.assertIn("cleanup socket closed", "\n".join(failure.exception.__notes__))

    def test_run_propagates_shot_cleanup_error_after_successful_recording(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)
            executable.parent.mkdir(parents=True)
            executable.touch()
            (root / "capture").mkdir()
            (root / "capture" / "catalog.json").write_text(json.dumps({
                "version": 1, "executable": capture_executable_relative(),
                "defaults": {"size": "1280x720", "fps": 30},
                "shots": {"intro": {"seconds": 1}},
            }), encoding="utf-8")

            class CleanupFailingGame:
                process_id = 123

                def result(self, method: str, _params: dict | None = None):
                    if method == "input.set_player_enabled":
                        raise ConnectionResetError("cleanup socket closed")
                    return {}

            class GameContext:
                def __enter__(self):
                    return CleanupFailingGame()

                def __exit__(self, *_args):
                    return None

            with patch("ai_studio.runtime_automation.capture_game.running_game", return_value=GameContext()), patch(
                "ai_studio.runtime_automation.capture_game.record_native_take",
                return_value={"edit": root / "edit.mp4", "master": root / "master.mkv"},
            ), patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                return_value=native_build_record(executable),
            ), patch("ai_studio.runtime_automation.capture_game.validate_native_capture_final"):
                with self.assertRaisesRegex(ConnectionResetError, "cleanup socket closed"):
                    run(parse_args([str(root), "shot", "intro"]))

    def test_studio_root_resolution_supports_template_and_private_games(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            (root / "ai_studio" / "runtime_automation").mkdir(parents=True)
            (root / "ai_studio" / "runtime_automation" / "capture_game.py").touch()
            self.assertEqual(resolve_studio_root(root / "templates" / "template"), root)
            self.assertEqual(resolve_studio_root(root / "games" / "private" / "example-game"), root)
            wrapper = (Path(__file__).parents[2] / "ai_studio" / "feature_testing" / "consumer" / "capture.cmd").read_text(encoding="ascii")
            self.assertIn(":find_studio", wrapper)
            self.assertIn("ai_studio\\dev_environment\\python_run.mjs", wrapper)
            self.assertNotIn("py -3.12", wrapper)

    def test_live_uses_saved_state_autosave_and_fifteen_second_default(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)
            executable.parent.mkdir(parents=True)
            executable.touch()
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps({"version": 1, "executable": capture_executable_relative(), "defaults": {"size": "1280x720", "fps": 30}, "live": {"seconds": 15}, "shots": {}}),
                encoding="utf-8",
            )
            game = FakeGame()
            game.process_id = 123
            launch = {}
            recorded = {}

            class GameContext:
                def __enter__(self):
                    return game

                def __exit__(self, *_args):
                    return None

            def fake_running_game(**kwargs):
                launch.update(kwargs)
                return GameContext()

            def fake_record_native_take(**kwargs):
                print("backend progress")
                recorded.update(kwargs)
                return {"edit": root / "edit.mp4", "master": root / "master.mkv"}

            with patch("ai_studio.runtime_automation.capture_game.running_game", fake_running_game), patch(
                "ai_studio.runtime_automation.capture_game.record_native_take", fake_record_native_take
            ), patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                return_value=native_build_record(executable),
            ), patch(
                "ai_studio.runtime_automation.capture_game.validate_native_capture_final",
            ):
                output = StringIO()
                with redirect_stdout(output):
                    run(parse_args([str(root), "live"]))
                self.assertEqual(output.getvalue(), "")
                self.assertEqual(recorded["duration_seconds"], 15.0)
                run(parse_args([str(root), "live", "--seconds", "4"]))

        self.assertFalse(launch["fresh_state"])
        self.assertTrue(launch["autosave_enabled"])
        self.assertIsNone(recorded["recording_driver"])
        self.assertIsNone(recorded["max_freeze_seconds"])
        self.assertEqual(recorded["duration_seconds"], 4.0)

    def test_catalog_launch_options_override_template_flags(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)
            executable.parent.mkdir(parents=True)
            executable.touch()
            (root / "capture").mkdir()
            (root / "capture" / "catalog.json").write_text(json.dumps({
                "version": 1, "executable": capture_executable_relative(),
                "launch": {"fresh_state": True, "autosave_enabled": False, "args": ["--solo"]},
                "defaults": {"size": "1280x720", "fps": 30},
                "live": {"seconds": 1}, "shots": {},
            }), encoding="utf-8")
            game = FakeGame()
            game.process_id = 123
            launch = {}

            class GameContext:
                def __enter__(self):
                    return game

                def __exit__(self, *_args):
                    return None

            def fake_running_game(**kwargs):
                launch.update(kwargs)
                return GameContext()

            with patch("ai_studio.runtime_automation.capture_game.running_game", fake_running_game), patch(
                "ai_studio.runtime_automation.capture_game.record_native_take",
                return_value={"edit": root / "edit.mp4", "master": root / "master.mkv"},
            ), patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                return_value=native_build_record(executable),
            ), patch(
                "ai_studio.runtime_automation.capture_game.validate_native_capture_final",
            ):
                run(parse_args([str(root), "live"]))

            self.assertTrue(launch["fresh_state"])
            self.assertFalse(launch["autosave_enabled"])
            self.assertEqual(launch["extra_args"], ["--no-vsync", "--solo"])
            storage_root = Path(launch["env_overrides"]["GAME_STORAGE_ROOT"])
            self.assertTrue(storage_root.is_dir())
            self.assertEqual(storage_root.parent.parent, root / "tmp" / "captures")

    def test_shot_can_keep_vsync_for_a_heavy_capture(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            executable = canonical_native_executable(root)
            executable.parent.mkdir(parents=True)
            executable.touch()
            (root / "capture").mkdir()
            (root / "capture" / "catalog.json").write_text(json.dumps({
                "version": 1, "executable": capture_executable_relative(),
                "defaults": {"size": "1280x720", "fps": 30},
                "shots": {"crowd": {"seconds": 1, "no_vsync": False}},
            }), encoding="utf-8")
            game = FakeGame()
            game.process_id = 123
            launch = {}

            class GameContext:
                def __enter__(self):
                    return game

                def __exit__(self, *_args):
                    return None

            def fake_running_game(**kwargs):
                launch.update(kwargs)
                return GameContext()

            with patch("ai_studio.runtime_automation.capture_game.running_game", fake_running_game), patch(
                "ai_studio.runtime_automation.capture_game.record_native_take",
                return_value={"edit": root / "edit.mp4", "master": root / "master.mkv"},
            ), patch(
                "ai_studio.runtime_automation.capture_game.prepare_native_capture",
                return_value=native_build_record(executable),
            ), patch(
                "ai_studio.runtime_automation.capture_game.validate_native_capture_final",
            ):
                run(parse_args([str(root), "shot", "crowd"]))

            self.assertNotIn("--no-vsync", launch["extra_args"])

    def test_main_prints_only_the_edit_path_on_success(self) -> None:
        output = StringIO()
        with patch(
            "ai_studio.runtime_automation.capture_game.run",
            return_value={"edit": Path("C:/take/edit.mp4"), "master": Path("C:/take/master.mkv")},
        ), redirect_stdout(output):
            status = main(["."])

        self.assertEqual(status, 0)
        self.assertEqual(output.getvalue(), f"{Path('C:/take/edit.mp4')}\n")

if __name__ == "__main__":
    unittest.main()
