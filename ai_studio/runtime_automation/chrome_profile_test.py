#!/usr/bin/env python3
"""Tests for throwaway Chrome profiles."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from chrome_profile import chrome_profile, remove_profile, sweep_stale_profiles  # noqa: E402

_CANDIDATES = [
    os.environ.get("CHROME_PATH"),
    os.path.join(os.environ.get("PROGRAMFILES", ""), "Google", "Chrome", "Application", "chrome.exe"),
    os.path.join(os.environ.get("PROGRAMFILES(X86)", ""), "Google", "Chrome", "Application", "chrome.exe"),
    "/usr/bin/google-chrome",
    "/usr/bin/chromium",
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
]
CHROME = next((path for path in _CANDIDATES if path and os.path.exists(path)), None)


class ChromeProfileTest(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="chrome-profile-test-"))
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)

    def test_sweep_removes_old_and_keeps_fresh(self) -> None:
        old = self.root / "old-1"
        fresh = self.root / "fresh-1"
        old.mkdir()
        fresh.mkdir()
        two_days_ago = time.time() - 2 * 24 * 60 * 60
        os.utime(old, (two_days_ago, two_days_ago))

        self.assertEqual(sweep_stale_profiles(self.root), 1)
        self.assertEqual([p.name for p in self.root.iterdir()], ["fresh-1"])

    def test_missing_root_sweeps_nothing(self) -> None:
        self.assertEqual(sweep_stale_profiles(self.root / "missing"), 0)

    def test_context_removes_profile_on_exit(self) -> None:
        with chrome_profile("probe", root=self.root) as path:
            (path / "marker").write_text("x")
        self.assertFalse(path.exists())

    @unittest.skipUnless(CHROME, "Chrome is not installed")
    def test_remove_profile_after_running_chrome(self) -> None:
        with chrome_profile("probe", root=self.root) as path:
            proc = subprocess.Popen(
                [CHROME, "--headless=new", f"--user-data-dir={path}", "--no-first-run",
                 "--no-default-browser-check", "--remote-debugging-port=0", "about:blank"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            deadline = time.time() + 15
            while not (path / "Default").exists() and time.time() < deadline:
                time.sleep(0.1)
            self.assertTrue((path / "Default").exists(), "Chrome did not create its profile")
            # Let Chrome finish starting: from then on Windows refuses to delete the files it holds.
            time.sleep(1)
            remove_profile(path, proc)
            self.assertFalse(path.exists())


if __name__ == "__main__":
    unittest.main()
