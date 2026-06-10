#!/usr/bin/env python3
"""Smoke-test the first playable Game 67 slice through DevAPI."""

import sys

from devapi_client import DevApiError, running_game

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass


def main() -> int:
    try:
        def check(name: str, condition: bool, extra=""):
            print(("PASS" if condition else "FAIL"), name, "::", extra)
            return condition

        ok = True
        with running_game(port=PORT) as bus:
            ok &= check("ping", bus.request("ping").get("ok") is True)
            endpoints = bus.request("endpoints")
            endpoint_names = endpoints.get("result", [])
            ok &= check("game endpoints", all(name in endpoint_names for name in ("game.state", "game.reset_playtest", "ui.tree", "ui.click", "frame.wait")), endpoints)

            bus.request("game.reset_playtest")
            bus.request("frame.wait", {"frames": 2})
            ui_tree = bus.request("ui.tree")
            ui_ids = [item.get("id") for item in ui_tree.get("result", [])]
            ok &= check("ui exposes gameplay", all(item in ui_ids for item in ("main.do67", "main.upgrade.first", "main.job.first", "main.reset")), ui_tree)

            state0 = bus.observe()
            ok &= check(
                "fresh state",
                state0.get("meme_coins") == 0
                and state0.get("status") == 1
                and state0.get("click_power") == 1
                and state0.get("visual_stage") == 1
                and state0.get("third_upgrade_owned") is False,
                state0,
            )

            for _ in range(5):
                state = bus.click_ui("main.do67", wait_frames=2)
            ok &= check("do67 earns coins", state.get("meme_coins") >= 5, state)

            state = bus.click_ui("main.upgrade.first", wait_frames=2)
            ok &= check("first upgrade raises status", state.get("status") >= 2 and state.get("first_upgrade_owned") is True, state)

            state = bus.click_ui("main.job.first", wait_frames=4)
            ok &= check("first job starts", state.get("first_job_active") is True, state)

            state = {}
            for _ in range(120):
                bus.request("frame.wait", {"frames": 20})
                state = bus.request("game.state").get("result", {})
                if state.get("first_job_ready") is True:
                    break
            ok &= check("first job becomes ready", state.get("first_job_ready") is True, state)

            if state.get("first_job_ready") is True:
                state = bus.click_ui("main.claim", wait_frames=2)
                ok &= check(
                    "claim exact reward state",
                    state.get("meme_coins") == 8
                    and state.get("status") == 3
                    and state.get("click_power") == 2
                    and state.get("income_per_second") == 1
                    and state.get("comfort") == 2
                    and state.get("visual_stage") == 3
                    and state.get("first_upgrade_owned") is True
                    and state.get("second_upgrade_owned") is False
                    and state.get("first_job_active") is False
                    and state.get("first_job_ready") is False
                    and state.get("first_job_elapsed_ms") == 0,
                    state,
                )
                upgrade = bus.request("ui.element", {"id": "main.upgrade.first"}).get("result", {})
                ok &= check(
                    "second upgrade available",
                    upgrade.get("role") == "button"
                    and upgrade.get("label") == "Upgrade"
                    and upgrade.get("text") == "ready cap"
                    and upgrade.get("enabled") is True,
                    upgrade,
                )
                state = bus.click_ui("main.upgrade.first", wait_frames=2)
                ok &= check(
                    "second upgrade raises status",
                    state.get("meme_coins") == 0
                    and state.get("status") == 4
                    and state.get("click_power") == 3
                    and state.get("hands_skill") == 3
                    and state.get("visual_stage") == 4
                    and state.get("first_upgrade_owned") is True
                    and state.get("second_upgrade_owned") is True
                    and state.get("third_upgrade_owned") is False
                    and state.get("income_per_second") == 1
                    and state.get("comfort") == 2,
                    state,
                )
                upgrade = bus.request("ui.element", {"id": "main.upgrade.first"}).get("result", {})
                ok &= check(
                    "third upgrade locked goal visible",
                    upgrade.get("role") == "button"
                    and upgrade.get("label") == "Upgrade"
                    and upgrade.get("text") == "need 20 coins"
                    and upgrade.get("enabled") is True,
                    upgrade,
                )
                for _ in range(7):
                    state = bus.click_ui("main.do67", wait_frames=2)
                ok &= check(
                    "x3 taps reach third upgrade cost",
                    state.get("meme_coins", 0) >= 20 and state.get("status") == 4 and state.get("click_power") == 3,
                    state,
                )
                upgrade = bus.request("ui.element", {"id": "main.upgrade.first"}).get("result", {})
                ok &= check(
                    "third upgrade available",
                    upgrade.get("role") == "button"
                    and upgrade.get("label") == "Upgrade"
                    and upgrade.get("text") == "ready pic"
                    and upgrade.get("enabled") is True,
                    upgrade,
                )
                coins_before_third = state.get("meme_coins", 0)
                state = bus.click_ui("main.upgrade.first", wait_frames=2)
                third_idle_gain = state.get("meme_coins", 0) - (coins_before_third - 20)
                ok &= check(
                    "third upgrade extends loop",
                    third_idle_gain >= 0
                    and state.get("status") == 6
                    and state.get("click_power") == 5
                    and state.get("hands_skill") == 4
                    and state.get("visual_stage") == 6
                    and state.get("first_upgrade_owned") is True
                    and state.get("second_upgrade_owned") is True
                    and state.get("third_upgrade_owned") is True
                    and state.get("first_job_active") is False,
                    {"state": state, "coins_before_third": coins_before_third, "third_idle_gain": third_idle_gain},
                )
                job = bus.request("ui.element", {"id": "main.job.first"}).get("result", {})
                ok &= check(
                    "second job tier visible",
                    job.get("role") == "button"
                    and job.get("label") == "Start sticker job"
                    and job.get("text") == "reward 30"
                    and job.get("enabled") is True,
                    job,
                )
                state = bus.click_ui("main.job.first", wait_frames=4)
                ok &= check(
                    "second job starts",
                    state.get("first_job_active") is True
                    and state.get("active_job_id") == "sticker_run"
                    and state.get("active_job_duration_ms") == 8000,
                    state,
                )
                state = {}
                for _ in range(160):
                    bus.request("frame.wait", {"frames": 20})
                    state = bus.request("game.state").get("result", {})
                    if state.get("first_job_ready") is True:
                        break
                ok &= check("second job becomes ready", state.get("first_job_ready") is True and state.get("active_job_id") == "sticker_run", state)
                if state.get("first_job_ready") is True:
                    coins_before_claim = state.get("meme_coins", 0)
                    state = bus.click_ui("main.claim", wait_frames=2)
                    ok &= check(
                        "second job claim extends progression",
                        state.get("meme_coins", 0) >= coins_before_claim + 30
                        and state.get("status") >= 7
                        and state.get("income_per_second") >= 2
                        and state.get("hands_skill") >= 5
                        and state.get("visual_stage") >= 7
                        and state.get("first_job_active") is False
                        and state.get("active_job_id") == "",
                        state,
                    )
                    coins_before_bike = state.get("meme_coins", 0)
                    ok &= check(
                        "stk claim leaves bike affordable",
                        coins_before_bike >= 35
                        and state.get("status") >= 7
                        and state.get("click_power") == 5
                        and state.get("income_per_second") >= 2
                        and state.get("visual_stage") >= 7,
                        state,
                    )
                    upgrade = bus.request("ui.element", {"id": "main.upgrade.first"}).get("result", {})
                    ok &= check(
                        "bike upgrade available",
                        upgrade.get("role") == "button"
                        and upgrade.get("label") == "Upgrade"
                        and upgrade.get("text") == "ready bike"
                        and upgrade.get("enabled") is True,
                        upgrade,
                    )
                    state = bus.click_ui("main.upgrade.first", wait_frames=2)
                    bike_cost = 35
                    bike_idle_gain = state.get("meme_coins", 0) - (coins_before_bike - bike_cost)
                    ok &= check(
                        "bike upgrade extends loop",
                        state.get("fourth_upgrade_owned") is True
                        and 0 <= bike_idle_gain <= max(3, state.get("income_per_second", 0))
                        and state.get("status") >= 9
                        and state.get("click_power") == 8
                        and state.get("income_per_second") >= 3
                        and state.get("hands_skill") >= 6
                        and state.get("visual_stage") >= 9
                        and state.get("first_upgrade_owned") is True
                        and state.get("second_upgrade_owned") is True
                        and state.get("third_upgrade_owned") is True,
                        {"state": state, "coins_before_bike": coins_before_bike, "bike_idle_gain": bike_idle_gain},
                    )
                    coins_after_bike = state.get("meme_coins", 0)
                    bike_click_power = state.get("click_power", 0)

                    for _ in range(7):
                        state = bus.click_ui("main.do67", wait_frames=2)

                    coins_before_stnd = state.get("meme_coins", 0)
                    ok &= check(
                        "x8 taps reach stnd cost",
                        coins_before_stnd >= 60
                        and coins_before_stnd >= coins_after_bike + (7 * bike_click_power)
                        and state.get("fourth_upgrade_owned") is True
                        and state.get("fifth_upgrade_owned") is False
                        and state.get("click_power") >= 8
                        and state.get("income_per_second") >= 3,
                        {"state": state, "coins_after_bike": coins_after_bike},
                    )

                    upgrade = bus.request("ui.element", {"id": "main.upgrade.first"}).get("result", {})
                    ok &= check(
                        "stnd upgrade available",
                        upgrade.get("role") == "button"
                        and upgrade.get("label") == "Upgrade"
                        and upgrade.get("text") == "ready stnd"
                        and upgrade.get("enabled") is True,
                        upgrade,
                    )

                    state = bus.click_ui("main.upgrade.first", wait_frames=2)
                    stnd_cost = 60
                    stnd_idle_gain = state.get("meme_coins", 0) - (coins_before_stnd - stnd_cost)
                    ok &= check(
                        "stnd upgrade extends loop",
                        state.get("fifth_upgrade_owned") is True
                        and stnd_idle_gain >= 0
                        and state.get("status") >= 10
                        and state.get("click_power") >= 10
                        and state.get("income_per_second") >= 8
                        and state.get("hands_skill") >= 7
                        and state.get("visual_stage") >= 10
                        and state.get("first_upgrade_owned") is True
                        and state.get("second_upgrade_owned") is True
                        and state.get("third_upgrade_owned") is True
                        and state.get("fourth_upgrade_owned") is True,
                        {"state": state, "coins_before_stnd": coins_before_stnd, "stnd_idle_gain": stnd_idle_gain},
                    )
                    upgrade = bus.request("ui.element", {"id": "main.upgrade.first"}).get("result", {})
                    ok &= check(
                        "stnd terminal upgrade state",
                        upgrade.get("text") == "owned x10"
                        and upgrade.get("enabled") is False
                        and state.get("fifth_upgrade_owned") is True
                        and state.get("status") >= 10
                        and state.get("click_power") >= 10
                        and state.get("income_per_second") >= 8,
                        {"upgrade": upgrade, "state": state},
                    )

                    coins_before_idle = state.get("meme_coins", 0)
                    income = state.get("income_per_second", 0)
                    state = {}
                    idle_gain = 0
                    for _ in range(80):
                        bus.wait_frames(20)
                        state = bus.request("game.state").get("result", {})
                        idle_gain = state.get("meme_coins", 0) - coins_before_idle
                        if idle_gain >= income:
                            break
                    ok &= check(
                        "post-stnd idle income ticks",
                        income >= 8 and idle_gain >= income,
                        {"state": state, "coins_before_idle": coins_before_idle, "idle_gain": idle_gain},
                    )

                    job = bus.request("ui.element", {"id": "main.job.first"}).get("result", {})
                    ok &= check(
                        "shop job visible",
                        job.get("role") == "button"
                        and job.get("label") == "Start shop job"
                        and job.get("text") == "reward 90"
                        and job.get("enabled") is True,
                        job,
                    )
                    state = bus.click_ui("main.job.first", wait_frames=4)
                    ok &= check(
                        "shop job starts",
                        state.get("first_job_active") is True
                        and state.get("active_job_id") == "meme_stand_owner"
                        and state.get("active_job_duration_ms") == 10000
                        and state.get("fifth_upgrade_owned") is True,
                        state,
                    )
                    state = {}
                    for _ in range(220):
                        bus.wait_frames(20)
                        state = bus.request("game.state").get("result", {})
                        if state.get("first_job_ready") is True:
                            break
                    ok &= check("shop job becomes ready", state.get("first_job_ready") is True and state.get("active_job_id") == "meme_stand_owner", state)
                    if state.get("first_job_ready") is True:
                        coins_before_claim = state.get("meme_coins", 0)
                        state = bus.click_ui("main.claim", wait_frames=2)
                        claim_gain = state.get("meme_coins", 0) - coins_before_claim
                        ok &= check(
                            "shop claim extends post-stnd loop",
                            claim_gain >= 90
                            and state.get("income_per_second") >= 16
                            and state.get("status") >= 11
                            and state.get("hands_skill") >= 8
                            and state.get("visual_stage") >= 11
                            and state.get("first_job_active") is False
                            and state.get("active_job_id") == "",
                            {"state": state, "claim_gain": claim_gain},
                        )

        print("\n=== %s ===" % ("ALL PASSED" if ok else "FAILED"))
        return 0 if ok else 1
    except DevApiError as exc:
        print("FAIL devapi ::", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
