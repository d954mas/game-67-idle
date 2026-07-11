#!/usr/bin/env python3
"""Run the complete feature-local game-state codegen test contract."""

from __future__ import annotations

import subprocess
import sys
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
        str(ROOT / "features/game-state/benchmarks/benchmark_codegen_test.py"),
    ),
)


def main() -> int:
    failed = False
    for command in TEST_COMMANDS:
        result = subprocess.run(command, cwd=ROOT, check=False)
        failed = failed or result.returncode != 0
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
