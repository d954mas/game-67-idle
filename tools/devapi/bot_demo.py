#!/usr/bin/env python3
"""Small conditional bot example for the test scene."""

from __future__ import annotations

import sys

from devapi_client import DevApiError, running_game

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123


def main() -> int:
    try:
        with running_game(port=PORT) as game:
            endpoints = set(game.result("endpoints"))
            required = {"game.state", "input.key", "frame.wait", "ui.scroll"}
            missing = sorted(required - endpoints)
            if missing:
                print("FAIL missing endpoints:", ", ".join(missing))
                return 1

            state = game.observe()
            print("start:", state["shape"], state["render_mode"], state["camera_distance"])

            target_shape_index = 2
            for _ in range(8):
                state = game.observe()
                if state["shape_index"] == target_shape_index:
                    break
                state = game.key_tap("D")
            if state["shape_index"] != target_shape_index:
                print("FAIL shape target:", state)
                return 1

            for _ in range(4):
                state = game.observe()
                if state["render_mode"] == "wire":
                    break
                state = game.key_tap("W")
            if state["render_mode"] != "wire":
                print("FAIL render target:", state)
                return 1

            before_zoom = state["camera_distance"]
            state = game.scroll_ui("scene.viewport", dy=-120)
            if state["camera_distance"] == before_zoom:
                print("FAIL zoom unchanged:", before_zoom, state["camera_distance"])
                return 1

            print("done:", state["shape"], state["render_mode"], state["camera_distance"])
            return 0
    except DevApiError as exc:
        print("FAIL devapi:", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
