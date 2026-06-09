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
            ok &= check("entity.list returns list", isinstance(entities, list) and len(entities) > 0, len(entities) if isinstance(entities, list) else None)

            tree = game.result("ui.tree")
            viewport = next((node for node in tree if node.get("id") == "scene.viewport"), None)
            test_panel = next((node for node in tree if node.get("id") == "test.ui"), None)
            test_label = next((node for node in tree if node.get("id") == "test.label"), None)
            test_button = next((node for node in tree if node.get("id") == "test.button"), None)
            ok &= check("ui.tree scene.viewport", viewport is not None, tree)
            ok &= check("ui.tree bounds", viewport is not None and all(k in viewport for k in ("x", "y", "w", "h", "center_x", "center_y")), viewport)
            ok &= check("ui.tree test nodes", test_panel is not None and test_label is not None and test_button is not None, tree)
            ok &= check("ui.tree hierarchy", test_panel is not None and set(test_panel.get("children", [])) >= {"test.label", "test.button"}, test_panel)
            ok &= check("ui.tree text", test_label is not None and test_label.get("text") == "Label: waiting" and test_button is not None and test_button.get("text") == "Click me", {"label": test_label, "button": test_button})

            element = game.result("ui.element", {"id": "scene.viewport"})
            ok &= check("ui.element viewport", element.get("id") == "scene.viewport" and element.get("w", 0) > 0 and element.get("h", 0) > 0, element)
            button_element = game.result("ui.element", {"id": "test.button"})
            ok &= check("ui.element button detail", button_element.get("role") == "button" and button_element.get("text") == "Click me" and button_element.get("enabled") is True, button_element)
            ok &= check("ui.element invalid fails", is_error(game.request("ui.element", {"id": "missing"})))

            state0 = game.observe()
            ok &= check("game.state shape", all(k in state0 for k in ("shape", "shape_index", "render_mode", "camera_distance", "grabbed", "frame")), state0)

            batch = game.batch([
                ("input.key", {"key": "D", "mode": "tap"}),
                ("frame.wait", {"frames": 2}),
                ("game.state", {}),
            ])
            ok &= check("batch ordered responses", isinstance(batch, list) and len(batch) == 3 and all(item.get("ok") for item in batch), batch)
            state1 = batch[2]["result"]
            ok &= check("input.key tap changes shape", state1["shape_index"] != state0["shape_index"], {"before": state0["shape_index"], "after": state1["shape_index"]})

            before_mode = state1["render_mode"]
            batch = game.batch([
                ("input.key", {"key": "W", "mode": "down"}),
                ("frame.wait", {"frames": 1}),
                ("input.key", {"key": "W", "mode": "up"}),
                ("frame.wait", {"frames": 1}),
                ("game.state", {}),
            ])
            ok &= check("input.key down/up ok", all(item.get("ok") for item in batch), batch)
            ok &= check("input.key down changes render mode", batch[-1]["result"]["render_mode"] != before_mode, {"before": before_mode, "after": batch[-1]["result"]["render_mode"]})

            ok &= check("input.move ok", is_ok(game.request("input.move", {"x": 120, "y": 140})))
            ok &= check("input.click ok", is_ok(game.request("input.click", {"x": 480, "y": 320, "button": "left"})))
            state_click = game.batch_results([("frame.wait", {"frames": 3}), ("game.state", {})])[-1]
            ok &= check("input.click reaches game", isinstance(state_click.get("grabbed"), bool), state_click)

            before_cam = state_click["camera_distance"]
            ok &= check("input.wheel ok", is_ok(game.request("input.wheel", {"x": 480, "y": 320, "dx": 0, "dy": -120})))
            state_wheel = game.batch_results([("frame.wait", {"frames": 1}), ("game.state", {})])[-1]
            ok &= check("input.wheel changes camera", state_wheel["camera_distance"] != before_cam, {"before": before_cam, "after": state_wheel["camera_distance"]})

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
            before_cam = game.observe()["camera_distance"]
            ok &= check("input.gesture scroll ok", is_ok(game.request("input.gesture", {"type": "scroll", "x": 480, "y": 320, "dy": 120})))
            after_cam = game.batch_results([("frame.wait", {"frames": 1}), ("game.state", {})])[-1]["camera_distance"]
            ok &= check("input.gesture scroll changes camera", after_cam != before_cam, {"before": before_cam, "after": after_cam})

            ok &= check("ui.click ok", is_ok(game.request("ui.click", {"id": "scene.viewport", "button": "left"})))
            game.wait_frames(2)
            ok &= check("ui.click button ok", is_ok(game.request("ui.click", {"id": "test.button", "button": "left"})))
            game.wait_frames(2)
            clicked_button = game.result("ui.element", {"id": "test.button"})
            clicked_label = game.result("ui.element", {"id": "test.label"})
            clicked_state = game.observe()
            ok &= check("ui.click button changes button text", clicked_button.get("text") == "Clicked 1", clicked_button)
            ok &= check("ui.click button changes label text", clicked_label.get("text") == "Label: clicked 1", clicked_label)
            ok &= check("game.state exposes test ui text", clicked_state.get("test_button_text") == "Clicked 1" and clicked_state.get("test_label_text") == "Label: clicked 1", clicked_state)
            before_cam = game.observe()["camera_distance"]
            ok &= check("ui.scroll ok", is_ok(game.request("ui.scroll", {"id": "scene.viewport", "dy": -120})))
            after_cam = game.batch_results([("frame.wait", {"frames": 1}), ("game.state", {})])[-1]["camera_distance"]
            ok &= check("ui.scroll changes camera", after_cam != before_cam, {"before": before_cam, "after": after_cam})
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
