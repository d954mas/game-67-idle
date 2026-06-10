#!/usr/bin/env python3
"""Drive the Game 67 surface and capture visual evidence."""

from __future__ import annotations

import os
import sys

from devapi_client import DevApiError, running_game

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
OUTPUT = sys.argv[2] if len(sys.argv) > 2 else "build/captures/devapi_capture_demo.png"
FULL_LOOP = "--full-loop" in sys.argv


def main() -> int:
    try:
        with running_game(port=PORT) as game:
            if FULL_LOOP:
                for _ in range(5):
                    game.click_ui("main.do67", wait_frames=2)
                game.click_ui("main.upgrade.first", wait_frames=2)
                game.click_ui("main.job.first", wait_frames=4)
                for _ in range(120):
                    state = game.request("game.state").get("result", {})
                    if state.get("first_job_ready") is True:
                        break
                    game.wait_frames(20)
                game.click_ui("main.claim", wait_frames=2)
                game.click_ui("main.upgrade.first", wait_frames=2)
                for _ in range(7):
                    game.click_ui("main.do67", wait_frames=2)
                game.click_ui("main.upgrade.first", wait_frames=2)
                game.click_ui("main.job.first", wait_frames=4)
                for _ in range(160):
                    state = game.request("game.state").get("result", {})
                    if state.get("first_job_ready") is True:
                        break
                    game.wait_frames(20)
                game.click_ui("main.claim", wait_frames=2)
                game.click_ui("main.upgrade.first", wait_frames=2)
                for _ in range(7):
                    game.click_ui("main.do67", wait_frames=2)
                game.click_ui("main.upgrade.first", wait_frames=2)
                game.click_ui("main.job.first", wait_frames=4)
                for _ in range(220):
                    state = game.request("game.state").get("result", {})
                    if state.get("first_job_ready") is True:
                        break
                    game.wait_frames(20)
                game.click_ui("main.claim", wait_frames=2)
            else:
                game.click_ui("main.do67", wait_frames=2)
            path = game.capture_screenshot(OUTPUT, wait_frames=30)
            if not os.path.exists(path) or os.path.getsize(path) <= 0:
                print("FAIL screenshot:", path)
                return 1
            health = game.audit_screenshot(path)
            print("screenshot:", path)
            print("pixel_health:", health.summary())
            return 0
    except DevApiError as exc:
        print("FAIL devapi:", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
