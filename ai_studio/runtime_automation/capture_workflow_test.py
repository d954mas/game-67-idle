import json
import tempfile
import unittest
from pathlib import Path

from capture_scenario import parse_scenario
from capture_workflow import (
    CaptureWorkflowError,
    build_parser,
    evaluate_shot_safe_area,
    load_catalog,
    play_scenario_realtime,
    prepare_live,
    prepare_scenario,
    publish_take,
    record_with_transient_retry,
    reset_scenario_for_recording,
)


def scenario_document(*, duration_frames=2, output_fps=30):
    return {
        "schema": "ai_studio.capture_scenario",
        "version": 1,
        "api_version": 1,
        "game": "example-game",
        "scene": {"id": "showcase", "contract_version": 1, "seed": 7},
        "viewport": {
            "orientation": "vertical",
            "output_width": 1080,
            "output_height": 1920,
            "min_framebuffer_width": 1080,
            "min_framebuffer_height": 1920,
            "prefer_supersample": True,
        },
        "clock": {
            "fixed_tick_hz": 60,
            "output_fps": output_fps,
            "ticks_per_output_frame": 60 // output_fps,
            "warmup_ticks": 3,
            "duration_frames": duration_frames,
        },
        "events": [
            {
                "frame": 0,
                "set": {"parameter": "population", "value": 100},
            },
            {
                "frame": 1,
                "action": {"id": "wave", "arguments": {}},
            },
        ],
        "ramps": [],
        "evidence": {
            "boundary_radius_frames": 1,
            "uniform_contact_sheet_samples": 2,
        },
    }


def catalog_document(*, policy_status="incomplete"):
    return {
        "schema": "ai_studio.game_capture_catalog",
        "version": 1,
        "game": "example-game",
        "executable": "build/devapi-debug/bin/game.exe",
        "live": {
            "purpose": "Manual gameplay",
            "duration_seconds": 30,
            "angle": "Player-controlled camera",
            "preset": "landscape",
            "scene_shot": "showcase",
        },
        "safe_area": {
            "id": "universal-social-v1",
            "policy_status": policy_status,
            "normalized_dimensions": {"width": 10, "height": 10},
            "guide_unsafe_rectangles": [
                [0.0, 0.0, 0.1, 1.0],
                [0.9, 0.0, 1.0, 1.0],
            ],
        },
        "shots": [
            {
                "id": "showcase",
                "purpose": "Show the core interaction",
                "duration_seconds": 2 / 30,
                "angle": "Fixed portrait three-quarter view",
                "preset": "social",
                "scenario": "devapi/capture_scenarios/showcase.v1.json",
                "critical_regions": [
                    {"id": "hero", "rectangle": [0.2, 0.2, 0.8, 0.8]}
                ],
            }
        ],
    }


class CatalogTest(unittest.TestCase):
    def test_incomplete_obs_container_gets_one_clean_local_retry(self):
        roots = []

        def record(root):
            roots.append(root)
            if len(roots) == 1:
                raise RuntimeError(
                    "AV_VALIDATION_FAILED: incomplete ffprobe metadata: 'duration'"
                )
            return {"status": "captured"}

        root, result = record_with_transient_retry(Path("take/.recorder"), record)

        self.assertEqual(root, Path("take/.recorder-retry-2"))
        self.assertEqual(result["status"], "captured")
        self.assertEqual(
            roots,
            [Path("take/.recorder"), Path("take/.recorder-retry-2")],
        )

    def test_unhealthy_obs_source_gets_one_clean_local_retry(self):
        calls = 0

        def record(root):
            nonlocal calls
            calls += 1
            if calls == 1:
                raise RuntimeError("OBS window source stayed unhealthy: black")
            return {"status": "captured"}

        root, result = record_with_transient_retry(Path("take/.recorder"), record)

        self.assertEqual(root, Path("take/.recorder-retry-2"))
        self.assertEqual(result["status"], "captured")

    def test_load_catalog_resolves_approved_shot_and_checks_duration(self):
        with tempfile.TemporaryDirectory() as directory:
            game_root = Path(directory)
            scenario_path = (
                game_root / "devapi" / "capture_scenarios" / "showcase.v1.json"
            )
            scenario_path.parent.mkdir(parents=True)
            scenario_path.write_text(
                json.dumps(scenario_document()), encoding="utf-8"
            )
            (game_root / "capture").mkdir()
            (game_root / "capture" / "catalog.json").write_text(
                json.dumps(catalog_document()), encoding="utf-8"
            )

            catalog = load_catalog(game_root)

            self.assertEqual(catalog.game, "example-game")
            self.assertEqual(catalog.shot("showcase").purpose, "Show the core interaction")
            self.assertEqual(catalog.shot("showcase").scenario.duration_frames, 2)

    def test_load_catalog_rejects_scenario_paths_outside_the_game(self):
        with tempfile.TemporaryDirectory() as directory:
            game_root = Path(directory)
            document = catalog_document()
            document["shots"][0]["scenario"] = "../outside.json"
            (game_root / "capture").mkdir()
            (game_root / "capture" / "catalog.json").write_text(
                json.dumps(document), encoding="utf-8"
            )

            with self.assertRaisesRegex(CaptureWorkflowError, "inside the game"):
                load_catalog(game_root)


