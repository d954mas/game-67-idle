#!/usr/bin/env python3
"""Drive one interaction and save a screenshot as visual evidence."""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from devapi_client import DevApiError, running_game
from base import Scenario, fail_devapi, finish

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
OUTPUT = sys.argv[2] if len(sys.argv) > 2 else "build/captures/scenarios/evidence_capture.png"


def main() -> int:
    try:
        with running_game(port=PORT) as game:
            scenario = Scenario(game)
            game.click_ui("test.button", wait_frames=2)
            path = scenario.capture(OUTPUT, wait_frames=1)
            print("screenshot:", path)
            return finish(scenario.ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
