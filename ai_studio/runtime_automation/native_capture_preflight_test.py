import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from native_capture_preflight import (  # noqa: F401
    NativeCapturePreflightError,
    prepare_native_capture,
    validate_native_capture_final,
)


class NativeCapturePreflightTest(unittest.TestCase):
    @staticmethod
    def _cache_text(game: Path, build: Path, *, devapi="ON", repository: Path | None = None) -> str:
        rows = [
            f"CMAKE_CACHEFILE_DIR:INTERNAL={build.resolve()}",
            f"CMAKE_HOME_DIRECTORY:INTERNAL={game.resolve()}",
            f"GAME_DEVAPI_ENABLED:BOOL={devapi}",
            "CMAKE_GENERATOR:INTERNAL=Ninja",
            "CMAKE_BUILD_TYPE:STRING=Debug",
        ]
        if repository is not None:
            rows.append(f"GAME_REPO_ROOT:PATH={repository.resolve()}")
        return "\n".join(rows) + "\n"

    def _fixture(self):
        temporary_root = Path.cwd() / "tmp"
        temporary_root.mkdir(exist_ok=True)
        directory = tempfile.TemporaryDirectory(dir=temporary_root)
        root = Path(directory.name)
        game = root / "games" / "private" / "example"
        build = game / "build" / "devapi-debug"
        executable = build / "bin" / ("game.exe" if os.name == "nt" else "game")
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"old executable")
        (build / "CMakeCache.txt").write_text(self._cache_text(game, build), encoding="utf-8")
        output = game / "tmp" / "captures" / "take"
        output.mkdir(parents=True)
        return directory, root, game, executable, output

    @staticmethod
    def _runner(calls, fingerprint="a" * 64):
        def run(command, **_kwargs):
            calls.append(command)
            stdout = json.dumps({"fingerprint": fingerprint}) if command[0] == "node" else "built"
            return subprocess.CompletedProcess(command, 0, stdout=stdout, stderr="")
        return run

    def test_existing_executable_still_rebuilds_the_canonical_target(self):
        directory, root, game, executable, output = self._fixture()
        with directory:
            calls = []
            provenance = prepare_native_capture(
                game, output, studio_root=root, runner=self._runner(calls)
            )
            self.assertTrue((output / ".preflight" / "02-cmake-build.log").is_file())

        self.assertIn(["cmake", "--build", str(game / "build" / "devapi-debug"), "--target", "game"], calls)
        self.assertEqual(provenance["sourceFingerprint"], "a" * 64)
        self.assertEqual(provenance["executable"]["path"], str(executable.resolve()))

    def _declared_fixture(self, build_type="Release"):
        directory, root, game, _canonical, output = self._fixture()
        build = game / "build" / "native-devapi"
        executable = build / "bin" / ("plw_client.exe" if os.name == "nt" else "plw_client")
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"declared executable")
        (build / "CMakeCache.txt").write_text(
            self._cache_text(game, build).replace("Debug", build_type), encoding="utf-8"
        )
        return directory, root, game, build, executable, output

    def test_declared_artifact_rebuilds_its_own_target_and_build_type(self):
        directory, root, game, build, executable, output = self._declared_fixture()
        with directory:
            calls = []
            provenance = prepare_native_capture(
                game, output, requested_executable=executable, studio_root=root, runner=self._runner(calls)
            )
            validate_native_capture_final(provenance, runner=self._runner(calls))
            executable.write_bytes(b"changed executable")
            with self.assertRaisesRegex(NativeCapturePreflightError, "changed during recording"):
                validate_native_capture_final(provenance, runner=self._runner(calls))

        self.assertIn(["cmake", "--build", str(build), "--target", "plw_client"], calls)
        self.assertEqual(provenance["target"], "plw_client")
        self.assertEqual(provenance["executable"]["path"], str(executable.resolve()))
        self.assertEqual(provenance["cacheContract"]["buildType"], "Release")

    def test_declared_artifact_keeps_the_cache_contract(self):
        directory, root, game, build, executable, output = self._declared_fixture()
        with directory:
            cache = build / "CMakeCache.txt"
            cache.write_text(self._cache_text(game, build, devapi="OFF"), encoding="utf-8")
            with self.assertRaisesRegex(NativeCapturePreflightError, "GAME_DEVAPI_ENABLED"):
                prepare_native_capture(game, output, requested_executable=executable,
                                       studio_root=root, runner=self._runner([]))
            cache.write_text(self._cache_text(game, game / "build" / "devapi-debug"), encoding="utf-8")
            with self.assertRaisesRegex(NativeCapturePreflightError, "CMAKE_CACHEFILE_DIR"):
                prepare_native_capture(game, output, requested_executable=executable,
                                       studio_root=root, runner=self._runner([]))
            cache.write_text(self._cache_text(game, build).replace("CMAKE_BUILD_TYPE:STRING=Debug\n", ""),
                             encoding="utf-8")
            with self.assertRaisesRegex(NativeCapturePreflightError, "explicit CMAKE_BUILD_TYPE"):
                prepare_native_capture(game, output, requested_executable=executable,
                                       studio_root=root, runner=self._runner([]))

    def test_canonical_artifact_still_requires_debug(self):
        directory, root, game, _executable, output = self._fixture()
        with directory:
            cache = game / "build" / "devapi-debug" / "CMakeCache.txt"
            cache.write_text(self._cache_text(game, cache.parent).replace("Debug", "Release"), encoding="utf-8")
            with self.assertRaisesRegex(NativeCapturePreflightError, "Ninja and Debug"):
                prepare_native_capture(game, output, studio_root=root, runner=self._runner([]))

    def test_build_failure_stops_before_an_executable_provenance_is_returned(self):
        directory, root, game, _executable, output = self._fixture()
        with directory:
            def failing(command, **_kwargs):
                if command[0] == "cmake":
                    return subprocess.CompletedProcess(command, 1, stdout="", stderr="compile failure")
                return subprocess.CompletedProcess(command, 0, stdout=json.dumps({"fingerprint": "a" * 64}), stderr="")

            with self.assertRaisesRegex(NativeCapturePreflightError, "native capture command failed"):
                prepare_native_capture(game, output, studio_root=root, runner=failing)
            self.assertTrue((output / ".preflight" / "02-cmake-build.log").is_file())


    def test_foreign_cache_and_artifact_are_rejected(self):
        directory, _root, game, _executable, output = self._fixture()
        with directory:
            (game / "build" / "devapi-debug" / "CMakeCache.txt").write_text(
                self._cache_text(Path("C:/foreign/game"), game / "build" / "devapi-debug"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(NativeCapturePreflightError, "different game"):
                prepare_native_capture(game, output, studio_root=game.parent.parent.parent)

            (game / "build" / "devapi-debug" / "CMakeCache.txt").write_text(
                self._cache_text(game, game / "build" / "devapi-debug"), encoding="utf-8"
            )
            foreign = game / "old" / "game.exe"
            foreign.parent.mkdir()
            foreign.touch()
            with self.assertRaisesRegex(NativeCapturePreflightError, "canonical DevAPI artifact"):
                prepare_native_capture(game, output, requested_executable=foreign, studio_root=game.parent.parent.parent)

    def test_cache_requires_devapi_ninja_debug_and_its_own_build_directory(self):
        directory, root, game, _executable, output = self._fixture()
        with directory:
            cache = game / "build" / "devapi-debug" / "CMakeCache.txt"
            cache.write_text(self._cache_text(game, cache.parent, devapi="OFF"), encoding="utf-8")
            with self.assertRaisesRegex(NativeCapturePreflightError, "GAME_DEVAPI_ENABLED"):
                prepare_native_capture(game, output, studio_root=root, runner=self._runner([]))

            foreign_build = game / "build" / "other"
            cache.write_text(self._cache_text(game, foreign_build), encoding="utf-8")
            with self.assertRaisesRegex(NativeCapturePreflightError, "CMAKE_CACHEFILE_DIR"):
                prepare_native_capture(game, output, studio_root=root, runner=self._runner([]))

    def test_cache_rejects_a_foreign_explicit_game_repository_root(self):
        directory, root, game, _executable, output = self._fixture()
        with directory:
            cache = game / "build" / "devapi-debug" / "CMakeCache.txt"
            cache.write_text(
                self._cache_text(game, cache.parent, repository=root / "foreign-studio"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(NativeCapturePreflightError, "GAME_REPO_ROOT"):
                prepare_native_capture(game, output, studio_root=root, runner=self._runner([]))

    def test_cache_config_change_during_build_or_recording_invalidates_proof(self):
        directory, root, game, _executable, output = self._fixture()
        with directory:
            cache = game / "build" / "devapi-debug" / "CMakeCache.txt"

            def changes_after_build(command, **_kwargs):
                if command[0] == "cmake":
                    cache.write_text(self._cache_text(game, cache.parent, devapi="OFF"), encoding="utf-8")
                    return subprocess.CompletedProcess(command, 0, stdout="built", stderr="")
                return subprocess.CompletedProcess(command, 0, stdout=json.dumps({"fingerprint": "a" * 64}), stderr="")

            with self.assertRaisesRegex(NativeCapturePreflightError, "cache settings changed during native capture rebuild"):
                prepare_native_capture(game, output, studio_root=root, runner=changes_after_build)

            cache.write_text(self._cache_text(game, cache.parent), encoding="utf-8")
            provenance = prepare_native_capture(game, output, studio_root=root, runner=self._runner([]))
            cache.write_text(self._cache_text(game, cache.parent, devapi="OFF"), encoding="utf-8")
            with self.assertRaisesRegex(NativeCapturePreflightError, "GAME_DEVAPI_ENABLED"):
                validate_native_capture_final(provenance, runner=self._runner([]))

    def test_final_source_or_binary_race_invalidates_the_proof(self):
        directory, root, game, executable, output = self._fixture()
        with directory:
            calls = []
            provenance = prepare_native_capture(
                game, output, studio_root=root, runner=self._runner(calls)
            )
            with self.assertRaisesRegex(NativeCapturePreflightError, "changed during recording"):
                validate_native_capture_final(provenance, runner=self._runner(calls, "b" * 64))
            executable.write_bytes(b"changed executable")
            with self.assertRaisesRegex(NativeCapturePreflightError, "changed during recording"):
                validate_native_capture_final(provenance, runner=self._runner(calls))

    def test_source_change_during_build_fails_closed(self):
        directory, root, game, _executable, output = self._fixture()
        with directory:
            fingerprints = iter(["a" * 64, "b" * 64])

            def changing_source(command, **_kwargs):
                stdout = json.dumps({"fingerprint": next(fingerprints)}) if command[0] == "node" else "built"
                return subprocess.CompletedProcess(command, 0, stdout=stdout, stderr="")

            with self.assertRaisesRegex(NativeCapturePreflightError, "changed during native capture rebuild"):
                prepare_native_capture(game, output, studio_root=root, runner=changing_source)

    @unittest.skipUnless(
        shutil.which("cmake") and shutil.which("ninja"),
        "CMake and Ninja are required for the disposable cache integration test",
    )
    def test_real_cmake_cache_and_artifact_accept_only_its_own_game_root(self):
        temporary_root = Path.cwd() / "tmp"
        temporary_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=temporary_root) as directory:
            game = Path(directory) / "game"
            game.mkdir()
            (game / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(capture_preflight C)\n"
                "option(GAME_DEVAPI_ENABLED \"capture DevAPI\" ON)\n"
                "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/bin\")\n"
                "file(WRITE \"${CMAKE_BINARY_DIR}/main.c\" \"int main(void) { return 0; }\\n\")\n"
                "add_executable(game \"${CMAKE_BINARY_DIR}/main.c\")\n",
                encoding="utf-8",
            )
            build = game / "build" / "devapi-debug"
            subprocess.run(["cmake", "-G", "Ninja", "-S", str(game), "-B", str(build), "-DGAME_DEVAPI_ENABLED=ON", "-DCMAKE_BUILD_TYPE=Debug"], check=True, capture_output=True, text=True)
            subprocess.run(["cmake", "--build", str(build), "--target", "game"], check=True, capture_output=True, text=True)
            cache, executable = None, None
            from native_capture_preflight import validate_native_build_cache
            cache, executable = validate_native_build_cache(game)

            self.assertEqual(cache, build)
            self.assertEqual(executable, build / "bin" / ("game.exe" if os.name == "nt" else "game"))
            self.assertTrue(executable.is_file())


if __name__ == "__main__":
    unittest.main()
