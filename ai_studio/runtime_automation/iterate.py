#!/usr/bin/env python3
"""One-command visual iteration loop for the native game.

  build-if-stale -> launch -> in-process framebuffer screenshot ->
  ui_readability montage -> print the next quality-review step.

Replaces re-deriving 3-4 manual invocations every session. Game-agnostic (uses
only the universal framebuffer + ui_readability). Pass --reuse to attach to an
already-running game and pay the ~1.7s launch only once per session.

Usage: py -3.12 ai_studio/runtime_automation/iterate.py [port] [--reuse]
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from devapi_client import NATIVE_DEBUG_EXE, ROOT, running_game

SHOT = "build/captures/iterate.png"


def newest_src_mtime() -> float:
    newest = 0.0
    src = Path(ROOT, "src")
    for path in src.rglob("*"):
        if path.suffix in (".c", ".h"):
            newest = max(newest, path.stat().st_mtime)
    return newest


def build_if_stale() -> bool:
    exe = Path(NATIVE_DEBUG_EXE)
    if exe.exists() and exe.stat().st_mtime >= newest_src_mtime():
        print("build: up to date")
        return True
    print("build: rebuilding game_seed (native-debug) ...")
    result = subprocess.run(
        ["cmake", "--build", "--preset", "native-debug", "--target", "game_seed"], cwd=ROOT
    )
    return result.returncode == 0


def main() -> int:
    argv = sys.argv[1:]
    reuse = "--reuse" in argv
    ports = [a for a in argv if a.isdigit()]
    port = int(ports[0]) if ports else 9123

    if not reuse and not build_if_stale():
        print("build failed", file=sys.stderr)
        return 1

    with running_game(port=port, fresh_state=True, reuse_existing=reuse) as game:
        shot_path = game.capture_screenshot(SHOT, audit=False)
    print(f"shot: {shot_path}")

    subprocess.run(
        [sys.executable, str(Path(ROOT, "ai_studio", "runtime_automation", "ui_readability.py")), shot_path],
        cwd=ROOT,
    )
    print("\nnext: review the screen against ai_studio/quality/rules/player_clarity and rules/art:")
    print(f"  screenshot evidence: {SHOT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
