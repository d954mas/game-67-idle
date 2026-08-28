from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
FEATURE_DIR = ROOT / "features/game-state"


def find_emcc() -> str | None:
    configured = os.environ.get("EMCC")
    if configured:
        return configured
    emsdk = os.environ.get("EMSDK")
    if emsdk:
        candidate = Path(emsdk) / "upstream/emscripten/emcc.bat"
        if candidate.is_file():
            return str(candidate)
    if os.name == "nt":
        candidate = Path("C:/develop/emsdk/upstream/emscripten/emcc.bat")
        if candidate.is_file():
            return str(candidate)
    return shutil.which("emcc")


class GameStorageWebLtoTests(unittest.TestCase):
    def test_imports_survive_cross_translation_unit_lto(self) -> None:
        emcc = find_emcc()
        if emcc is None:
            self.skipTest("emcc is unavailable")

        with tempfile.TemporaryDirectory() as tmp:
            tmp_dir = Path(tmp)
            fixture_object = tmp_dir / "fixture.o"
            implementation_object = tmp_dir / "implementation.o"
            common = (
                emcc,
                "-O3",
                "-flto",
                "-I",
                str(FEATURE_DIR / "src"),
            )
            commands = (
                common
                + (
                    "-c",
                    str(FEATURE_DIR / "tests/game_storage_web_lto_fixture.c"),
                    "-o",
                    str(fixture_object),
                ),
                common
                + (
                    "-c",
                    str(FEATURE_DIR / "src/game_storage_web.c"),
                    "-o",
                    str(implementation_object),
                ),
                (
                    emcc,
                    "-O3",
                    "-flto",
                    str(fixture_object),
                    str(implementation_object),
                    "-sENVIRONMENT=web",
                    "-sNO_EXIT_RUNTIME=1",
                    "-sEXPORTED_FUNCTIONS=_main,_malloc,_free",
                    "-o",
                    str(tmp_dir / "storage.js"),
                ),
            )
            for command in commands:
                result = subprocess.run(
                    command,
                    cwd=ROOT,
                    check=False,
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(0, result.returncode, result.stderr)


if __name__ == "__main__":
    unittest.main()
