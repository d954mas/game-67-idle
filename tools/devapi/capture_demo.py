#!/usr/bin/env python3
"""Drive the test scene and capture visual evidence."""

from __future__ import annotations

import os
import sys

from devapi_client import DevApiError, running_game

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
OUTPUT = sys.argv[2] if len(sys.argv) > 2 else "build/captures/devapi_capture_demo.png"


def main() -> int:
    try:
        with running_game(port=PORT) as game:
            game.key_tap("D")
            game.scroll_ui("scene.viewport", dy=-120)
            path = game.capture_screenshot(OUTPUT, wait_frames=2)
            if not os.path.exists(path) or os.path.getsize(path) <= 0:
                print("FAIL screenshot:", path)
                return 1
            print("screenshot:", path)
            return 0
    except DevApiError as exc:
        print("FAIL devapi:", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
