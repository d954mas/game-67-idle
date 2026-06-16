#!/usr/bin/env python3
"""Capture portrait (540x960) proof shots for Critter Corral: title + upgrade.

Part A verification for T0067 (portrait layout + touch). Launches the native
game in a 540x960 portrait window via DevAPI and captures:
  - build/captures/corral_portrait.png         (title in portrait)
  - build/captures/corral_portrait_upgrade.png  (pick-1-of-3 cards in portrait)

Both are audited for pixel health (non-blank / readable). Also dumps a slice of
game.state so touch/pointer + phase can be confirmed from the report.
"""

from __future__ import annotations

import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "devapi"))

from devapi_client import running_game  # noqa: E402


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9137
    with running_game(port=port, fresh_state=True, window_size="540x960") as game:
        st = game.result("game.reset_playtest")
        print("phase after reset:", st.get("phase"))
        # Title in portrait (pens + HUD + title text all on-screen).
        p1 = game.capture_screenshot(
            "build/captures/corral_portrait.png", wait_frames=3, audit=True
        )
        print("TITLE capture:", p1)

        # In-play portrait: start the run, let a couple frames tick, capture the
        # field with HUD + FTUE hint (proves gameplay layout reads in portrait).
        game.result("game.start")
        pplay = game.capture_screenshot(
            "build/captures/corral_portrait_play.png", wait_frames=6, audit=True
        )
        print("PLAY capture:", pplay)

        # Drive to an upgrade choice: skip wave 1 -> WAVE_CLEARED ->
        # UPGRADE_CHOICE (skip_wave surfaces the pick-1-of-3 like real play).
        st = game.result("game.debug.skip_wave")
        print("phase after skip_wave:", st.get("phase"),
              "offer:", [c.get("upgrade") for c in st.get("pending_choice", [])])
        p2 = game.capture_screenshot(
            "build/captures/corral_portrait_upgrade.png", wait_frames=3, audit=True
        )
        print("UPGRADE capture:", p2)

        # Report a compact state slice (proves pens laid out, phase, etc).
        slim = {
            "phase": st.get("phase"),
            "wave": st.get("wave"),
            "color_count": st.get("color_count"),
            "pending_choice": st.get("pending_choice"),
            "sprites_ready": st.get("sprites_ready"),
            "text_ready": st.get("text_ready"),
        }
        print("STATE:", json.dumps(slim))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
