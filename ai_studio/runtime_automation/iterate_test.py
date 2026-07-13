import contextlib
import base64
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import iterate


FIXTURES = {"cFixture": "leaf-c-v1", "schemaFixture": "Template ready"}
GUARD = {
    "commit": "a" * 40,
    "scopedWorktree": "b" * 64,
    "worktreeScope": ["templates/template", "features", "ai_studio/runtime_automation", "ai_studio/studio.mjs"],
    "dirtyPaths": ["templates/template/src/iteration_proof_devapi.c"],
    "dirtyPathCount": 1,
    "engineGitlink": "c" * 40,
    "engineDirty": False,
}
ARTIFACTS = {"executable": "d" * 64, "pack": "e" * 64}
TOOLS = {
    "cmake": "cmake version 3.31.0",
    "buildTool": "1.12.1",
    "compiler": "clang version 19.1.0",
    "generator": "Ninja",
    "compilerPath": "C:/LLVM/bin/clang.exe",
}


class FakeGame:
    def __init__(self, proof=None, methods=None, process_id=321):
        self.proof = proof or dict(FIXTURES)
        self.methods = methods or ["game.iteration.proof"]
        self.calls = []
        self.closed = False
        self.next_request_id = 1
        self.process_id = process_id

    def request(self, method, params=None):
        params = params or {}
        self.calls.append((method, params))
        self.next_request_id += 1
        if method == "endpoints":
            return {"ok": True, "result": {"commands": [{"method": item} for item in self.methods]}}
        if method == "game.iteration.proof":
            return {"ok": True, "result": dict(self.proof)}
        if method == "frame.wait":
            return {"ok": True, "result": {"frame": 1}}
        if method == "capture.frame":
            png = b"\x89PNG\r\n\x1a\nproof"
            return {"ok": True, "result": {
                "width": 1, "height": 1, "format": "png",
                "data": base64.b64encode(png).decode("ascii"),
            }}
        raise AssertionError(f"unexpected DevAPI call: {method}")

    def close(self):
        self.closed = True


def process_runner(calls):
    def run(command, phase):
        calls.append((phase, command))
        return iterate.ToolResult(duration_ms=2.0, output_bytes=7, stdout="ok\n")
    return run


def fresh_patches(*, fixtures=None, guards=None, artifacts=None, tools=None, inventory=None):
    fixture_values = iter(fixtures or [FIXTURES, FIXTURES, FIXTURES])
    guard_values = iter(guards or [GUARD, GUARD, GUARD])
    artifact_values = iter(artifacts or [ARTIFACTS, ARTIFACTS, ARTIFACTS])
    return mock.patch.multiple(
        iterate,
        read_expected_fixtures=mock.Mock(side_effect=lambda: dict(next(fixture_values))),
        read_consistency_guard=mock.Mock(side_effect=lambda: dict(next(guard_values))),
        hash_claimed_artifacts=mock.Mock(side_effect=lambda: dict(next(artifact_values))),
        cache_compatibility=mock.Mock(return_value="compatible"),
        collect_tool_metadata=mock.Mock(return_value=dict(tools or TOOLS)),
        read_ninja_inventory=mock.Mock(return_value=inventory or {"status": "available", "rebuiltFiles": [], "rebuiltTargets": []}),
        ninja_log_offset=mock.Mock(return_value=10),
        pick_free_port=mock.Mock(return_value=19001),
    )