class SafeAreaTest(unittest.TestCase):
    def test_incomplete_universal_policy_can_only_return_guidance(self):
        scenario = parse_scenario(scenario_document())
        document = catalog_document(policy_status="incomplete")

        result = evaluate_shot_safe_area(
            document["safe_area"],
            document["shots"][0]["critical_regions"],
        )

        self.assertEqual(result["geometryStatus"], "pass")
        self.assertEqual(result["status"], "guidance_only")
        self.assertFalse(result["masterEligible"])

    def test_ready_policy_rejects_critical_content_in_unsafe_region(self):
        scenario = parse_scenario(scenario_document())
        document = catalog_document(policy_status="official")
        regions = [{"id": "hero", "rectangle": [0.85, 0.2, 0.95, 0.8]}]

        result = evaluate_shot_safe_area(document["safe_area"], regions)

        self.assertEqual(result["status"], "fail")
        self.assertFalse(result["masterEligible"])


class ScenarioPlaybackTest(unittest.TestCase):
    def test_live_preflight_loads_approved_scene_without_running_its_timeline(self):
        scenario = parse_scenario(scenario_document())

        class FakeGame:
            def __init__(self):
                self.calls = []

            def endpoint_methods(self):
                return {
                    f"game.capture_scene.{name}"
                    for name in (
                        "list",
                        "describe",
                        "load",
                        "set_parameter",
                        "trigger_action",
                        "status",
                    )
                }

            def result(self, method, params=None):
                self.calls.append((method, params))
                if method == "command.describe":
                    return {"method": params["method"]}
                if method == "game.capture_scene.list":
                    return {
                        "apiVersion": 1,
                        "gameId": "example-game",
                        "scenes": [{"id": "showcase"}],
                    }
                if method == "game.capture_scene.describe":
                    return {
                        "apiVersion": 1,
                        "gameId": "example-game",
                        "scene": {
                            "id": "showcase",
                            "contractVersion": 1,
                            "parameters": [
                                {
                                    "id": "population",
                                    "type": "float",
                                    "minimum": 0,
                                    "maximum": 1000,
                                }
                            ],
                            "actions": [{"id": "wave"}],
                        },
                    }
                if method == "game.capture_scene.status":
                    return {"ready": True, "tick": 3}
                return {}

        game = FakeGame()
        prepare_live(game, scenario)

        self.assertIn(
            (
                "game.capture_scene.set_parameter",
                {
                    "scene": "showcase",
                    "parameter": "population",
                    "value": 100,
                },
            ),
            game.calls,
        )
        self.assertNotIn(
            (
                "game.capture_scene.trigger_action",
                {"scene": "showcase", "action": "wave", "arguments": {}},
            ),
            game.calls,
        )

    def test_prepare_discovers_describes_loads_and_warms_the_game_scene(self):
        scenario = parse_scenario(scenario_document())

        class FakeGame:
            def __init__(self):
                self.calls = []

            def endpoint_methods(self):
                return {
                    f"game.capture_scene.{name}"
                    for name in (
                        "list",
                        "describe",
                        "load",
                        "set_parameter",
                        "trigger_action",
                        "status",
                    )
                }

            def result(self, method, params=None):
                self.calls.append((method, params))
                if method == "command.describe":
                    return {"method": params["method"]}
                if method == "game.capture_scene.list":
                    return {
                        "apiVersion": 1,
                        "gameId": "example-game",
                        "scenes": [{"id": "showcase"}],
                    }
                if method == "game.capture_scene.describe":
                    return {
                        "apiVersion": 1,
                        "gameId": "example-game",
                        "scene": {
                            "id": "showcase",
                            "contractVersion": 1,
                            "parameters": [
                                {
                                    "id": "population",
                                    "type": "float",
                                    "minimum": 0,
                                    "maximum": 1000,
                                }
                            ],
                            "actions": [{"id": "wave"}],
                        },
                    }
                if method == "game.capture_scene.status":
                    return {"ready": True, "tick": 3}
                return {}

        game = FakeGame()
        result = prepare_scenario(game, scenario)

        self.assertEqual(result["ready"], True)
        self.assertIn(("game.capture_scene.list", None), game.calls)
        self.assertIn(
            ("game.capture_scene.describe", {"scene": "showcase"}),
            game.calls,
        )
        self.assertIn(("time.step", {"count": 3}), game.calls)
        self.assertEqual(
            game.calls[-1], ("time.set_mode", {"mode": "run"})
        )

    def test_recording_reset_reestablishes_seeded_tick_zero_at_rec(self):
        scenario = parse_scenario(scenario_document())

        class FakeGame:
            def __init__(self):
                self.calls = []

            def result(self, method, params=None):
                self.calls.append((method, params))
                if method == "game.capture_scene.status":
                    return {"ready": True, "tick": 3}
                return {}

        game = FakeGame()
        result = reset_scenario_for_recording(game, scenario)

        self.assertEqual(
            game.calls[0], ("time.set_mode", {"mode": "manual"})
        )
        self.assertIn(
            (
                "game.capture_scene.reset",
                {"scene": "showcase", "seed": 7},
            ),
            game.calls,
        )
        self.assertIn(("time.step", {"count": 3}), game.calls)
        self.assertTrue(result["ready"])

    def test_events_are_applied_before_each_fixed_time_step(self):
        scenario = parse_scenario(scenario_document())

        class FakeGame:
            def __init__(self):
                self.calls = []

            def result(self, method, params=None):
                self.calls.append((method, params))
                if method == "game.capture_scene.status":
                    return {"ready": True, "tick": 7}
                return {"status": {"ready": True}}

        game = FakeGame()
        now = [0.0]

        def monotonic():
            return now[0]

        def sleep(seconds):
            now[0] += seconds

        result = play_scenario_realtime(
            game, scenario, monotonic=monotonic, sleep=sleep
        )

        methods = [method for method, _ in game.calls]
        self.assertLess(
            methods.index("game.capture_scene.set_parameter"),
            methods.index("time.step"),
        )
        self.assertLess(
            methods.index("game.capture_scene.trigger_action"),
            len(methods) - 1 - methods[::-1].index("time.step"),
        )
        self.assertEqual(result["status"], "completed")
        self.assertEqual(result["frames"], 2)


