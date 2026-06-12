#!/usr/bin/env python3
"""Drive the seed screen and capture visual evidence."""

from __future__ import annotations

import sys

from devapi_client import running_game


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    output = sys.argv[2] if len(sys.argv) > 2 else "build/captures/devapi_capture_demo.png"
    with running_game(port=port, fresh_state=True) as game:
        game.result("game.reset_playtest")
        for _ in range(2):
            game.click_ui("seed.cycle", wait_frames=2)
        path = game.capture_screenshot(output, wait_frames=2, audit=True)
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