class IterateContractTest(unittest.TestCase):
    def test_scoped_snapshot_hashes_git_binary_diff_and_sorted_nonignored_untracked_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "new.bin").write_bytes(b"first")
            git_outputs = [b"binary-diff", b"tracked.c\0", b"new.bin\0"]
            with mock.patch.object(iterate, "ROOT_PATH", root), mock.patch.object(
                iterate, "SCOPED_GUARD_PATHS", ("scope",)
            ), mock.patch.object(iterate, "_run_git_bytes", side_effect=git_outputs) as run_git:
                first = iterate._scoped_worktree_snapshot()

            (root / "new.bin").write_bytes(b"second")
            with mock.patch.object(iterate, "ROOT_PATH", root), mock.patch.object(
                iterate, "SCOPED_GUARD_PATHS", ("scope",)
            ), mock.patch.object(iterate, "_run_git_bytes", side_effect=git_outputs):
                second = iterate._scoped_worktree_snapshot()

        self.assertNotEqual(first["scopedWorktree"], second["scopedWorktree"])
        self.assertEqual(first["dirtyPaths"], ["new.bin", "tracked.c"])
        self.assertEqual(first["dirtyPathCount"], 2)
        self.assertEqual(first["worktreeScope"], ["scope"])
        self.assertEqual(run_git.call_args_list[2].args[0][:3], ["ls-files", "--others", "--exclude-standard"])

    def test_fixture_parser_reads_leaf_c_and_generated_schema_defaults(self):
        with tempfile.TemporaryDirectory() as tmp:
            c_path = Path(tmp, "iteration_proof_devapi.c")
            schema_path = Path(tmp, "game_state.schema.json")
            c_path.write_text('#define GAME_ITERATION_C_FIXTURE "leaf-c-v7"\n', encoding="utf-8")
            schema_path.write_text(json.dumps({"fields": {"test_label_text": {"default": "Schema v7"}}}), encoding="utf-8")

            self.assertEqual(iterate.read_expected_fixtures(c_path, schema_path), {
                "cFixture": "leaf-c-v7",
                "schemaFixture": "Schema v7",
            })

    def test_proof_exact_compare_rejects_either_fixture_mismatch(self):
        self.assertIsNone(iterate.require_exact_proof(dict(FIXTURES), dict(FIXTURES)))
        with self.assertRaisesRegex(iterate.IterationError, "cFixture"):
            iterate.require_exact_proof({**FIXTURES, "cFixture": "stale"}, FIXTURES)
        with self.assertRaisesRegex(iterate.IterationError, "schemaFixture"):
            iterate.require_exact_proof({**FIXTURES, "schemaFixture": "stale"}, FIXTURES)

    def test_build_is_unconditional_and_precedes_launch(self):
        events = []
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            events.append(("launch", kwargs))
            yield game

        with fresh_patches():
            result = iterate.execute_iteration(
                process_runner=process_runner(events), game_launcher=launcher, capture=False
            )

        self.assertEqual(result["status"], "passed")
        self.assertEqual(events[0], ("compile", iterate.canonical_build_command()))
        self.assertEqual(events[1][0], "launch")
        self.assertEqual(events[1][1]["port"], 19001)
        self.assertEqual(result["runtime"], {"port": 19001, "processId": 321})

    def test_proof_mismatch_blocks_capture(self):
        game = FakeGame(proof={**FIXTURES, "cFixture": "stale"})

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches():
            result = iterate.execute_iteration(
                process_runner=process_runner([]), game_launcher=launcher, capture=True
            )

        self.assertEqual(result["status"], "failed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(result["error"]["phase"], "semanticProof")
        self.assertFalse(any(method == "capture.frame" for method, _ in game.calls))

    def test_fixture_race_before_capture_blocks_capture(self):
        changed = {**FIXTURES, "schemaFixture": "changed"}
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches(fixtures=[FIXTURES, changed]):
            result = iterate.execute_iteration(
                process_runner=process_runner([]), game_launcher=launcher, capture=True
            )

        self.assertEqual(result["status"], "failed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(result["error"]["phase"], "captureConsistency")
        self.assertFalse(any(method == "capture.frame" for method, _ in game.calls))

    def test_fixture_race_after_capture_marks_capture_invalid(self):
        changed = {**FIXTURES, "cFixture": "changed"}
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches(fixtures=[FIXTURES, FIXTURES, changed]):
            result = iterate.execute_iteration(
                process_runner=process_runner([]), game_launcher=launcher, capture=True
            )

        self.assertEqual(result["status"], "failed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(result["artifacts"]["screenshot"]["status"], "invalidated-race")

    def test_foreign_cache_roots_request_supported_fresh_configure(self):
        with tempfile.TemporaryDirectory() as tmp:
            cache = Path(tmp, "CMakeCache.txt")
            cache.write_text("\n".join([
                "CMAKE_HOME_DIRECTORY:INTERNAL=C:/foreign/source",
                "CMAKE_CACHEFILE_DIR:INTERNAL=C:/foreign/build",
                "GAME_DEVAPI_ENABLED:BOOL=ON",
                "CMAKE_GENERATOR:INTERNAL=Ninja",
                "CMAKE_BUILD_TYPE:STRING=Debug",
                "CMAKE_C_COMPILER:FILEPATH=clang",
                "CMAKE_CXX_COMPILER:FILEPATH=clang++",
            ]), encoding="utf-8")
            self.assertEqual(iterate.cache_compatibility(cache, Path(tmp, "source"), Path(tmp, "build")), "foreign")
        self.assertIn("--fresh", iterate.canonical_configure_command(fresh=True))

    def test_cache_compatibility_matrix(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp, "source").resolve()
            build = Path(tmp, "build").resolve()
            build.mkdir()
            cache = build / "CMakeCache.txt"

            def write(**overrides):
                values = {
                    "CMAKE_HOME_DIRECTORY": str(source),
                    "CMAKE_CACHEFILE_DIR": str(build),
                    "GAME_DEVAPI_ENABLED": "ON",
                    "CMAKE_GENERATOR": "Ninja",
                    "CMAKE_BUILD_TYPE": "Debug",
                    "CMAKE_C_COMPILER": "clang",
                    "CMAKE_CXX_COMPILER": "clang++",
                    "CMAKE_EXE_LINKER_FLAGS_DEBUG": "-fsanitize=address,undefined",
                }
                values.update(overrides)
                cache.write_text("\n".join(f"{key}:STRING={value}" for key, value in values.items()), encoding="utf-8")

            self.assertEqual(iterate.cache_compatibility(cache, source, build), "missing")
            write()
            self.assertEqual(iterate.cache_compatibility(cache, source, build, platform_name="win32"), "compatible")
            for overrides in (
                {"CMAKE_GENERATOR": "Visual Studio 17 2022"},
                {"GAME_DEVAPI_ENABLED": "OFF"},
                {"CMAKE_C_COMPILER": "cl.exe"},
                {"CMAKE_CXX_COMPILER": "cl.exe"},
                {"CMAKE_BUILD_TYPE": "Release"},
            ):
                write(**overrides)
                self.assertEqual(iterate.cache_compatibility(cache, source, build, platform_name="win32"), "incompatible")
            write(CMAKE_EXE_LINKER_FLAGS_DEBUG="")
            self.assertEqual(iterate.cache_compatibility(cache, source, build, platform_name="linux"), "incompatible")
            write(CMAKE_HOME_DIRECTORY=str(Path(tmp, "foreign")))
            self.assertEqual(iterate.cache_compatibility(cache, source, build, platform_name="win32"), "foreign")

    def test_process_failure_returns_bounded_structured_json(self):
        def fail_build(command, phase):
            raise iterate.ProcessError(phase, 7, "x" * 10000)

        with fresh_patches():
            result = iterate.execute_iteration(process_runner=fail_build, capture=False)

        self.assertEqual(result["status"], "failed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(result["phases"]["compile"]["status"], "failed")
        self.assertIsInstance(result["phases"]["compile"]["wallMs"], float)
        self.assertLessEqual(len(result["error"]["tail"]), iterate.ERROR_TAIL_LIMIT)
        json.dumps(result)

    def test_port_resolution_failure_is_structured(self):
        with mock.patch.object(iterate, "pick_free_port", side_effect=OSError("no ports")):
            result = iterate.execute_iteration(capture=False)
        self.assertEqual(result["status"], "failed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(result["error"]["phase"], "validation")
        json.dumps(result)

    def test_metadata_failure_marks_metadata_only(self):
        with fresh_patches(), mock.patch.object(
            iterate, "collect_tool_metadata", side_effect=iterate.ProcessError("metadata", 9, "version failed", 1.0)
        ):
            result = iterate.execute_iteration(process_runner=process_runner([]), capture=False)
        self.assertEqual(result["status"], "failed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(result["phases"]["metadata"]["status"], "failed")
        self.assertNotEqual(result["phases"]["validation"]["status"], "failed")
        self.assertNotEqual(result["phases"]["configure"]["status"], "failed")

    def test_compatible_cache_leaves_configure_command_null(self):
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches():
            result = iterate.execute_iteration(process_runner=process_runner([]), game_launcher=launcher, capture=False)
        self.assertIsNone(result["commandPath"]["configure"])

    def test_incompatible_cache_uses_fresh_configure(self):
        calls = []
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches(), mock.patch.object(iterate, "cache_compatibility", return_value="incompatible"):
            result = iterate.execute_iteration(process_runner=process_runner(calls), game_launcher=launcher, capture=False)
        configure = next(command for phase, command in calls if phase == "configure")
        self.assertIn("--fresh", configure)
        self.assertEqual(result["commandPath"]["configure"], configure)

    def test_preexisting_executable_and_changed_both_fixtures_still_builds_and_exact_proves(self):
        baseline = {"cFixture": "leaf-c-v1", "schemaFixture": "Schema v1"}
        changed = {"cFixture": "leaf-c-v2", "schemaFixture": "Schema v2"}
        calls = []
        parse_fixtures = iterate.read_expected_fixtures

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            executable = root / ("game.exe" if os.name == "nt" else "game")
            c_path = root / "iteration_proof_devapi.c"
            schema_path = root / "game_state.schema.json"

            def write_fixtures(values):
                c_path.write_text(
                    f'#define GAME_ITERATION_C_FIXTURE "{values["cFixture"]}"\n', encoding="utf-8"
                )
                schema_path.write_text(json.dumps({
                    "fields": {"test_label_text": {"default": values["schemaFixture"]}}
                }), encoding="utf-8")

            write_fixtures(baseline)
            executable.write_bytes(b"pre-existing-stale-binary")
            preexisting_bytes = executable.read_bytes()
            games = iter([FakeGame(proof=baseline), FakeGame(proof=changed)])

            def build_current_fixtures(command, phase):
                calls.append((phase, command))
                if phase == "compile":
                    fixtures = parse_fixtures(c_path, schema_path)
                    executable.write_bytes(json.dumps(fixtures, sort_keys=True).encode("utf-8"))
                return iterate.ToolResult(duration_ms=2.0, output_bytes=7, stdout="ok\n")

            @contextlib.contextmanager
            def launcher(**kwargs):
                yield next(games)

            with mock.patch.multiple(
                iterate,
                NATIVE_DEVAPI_EXE=executable,
                read_expected_fixtures=mock.Mock(
                    side_effect=lambda: parse_fixtures(c_path, schema_path)
                ),
                read_consistency_guard=mock.Mock(side_effect=lambda: dict(GUARD)),
                hash_claimed_artifacts=mock.Mock(return_value=dict(ARTIFACTS)),
                cache_compatibility=mock.Mock(return_value="compatible"),
                collect_tool_metadata=mock.Mock(return_value=dict(TOOLS)),
                read_ninja_inventory=mock.Mock(return_value={
                    "status": "available", "rebuiltFiles": [], "rebuiltTargets": []
                }),
                ninja_log_offset=mock.Mock(return_value=10),
                pick_free_port=mock.Mock(side_effect=[19001, 19002]),
            ):
                first = iterate.execute_iteration(
                    executable=executable, process_runner=build_current_fixtures,
                    game_launcher=launcher, capture=False,
                )
                first_built_bytes = executable.read_bytes()
                write_fixtures(changed)
                second = iterate.execute_iteration(
                    executable=executable, process_runner=build_current_fixtures,
                    game_launcher=launcher, capture=False,
                )
                second_built_bytes = executable.read_bytes()

        self.assertEqual(first["status"], "passed")
        self.assertEqual(second["status"], "passed")
        self.assertNotEqual(preexisting_bytes, first_built_bytes)
        self.assertNotEqual(first_built_bytes, second_built_bytes)
        self.assertEqual(
            [call for call in calls if call == ("compile", iterate.canonical_build_command())],
            [("compile", iterate.canonical_build_command())] * 2,
        )
        self.assertEqual(first["proof"]["expected"], baseline)
        self.assertEqual(second["proof"]["expected"], changed)
        self.assertEqual(second["proof"]["actual"], changed)

    def test_missing_fresh_process_pid_fails_before_readiness(self):
        game = FakeGame(process_id=None)

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches():
            result = iterate.execute_iteration(process_runner=process_runner([]), game_launcher=launcher, capture=False)
        self.assertEqual(result["status"], "failed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(result["error"]["phase"], "launchToDevapiReady")
        self.assertEqual(game.calls, [])

    def test_engine_dirty_change_invalidates_same_guard(self):
        dirty = {**GUARD, "engineDirty": True}
        self.assertFalse(iterate._same_guard(GUARD, dirty))

    def test_already_dirty_mutation_add_and_delete_are_races(self):
        mutated = {**GUARD, "scopedWorktree": "f" * 64}
        added = {**GUARD, "scopedWorktree": "1" * 64, "dirtyPaths": [*GUARD["dirtyPaths"], "features/new.c"], "dirtyPathCount": 2}
        deleted = {**GUARD, "scopedWorktree": "2" * 64, "dirtyPaths": [], "dirtyPathCount": 0}
        for changed in (mutated, added, deleted):
            with self.subTest(changed=changed["dirtyPathCount"]):
                game = FakeGame()

                @contextlib.contextmanager
                def launcher(**kwargs):
                    yield game

                with fresh_patches(guards=[GUARD, changed]):
                    result = iterate.execute_iteration(process_runner=process_runner([]), game_launcher=launcher, capture=True)
                self.assertEqual(result["status"], "failed")
                self.assertFalse(result["freshnessClaim"])
                self.assertFalse(any(method == "capture.frame" for method, _ in game.calls))

    def test_devapi_metrics_count_exact_requests_and_normalized_bytes(self):
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches():
            result = iterate.execute_iteration(
                process_runner=process_runner([]), game_launcher=launcher, capture=False
            )

        self.assertEqual(result["devapiMetrics"]["requests"], 2)
        self.assertGreater(result["devapiMetrics"]["requestBytes"], 0)
        self.assertGreater(result["devapiMetrics"]["normalizedResponseBytes"], 0)

    def test_capture_requests_are_included_in_devapi_metrics(self):
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        with fresh_patches():
            result = iterate.execute_iteration(
                process_runner=process_runner([]), game_launcher=launcher, capture=True
            )
        self.assertEqual(result["devapiMetrics"]["requests"], 4)
        self.assertEqual([method for method, _ in game.calls], [
            "endpoints", "game.iteration.proof", "frame.wait", "capture.frame",
        ])

    def test_reuse_excludes_fingerprint_fixtures_build_launch_proof_and_capture(self):
        game = FakeGame(methods=["game.iteration.proof"])
        with mock.patch.object(iterate, "read_consistency_guard") as guard, \
             mock.patch.object(iterate, "read_expected_fixtures") as fixtures, \
             mock.patch.object(iterate, "hash_claimed_artifacts") as artifacts:
            result = iterate.execute_iteration(
                reuse=True,
                port=19000,
                attach_client=lambda **kwargs: game,
                process_runner=lambda *args: self.fail("reuse must not run tools"),
                game_launcher=lambda **kwargs: self.fail("reuse must not launch"),
                capture=True,
            )

        guard.assert_not_called()
        fixtures.assert_not_called()
        artifacts.assert_not_called()
        self.assertEqual(result["status"], "passed")
        self.assertFalse(result["freshnessClaim"])
        self.assertEqual(game.calls, [("endpoints", {})])
        self.assertEqual(result["proof"]["status"], "skipped")
        self.assertEqual(result["runtime"], {"port": 19000, "processId": None})

    def test_exact_metadata_records_guards_engine_artifacts_tools_and_inventory(self):
        game = FakeGame()

        @contextlib.contextmanager
        def launcher(**kwargs):
            yield game

        inventory = {"status": "available", "rebuiltFiles": ["bin/game.exe"], "rebuiltTargets": ["game"]}
        with fresh_patches(inventory=inventory):
            result = iterate.execute_iteration(
                process_runner=process_runner([]), game_launcher=launcher, capture=False
            )

        self.assertEqual(result["consistency"]["start"], GUARD)
        self.assertEqual(result["consistency"]["end"], GUARD)
        self.assertEqual(result["artifactHashes"]["afterBuild"], ARTIFACTS)
        self.assertEqual(result["artifactHashes"]["afterProof"], ARTIFACTS)
        self.assertEqual(result["environment"]["tools"], TOOLS)
        self.assertEqual(result["build"], inventory)


if __name__ == "__main__":
    unittest.main()
