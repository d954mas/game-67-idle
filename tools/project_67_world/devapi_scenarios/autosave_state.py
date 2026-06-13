#!/usr/bin/env python3
"""Verify dirty state is autosaved and loaded on the next run."""

from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT_DIR / "tools" / "devapi"))

from devapi_client import DevApiError, ROOT, running_game
from base import Scenario, fail_devapi, finish

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123
AUTOSAVE_PATH = os.path.join(ROOT, "build", "saves", "autosave", "game.json")


def main() -> int:
    ok = True
    try:
        if os.path.exists(AUTOSAVE_PATH):
            os.remove(AUTOSAVE_PATH)

        with running_game(port=PORT, fresh_state=True, autosave_enabled=True) as game:
            scenario = Scenario(game)
            scenario.require_endpoint("game.state.set")
            game.result("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 0.33})
            game.wait_frames(3)
            state = game.observe()
            ok &= scenario.check("autosave cleared dirty", state.get("state_dirty") is False, state)
            ok &= scenario.check("autosave file exists", os.path.exists(AUTOSAVE_PATH), AUTOSAVE_PATH)
            ok &= scenario.ok

        with running_game(port=PORT, fresh_state=False, autosave_enabled=True) as game:
            scenario = Scenario(game)
            game.wait_frames(2)
            loaded = game.result("game.state.get", {"doc": "game", "path": "settings.master_volume"})
            ok &= scenario.check("autosave loaded on startup", 0.32 <= loaded <= 0.34, loaded)
            ok &= scenario.ok
        return finish(ok)
    except DevApiError as exc:
        return fail_devapi(exc)


if __name__ == "__main__":
    raise SystemExit(main())
