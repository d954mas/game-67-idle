#!/usr/bin/env python3
"""Run the complete feature-local game-state codegen test contract."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parents[2]
TEST_COMMANDS = (
    (sys.executable, str(SCRIPT_DIR / "generate_state_test.py")),
    (sys.executable, "-m", "unittest", str(SCRIPT_DIR / "state_modules_test.py")),
    (
        sys.executable,
        "-m",
        "unittest",
        str(ROOT / "features/game-state/tests/test_game_storage_web_lto.py"),
    ),
    (
        sys.executable,
        "-m",
        "unittest",
        str(ROOT / "features/game-state/benchmarks/benchmark_codegen_test.py"),
    ),
)


def run_writer_test() -> int:
    """Compile the C hot-path contract without relying on a consumer build tree."""
    with tempfile.TemporaryDirectory() as tmp:
        executable = Path(tmp) / "test_game_save_writer.exe"
        command = (
            "clang",
            "-std=c17",
            "-D_CRT_SECURE_NO_WARNINGS",
            "-I", str(ROOT / "features/game-state/include"),
            "-I", str(ROOT / "external/neotolis-engine/deps/cjson"),
            "-I", str(ROOT / "external/neotolis-engine/deps/unity/src"),
            str(ROOT / "features/game-state/tests/test_game_save_writer.c"),
            str(ROOT / "features/game-state/src/game_save_writer.c"),
            str(ROOT / "external/neotolis-engine/deps/cjson/cJSON.c"),
            str(ROOT / "external/neotolis-engine/deps/unity/src/unity.c"),
            "-o", str(executable),
        )
        result = subprocess.run(command, cwd=ROOT, check=False)
        if result.returncode != 0:
            return result.returncode
        return subprocess.run((str(executable),), cwd=ROOT, check=False).returncode


def run_generated_snapshot_regression() -> int:
    """Compile a schema-bounded aggregate snapshot without a consumer build tree."""
    with tempfile.TemporaryDirectory() as tmp:
        generated = Path(tmp) / "generated"
        generate = (
            sys.executable,
            str(SCRIPT_DIR / "generate_state.py"),
            "--schema", str(ROOT / "features/game-state/tests/items_containers.schema.json"),
            "--out-dir", str(generated),
            "--fragment", "items_v2",
        )
        if subprocess.run(generate, cwd=ROOT, check=False).returncode != 0:
            return 1
        executable = Path(tmp) / "test_generated_snapshot_over512.exe"
        command = (
            "clang",
            "-std=c17",
            "-D_CRT_SECURE_NO_WARNINGS",
            "-I", str(generated),
            "-I", str(ROOT / "features/game-state/include"),
            "-I", str(ROOT / "external/neotolis-engine/deps/cjson"),
            "-I", str(ROOT / "external/neotolis-engine/deps/unity/src"),
            str(ROOT / "features/game-state/tests/test_generated_snapshot_over512.c"),
            str(generated / "items_v2_state.c"),
            str(ROOT / "features/game-state/src/game_save_writer.c"),
            str(ROOT / "features/game-state/src/game_state_json.c"),
            str(ROOT / "external/neotolis-engine/deps/cjson/cJSON.c"),
            str(ROOT / "external/neotolis-engine/deps/unity/src/unity.c"),
            "-o", str(executable),
        )
        result = subprocess.run(command, cwd=ROOT, check=False)
        if result.returncode != 0:
            return result.returncode
        return subprocess.run((str(executable),), cwd=ROOT, check=False).returncode


def main() -> int:
    failed = False
    for command in TEST_COMMANDS:
        result = subprocess.run(command, cwd=ROOT, check=False)
        failed = failed or result.returncode != 0
    failed = run_writer_test() != 0 or failed
    failed = run_generated_snapshot_regression() != 0 or failed
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
