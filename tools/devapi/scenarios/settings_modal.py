#!/usr/bin/env python3
"""Verify generic settings state can be changed through DevAPI."""

from __future__ import annotations

import sys

from base import Scenario
from devapi_client import running_game


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
    ok = True
    with running_game(port=port, fresh_state=True) as game:
        scenario = Scenario(game)
        game.result("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 0.33})
        value = game.result("game.state.get", {"doc": "game", "path": "settings.master_volume"})
        ok &= scenario.check("master volume changed", 0.32 <= value <= 0.34, value)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
