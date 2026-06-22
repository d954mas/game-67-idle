#!/usr/bin/env python3
"""Cozy Automation -- first-screen playable smoke + product-read state capture.

Launches the native debug build with DevAPI, proves the core loop end to end
(auto-production -> spend to plant -> spend to unlock the greenhouse), and
captures the live-state-matrix screenshots via the game's frame.screenshot
endpoint (glReadPixels backbuffer PNG -- robust on headless/RDP).

Prints named acceptance checks + a compact PASS/FAIL summary.

Usage:
    python tools/cozy-automation/first_screen_smoke.py [--port 9123] [--keep]
"""
from __future__ import annotations

import argparse
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "devapi"))

from devapi_client import running_game, DevApiError  # noqa: E402

CAP_DIR = os.path.join(ROOT, "build", "captures", "cozy")

CHECKS: list[tuple[str, bool, str]] = []


def check(name: str, ok: bool, detail: str = "") -> bool:
    CHECKS.append((name, bool(ok), detail))
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f" -- {detail}" if detail else ""))
    return ok


def set_berries(game, n: int) -> dict:
    game.result("game.state.set", {"path": "cozy.berries", "value": n})
    return game.result("game.state")


def shoot(game, name: str) -> str:
    """Capture a backbuffer PNG via frame.screenshot; wait until written."""
    path = os.path.join(CAP_DIR, f"{name}.png")
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    game.result("frame.screenshot", {"path": rel})
    for _ in range(12):
        game.wait_frames(2)
        st = game.result("frame.screenshot", {})
        if st.get("done"):
            break
    ok = bool(st.get("ok")) and os.path.exists(path) and os.path.getsize(path) > 2000
    check(f"capture:{name}", ok, f"{rel} ({os.path.getsize(path) if os.path.exists(path) else 0} B)")
    return path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=9123)
    ap.add_argument("--keep", action="store_true", help="leave the game running")
    args = ap.parse_args()

    os.makedirs(CAP_DIR, exist_ok=True)

    with running_game(port=args.port, window_size="960x540") as game:
        # --- contract ---
        methods = game.endpoint_methods()
        for m in ("game.state", "game.action.plant", "game.action.unlock",
                  "game.action.tick", "frame.screenshot", "ui.tree"):
            check(f"endpoint:{m}", m in methods)

        game.result("game.reset_playtest")
        game.wait_frames(30)  # let the pack load + atlas resolve
        st = game.observe()
        check("atlas_ready", st.get("atlas_ready") is True, str(st.get("atlas_ready")))
        check("fresh_state", st.get("berries") == 0 and not st.get("plot2_planted")
              and not st.get("greenhouse_unlocked"), str(st))
        check("start_rate_1", st.get("rate") == 1, f"rate={st.get('rate')}")

        # --- first screen / HUD / locked + disabled states (berries=0) ---
        shoot(game, "first_screen")          # first_screen, hud_visible, locked_or_disabled_state

        # --- auto-route: a forced tick raises berries by the rate ---
        before = game.observe()["berries"]
        st = game.result("game.action.tick", {"count": 3})
        check("auto_route_tick", st["berries"] == before + 3 * 1, f"{before} -> {st['berries']}")
        shoot(game, "transient_auto_route")  # transient_stress_state (berries drifting after live ticks)

        # --- primary action ready (affordable) ---
        st = set_berries(game, PLANT := 10)
        check("can_plant_at_10", st.get("can_plant") is True, str(st.get("can_plant")))
        shoot(game, "primary_action_ready")  # primary_action_ready

        # --- primary action feedback: plant changes state ---
        st = game.result("game.action.plant")
        ok = st.get("applied") and st.get("plot2_planted") and st.get("berries") == 0 and st.get("rate") == 2
        check("plant_applied", ok, f"applied={st.get('applied')} planted={st.get('plot2_planted')} "
                                    f"berries={st.get('berries')} rate={st.get('rate')}")
        shoot(game, "primary_action_feedback")  # primary_action_feedback

        # --- the lock: greenhouse unlock at 50 (reward) ---
        st = set_berries(game, 50)
        check("can_unlock_at_50", st.get("can_unlock") is True, str(st.get("can_unlock")))
        st = game.result("game.action.unlock")
        ok = st.get("applied") and st.get("greenhouse_unlocked") and st.get("rate") == 5
        check("greenhouse_unlocked", ok, f"applied={st.get('applied')} "
                                         f"unlocked={st.get('greenhouse_unlocked')} rate={st.get('rate')}")
        shoot(game, "reward_active")         # reward_active / progression

        # --- ui.click drives the loop too (real input path) ---
        game.result("game.reset_playtest")
        set_berries(game, 10)
        game.wait_frames(2)
        try:
            game.click_ui("plant", wait_frames=3)
            st = game.observe()
            check("ui_click_plant", st.get("plot2_planted") is True, str(st.get("plot2_planted")))
        except DevApiError as exc:
            check("ui_click_plant", False, str(exc))

        if args.keep:
            print("  (game left running; ctrl-c to stop)")
            try:
                game.wait_frames(100000)
            except KeyboardInterrupt:
                pass

    passed = sum(1 for _, ok, _ in CHECKS if ok)
    total = len(CHECKS)
    print(f"\n=== Cozy Automation first-screen smoke: {passed}/{total} checks passed ===")
    print(f"captures: {os.path.relpath(CAP_DIR, ROOT)}")
    return 0 if passed == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
