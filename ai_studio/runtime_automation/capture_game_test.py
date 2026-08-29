from pathlib import Path
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
)


class CaptureGameCliTest(unittest.TestCase):
    def test_catalog_controls_live_capture_defaults(self) -> None:
        args = parse_args(["games/private/example"])

        self.assertEqual(args.game, Path("games/private/example"))
        self.assertEqual(args.mode, "live")
        self.assertIsNone(args.preset)
        self.assertIsNone(args.seconds)
        self.assertEqual(args.countdown, 0)

    def test_resolves_the_standard_debug_executable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "build" / "devapi-debug" / "bin" / "game.exe"
            executable.parent.mkdir(parents=True)
            executable.touch()

            self.assertEqual(resolve_game_executable(root), executable.resolve())

    def test_missing_executable_reports_the_build_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            with self.assertRaisesRegex(RuntimeError, r"build[\\/]devapi-debug"):
                resolve_game_executable(root)

    def test_parses_a_deterministic_shot_command(self) -> None:
        args = parse_args(["games/private/example", "shot", "intro"])

        self.assertEqual(args.mode, "shot")
        self.assertEqual(args.shot_id, "intro")

    def test_loads_the_game_owned_capture_catalog(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "executable": "build/devapi-debug/bin/game.exe",
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
            root = Path(directory)
            executable = root / "out" / "game.exe"
            executable.parent.mkdir(parents=True)
            executable.touch()
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps(
                    {
                        "version": 1,
                        "executable": "out/game.exe",
                        "defaults": {
                            "preset": "landscape",
                            "size": "1280x720",
                            "fps": 30,
                            "ticks_per_frame": 2,
                        },
                        "shots": {"intro": {"seconds": 0.1, "preset": "social", "size": "1080x1920", "fps": 60, "ticks_per_frame": 1, "setup": [{"method": "game.planet.stage_intro"}], "events": []}},
                    }
                ),
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

            def fake_record_take(**kwargs):
                recorded.update(kwargs)
                kwargs["recording_prepare"]()
                recorded["prepare_end"] = len(game.calls)
                kwargs["recording_driver"]()
                return {"edit": root / "edit.mp4", "master": root / "master.mp4"}

            with patch(
                "ai_studio.runtime_automation.capture_game.running_game",
                fake_running_game,
            ), patch(
                "ai_studio.runtime_automation.capture_game.record_take",
                fake_record_take,
            ):
                result = run(parse_args([str(root), "shot", "intro"]))

        self.assertEqual(result["edit"].name, "edit.mp4")
        self.assertEqual(launch["exe"], str(executable.resolve()))
        self.assertEqual(launch["window_size"], "1080x1920")
        self.assertEqual(recorded["duration_seconds"], 0.1)
        self.assertEqual(recorded["countdown"], 0)
        self.assertTrue(recorded["hide_game_window"])
        self.assertTrue(callable(recorded["recording_prepare"]))
        self.assertTrue(callable(recorded["recording_driver"]))
        self.assertEqual(recorded["max_freeze_seconds"], 2.0)
        self.assertEqual(recorded["settings"].fps, 60)

        prepare_end = recorded["prepare_end"]
        self.assertEqual(game.calls[:prepare_end], [
            ("time.set_fps", {"fps": 60}),
            ("time.set_scale", {"scale": 1.0}),
            ("time.set_mode", {"mode": "manual"}),
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
        shot = {
            "seconds": 0.1,
            "warmup_ticks": 2,
            "setup": [{"method": "game.planet.stage_intro"}],
            "events": [],
        }

        from ai_studio.runtime_automation.capture_game import cleanup_shot, prepare_shot

        prepare_shot(game, shot)
        prepared = list(game.calls)
        play_timeline(game, shot, output_fps=30)
        after_driver = list(game.calls)
        cleanup_shot(game)

        self.assertNotIn(("input.set_player_enabled", {"enabled": True}), after_driver)
        self.assertIn(("time.set_mode", {"mode": "run"}), after_driver)
        self.assertEqual(prepared, [
            ("time.set_fps", {"fps": 60}),
            ("time.set_scale", {"scale": 1.0}),
            ("time.set_mode", {"mode": "manual"}),
            ("input.set_player_enabled", {"enabled": False}),
            ("game.planet.stage_intro", {}),
            ("time.step", {"count": 2}),
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

class CaptureGameAdoptionTest(unittest.TestCase):
    def test_catalog_rejects_invalid_shots_before_a_game_is_launched(self) -> None:
        with self.assertRaisesRegex(ValueError, "catalog shots must be an object"):
            validate_catalog({"version": 1, "shots": []})

    def test_missing_shot_lists_available_ids_before_build_lookup(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
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
            root = Path(directory)
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
            root = Path(directory)
            first = default_output_root(root)
            second = default_output_root(root)

        self.assertNotEqual(first, second)
        self.assertEqual(first.parent, root / "tmp" / "captures")

    def test_studio_root_resolution_supports_template_and_private_games(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ai_studio" / "runtime_automation").mkdir(parents=True)
            (root / "ai_studio" / "runtime_automation" / "capture_game.py").touch()
            self.assertEqual(resolve_studio_root(root / "templates" / "template"), root)
            self.assertEqual(resolve_studio_root(root / "games" / "private" / "example-game"), root)
            wrapper = (Path(__file__).parents[2] / "templates" / "template" / "capture.cmd").read_text(encoding="ascii")
            self.assertIn(":find_studio", wrapper)
            self.assertIn("ai_studio\\dev_environment\\python_run.mjs", wrapper)
            self.assertNotIn("py -3.12", wrapper)

    def test_live_uses_saved_state_autosave_and_fifteen_second_default(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root / "out" / "game.exe"
            executable.parent.mkdir(parents=True)
            executable.touch()
            capture = root / "capture"
            capture.mkdir()
            (capture / "catalog.json").write_text(
                json.dumps({"version": 1, "executable": "out/game.exe", "defaults": {"size": "1280x720", "fps": 30}, "live": {"seconds": 15}, "shots": {}}),
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

            def fake_record_take(**kwargs):
                print("backend progress")
                recorded.update(kwargs)
                return {"edit": root / "edit.mp4", "master": root / "master.mkv"}

            with patch("ai_studio.runtime_automation.capture_game.running_game", fake_running_game), patch(
                "ai_studio.runtime_automation.capture_game.record_take", fake_record_take
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
        self.assertNotIn("max_freeze_seconds", recorded)
        self.assertEqual(recorded["duration_seconds"], 4.0)

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
