#!/usr/bin/env python3
"""Small conditional bot example for the first playable Game 67 slice."""

from __future__ import annotations

import sys

from devapi_client import DevApiError, running_game

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123


def main() -> int:
    try:
        with running_game(port=PORT) as game:
            endpoints = set(game.result("endpoints"))
            required = {"game.state", "game.reset_playtest", "frame.wait", "ui.click"}
            missing = sorted(required - endpoints)
            if missing:
                print("FAIL missing endpoints:", ", ".join(missing))
                return 1

            game.result("game.reset_playtest")
            game.wait_frames(2)
            state = game.observe()
            print("start:", state["meme_coins"], state["status"])

            for _ in range(8):
                state = game.observe()
                if state["meme_coins"] >= 5:
                    break
                state = game.click_ui("main.do67", wait_frames=2)
            if state["meme_coins"] < 5:
                print("FAIL coin target:", state)
                return 1

            state = game.click_ui("main.upgrade.first", wait_frames=2)
            if state["status"] < 2 or state["first_upgrade_owned"] is not True:
                print("FAIL upgrade target:", state)
                return 1

            state = game.click_ui("main.job.first", wait_frames=4)
            if state["first_job_active"] is not True:
                print("FAIL first job start:", state)
                return 1

            print("done:", state["meme_coins"], state["status"], "job_active=", state["first_job_active"])
            return 0
    except DevApiError as exc:
        print("FAIL devapi:", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