class PublicationTest(unittest.TestCase):
    def _recorder_output(self, root):
        root.mkdir()
        (root / "master.mkv").write_bytes(b"video+audio")
        (root / "edit.mp4").write_bytes(b"edit")
        (root / "capture.json").write_text(
            json.dumps({"status": "captured"}), encoding="utf-8"
        )
        frame = root / "frame.png"
        frame.write_bytes(b"png")
        return frame

    def test_incomplete_safe_area_publishes_draft_without_master(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recorder = root / "recorder"
            frame = self._recorder_output(recorder)

            result = publish_take(
                recorder,
                root / "take",
                representative_frame=frame,
                workflow={
                    "mode": "shot",
                    "scenarioStatus": "completed",
                    "safeArea": {
                        "status": "guidance_only",
                        "masterEligible": False,
                    },
                },
            )

            self.assertEqual(result["classification"], "draft")
            self.assertTrue((root / "take" / "draft" / "recording.mkv").is_file())
            self.assertFalse((root / "take" / "master").exists())

    def test_passing_scripted_take_is_copied_to_immutable_master_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recorder = root / "recorder"
            frame = self._recorder_output(recorder)

            result = publish_take(
                recorder,
                root / "take",
                representative_frame=frame,
                workflow={
                    "mode": "shot",
                    "scenarioStatus": "completed",
                    "safeArea": {"status": "pass", "masterEligible": True},
                },
            )

            self.assertEqual(result["classification"], "master")
            self.assertTrue((root / "take" / "draft" / "recording.mkv").is_file())
            self.assertTrue((root / "take" / "master" / "recording.mkv").is_file())


class CliSurfaceTest(unittest.TestCase):
    def test_public_surface_is_only_live_and_shot_id(self):
        parser = build_parser()

        live = parser.parse_args(["--game-root", "game", "live"])
        shot = parser.parse_args(["--game-root", "game", "shot", "showcase"])

        self.assertEqual(live.command, "live")
        self.assertEqual(shot.command, "shot")
        self.assertEqual(shot.shot_id, "showcase")


if __name__ == "__main__":
    unittest.main()
