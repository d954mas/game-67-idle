#!/usr/bin/env python3
"""Throwaway Chrome profiles for headless runs.

Every profile lives under one root in the system temp folder, so a run that
crashed before cleanup is swept by the next run instead of leaking forever.
A fresh profile grows to 50-200 MB on first launch.
"""

from __future__ import annotations

import contextlib
import shutil
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Iterator

PROFILE_ROOT = Path(tempfile.gettempdir()) / "ai-studio-chrome"
STALE_SECONDS = 24 * 60 * 60
EXIT_WAIT_SECONDS = 5.0


def sweep_stale_profiles(root: Path = PROFILE_ROOT, max_age: float = STALE_SECONDS, now: float | None = None) -> int:
    """Remove profiles older than max_age; one a live Chrome still holds fails to delete and is kept."""
    now = time.time() if now is None else now
    removed = 0
    try:
        entries = list(root.iterdir())
    except OSError:
        return 0
    for path in entries:
        try:
            if now - path.stat().st_mtime < max_age:
                continue
            shutil.rmtree(path)
            removed += 1
        except OSError:
            pass
    return removed


def remove_profile(path: Path, proc: subprocess.Popen | None = None) -> None:
    """Stop Chrome, wait for it to exit (Windows keeps held files) and delete the profile."""
    if proc is not None and proc.poll() is None:
        proc.terminate()
        with contextlib.suppress(subprocess.TimeoutExpired):
            proc.wait(EXIT_WAIT_SECONDS)
    for _ in range(10):
        shutil.rmtree(path, ignore_errors=True)
        if not path.exists():
            return
        time.sleep(0.1)


@contextlib.contextmanager
def chrome_profile(prefix: str = "chrome", root: Path = PROFILE_ROOT) -> Iterator[Path]:
    """Yield a profile directory for --user-data-dir.

    Stop Chrome inside the block (or pass the process to remove_profile) so the
    folder can be deleted on exit.
    """
    root.mkdir(parents=True, exist_ok=True)
    sweep_stale_profiles(root)
    path = Path(tempfile.mkdtemp(prefix=f"{prefix}-", dir=root))
    try:
        yield path
    finally:
        remove_profile(path)


__all__ = ["PROFILE_ROOT", "chrome_profile", "remove_profile", "sweep_stale_profiles"]
