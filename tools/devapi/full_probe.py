#!/usr/bin/env python3
"""Probe every temporary game DevAPI endpoint on a fresh native game process."""

from __future__ import annotations

import sys
from typing import Any

from devapi_client import DevApiError, running_game

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123

EXPECTED_ENDPOINTS = {
    "ping",
    "endpoints",
    "view",
    "frame.current",
    "frame.wait",
    "entity.list",
    "ui.tree",
    "ui.element",
    "ui.click",
    "ui.drag",
    "ui.scroll",
    "input.key",
    "input.move",
    "input.click",
    "input.pointer",
    "input.wheel",
    "input.gesture",
    "input.button",
    "game.state",
    "game.reset_playtest",
    "game.state.schema",
    "game.state.get",
    "game.state.set",
    "game.state.patch",
    "game.state.save",
    "game.state.load",
    "game.state.reset",
}


try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass


def is_ok(response: Any) -> bool:
    return isinstance(response, dict) and response.get("ok") is True


def is_error(response: Any) -> bool:
    return isinstance(response, dict) and response.get("ok") is False


def check(name: str, condition: bool, detail: Any = "") -> bool:
    print(("PASS" if condition else "FAIL"), name, "::", detail)
    return condition


def node_by_id(tree: list[dict[str, Any]], element_id: str) -> dict[str, Any] | None:
    return next((node for node in tree if node.get("id") == element_id), None)


