#!/usr/bin/env python3
"""Tiny DevAPI bot for the seed screen."""

from __future__ import annotations

import sys

from devapi_client import running_game


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    clicks = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    with running_game(port=port, fresh_state=True) as game:
        game.result("game.reset_playtest")
        state = {}
        for _ in range(clicks):
            state = game.click_ui("seed.cycle", wait_frames=1)
        print(state)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
