#!/usr/bin/env python3
"""Shared helpers for DevAPI automation scenarios."""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "devapi"))

from devapi_client import DevApiClient, DevApiError


@dataclass
class Scenario:
    game: DevApiClient
    ok: bool = True

    def check(self, name: str, condition: bool, details: Any = "") -> bool:
        print(("PASS" if condition else "FAIL"), name, "::", details)
        self.ok = self.ok and condition
        return condition

    def require_endpoint(self, method: str) -> bool:
        endpoints = self.game.result("endpoints")
        return self.check(f"endpoint {method}", method in endpoints, endpoints)

    def click_and_observe(self, element_id: str, wait_frames: int = 1) -> dict[str, Any]:
        return self.game.click_ui(element_id, wait_frames=wait_frames)

    def capture(self, output: str, wait_frames: int = 1) -> str:
        path = self.game.capture_screenshot(output, wait_frames=wait_frames)
        self.check("capture exists", os.path.exists(path) and os.path.getsize(path) > 0, path)
        return path


def finish(ok: bool) -> int:
    print("\n=== %s ===" % ("ALL PASSED" if ok else "FAILED"))
    return 0 if ok else 1


def fail_devapi(exc: DevApiError) -> int:
    print("FAIL devapi ::", exc)
    return 1