def main() -> int:
    ok = True
    try:
        with running_game(port=PORT) as game:
            ok &= check("ping", is_ok(game.request("ping")))

            endpoints = set(game.result("endpoints"))
            ok &= check("endpoints complete", EXPECTED_ENDPOINTS <= endpoints, {"missing": sorted(EXPECTED_ENDPOINTS - endpoints), "extra": sorted(endpoints - EXPECTED_ENDPOINTS)})

            view = game.result("view")
            ok &= check("view shape", view.get("fb_w", 0) > 0 and view.get("fb_h", 0) > 0 and view.get("logical_w", 0) > 0 and view.get("logical_h", 0) > 0, view)

            frame0 = game.result("frame.current")["frame"]
            frame1 = game.wait_frames(2)["frame"]
            ok &= check("frame.wait advances", frame1 >= frame0 + 2, {"before": frame0, "after": frame1})

            entities = game.result("entity.list")
            ok &= check("entity.list returns list", isinstance(entities, list), len(entities) if isinstance(entities, list) else None)

            tree = game.result("ui.tree")
            viewport = node_by_id(tree, "scene.viewport")
            main_root = node_by_id(tree, "main.root")
            do67_button = node_by_id(tree, "main.do67")
            coins_label = node_by_id(tree, "main.coins")
            status_label = node_by_id(tree, "main.status")
            upgrade_button = node_by_id(tree, "main.upgrade.first")
            job_button = node_by_id(tree, "main.job.first")
            reset_button = node_by_id(tree, "main.reset")
            ok &= check("ui.tree scene.viewport", viewport is not None, tree)
            ok &= check("ui.tree bounds", viewport is not None and all(k in viewport for k in ("x", "y", "w", "h", "center_x", "center_y")), viewport)
            ok &= check("ui.tree gameplay nodes", all(node is not None for node in (main_root, do67_button, coins_label, status_label, upgrade_button, job_button, reset_button)), tree)
            ok &= check(
                "ui.tree main hierarchy",
                main_root is not None
                and set(main_root.get("children", [])) >= {"main.do67", "main.coins", "main.status", "main.upgrade.first", "main.job.first", "main.reset"},
                main_root,
            )
            ok &= check(
                "ui.tree gameplay roles",
                do67_button is not None
                and do67_button.get("role") == "button"
                and coins_label is not None
                and coins_label.get("role") == "label"
                and status_label is not None
                and status_label.get("role") == "label",
                {"do67": do67_button, "coins": coins_label, "status": status_label},
            )

            element = game.result("ui.element", {"id": "scene.viewport"})
            ok &= check("ui.element viewport", element.get("id") == "scene.viewport" and element.get("w", 0) > 0 and element.get("h", 0) > 0, element)
            button_element = game.result("ui.element", {"id": "main.do67"})
            ok &= check("ui.element button detail", button_element.get("role") == "button" and button_element.get("enabled") is True, button_element)
            ok &= check("ui.element invalid fails", is_error(game.request("ui.element", {"id": "missing"})))

            state0 = game.observe()
            ok &= check("game.state shape", isinstance(state0, dict) and all(k in state0 for k in ("frame", "meme_coins", "status")), state0)

            batch = game.batch([
                ("input.key", {"key": "D", "mode": "tap"}),
                ("frame.wait", {"frames": 2}),
                ("game.state", {}),
            ])
            ok &= check("batch ordered responses", isinstance(batch, list) and len(batch) == 3 and all(item.get("ok") for item in batch), batch)
            state1 = batch[2]["result"]
            ok &= check("input.key tap returns state", isinstance(state1, dict) and "frame" in state1, state1)

            batch = game.batch([
                ("input.key", {"key": "W", "mode": "down"}),
                ("frame.wait", {"frames": 1}),
                ("input.key", {"key": "W", "mode": "up"}),
                ("frame.wait", {"frames": 1}),
                ("game.state", {}),
            ])
            ok &= check("input.key down/up ok", all(item.get("ok") for item in batch), batch)
            ok &= check("input.key down/up returns state", isinstance(batch[-1].get("result"), dict) and "frame" in batch[-1]["result"], batch[-1])

            ok &= check("input.move ok", is_ok(game.request("input.move", {"x": 120, "y": 140})))
            ok &= check("input.click ok", is_ok(game.request("input.click", {"x": 480, "y": 320, "button": "left"})))
            state_click = game.batch_results([("frame.wait", {"frames": 3}), ("game.state", {})])[-1]
            ok &= check("input.click keeps state readable", isinstance(state_click, dict) and "frame" in state_click, state_click)

            ok &= check("input.wheel ok", is_ok(game.request("input.wheel", {"x": 480, "y": 320, "dx": 0, "dy": -120})))
            state_wheel = game.batch_results([("frame.wait", {"frames": 1}), ("game.state", {})])[-1]
            ok &= check("input.wheel keeps state readable", isinstance(state_wheel, dict) and "frame" in state_wheel, state_wheel)

            ok &= check("input.pointer down ok", is_ok(game.request("input.pointer", {"phase": "down", "id": 2, "x": 300, "y": 250, "button": "left"})))
            ok &= check("input.pointer move ok", is_ok(game.request("input.pointer", {"phase": "move", "id": 2, "x": 340, "y": 250, "buttons_mask": 1})))
            ok &= check("input.pointer up ok", is_ok(game.request("input.pointer", {"phase": "up", "id": 2})))
            game.wait_frames(2)

            ok &= check("input.button down ok", is_ok(game.request("input.button", {"button": "left", "state": "down"})))
            game.wait_frames(1)
            ok &= check("input.button up ok", is_ok(game.request("input.button", {"button": "left", "state": "up"})))
            game.wait_frames(1)

            ok &= check("input.gesture tap ok", is_ok(game.request("input.gesture", {"type": "tap", "x": 200, "y": 180, "frames": 1})))
            game.wait_frames(2)
            ok &= check("input.gesture drag ok", is_ok(game.request("input.gesture", {"type": "drag", "from_x": 200, "from_y": 180, "to_x": 260, "to_y": 180, "frames": 4})))
            game.wait_frames(6)
            ok &= check("input.gesture scroll ok", is_ok(game.request("input.gesture", {"type": "scroll", "x": 480, "y": 320, "dy": 120})))
            after_scroll_state = game.batch_results([("frame.wait", {"frames": 1}), ("game.state", {})])[-1]
            ok &= check("input.gesture scroll keeps gameplay state readable", isinstance(after_scroll_state.get("meme_coins"), int), after_scroll_state)

            ok &= check("ui.click ok", is_ok(game.request("ui.click", {"id": "scene.viewport", "button": "left"})))
            game.wait_frames(2)
            ok &= check("ui.click button ok", is_ok(game.request("ui.click", {"id": "main.do67", "button": "left"})))
            game.wait_frames(2)
            clicked_button = game.result("ui.element", {"id": "main.do67"})
            clicked_label = game.result("ui.element", {"id": "main.coins"})
            clicked_state = game.observe()
            ok &= check("ui.click current button readable", clicked_button.get("role") == "button" and clicked_button.get("id") == "main.do67", clicked_button)
            ok &= check("ui.click current label readable", clicked_label.get("role") == "label" and clicked_label.get("id") == "main.coins", clicked_label)
            ok &= check("game.state remains readable after ui.click", isinstance(clicked_state, dict) and "frame" in clicked_state, clicked_state)
            ok &= check("ui.scroll ok", is_ok(game.request("ui.scroll", {"id": "scene.viewport", "dy": -120})))
            after_scroll_state = game.batch_results([("frame.wait", {"frames": 1}), ("game.state", {})])[-1]
            ok &= check("ui.scroll keeps gameplay state readable", isinstance(after_scroll_state.get("meme_coins"), int), after_scroll_state)
            ok &= check("ui.drag ok", is_ok(game.request("ui.drag", {"id": "scene.viewport", "dx": 60, "dy": 0, "frames": 3})))
            game.wait_frames(5)

            ok &= check("unknown method fails", is_error(game.request("missing.method")))
            ok &= check("invalid key fails", is_error(game.request("input.key", {"key": "NOPE", "mode": "tap"})))
            ok &= check("invalid pointer phase fails", is_error(game.request("input.pointer", {"phase": "bad"})))
            ok &= check("too large wait fails", is_error(game.request("frame.wait", {"frames": 999999})))
            ok &= check("too large gesture fails", is_error(game.request("input.gesture", {"type": "drag", "frames": 999})))
            big = game.raw({"request_id": "big", "method": "ping", "params": {"blob": "x" * 20000}})
            ok &= check("oversized request fails structured", is_error(big) and big.get("error") == "request line too large", big)
            ok &= check("connection alive after oversized request", is_ok(game.request("ping")))
    except DevApiError as exc:
        print("FAIL devapi ::", exc)
        return 1

    print("\n=== %s ===" % ("ALL DEVAPI COMMANDS PASSED" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
