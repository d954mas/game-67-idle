#!/usr/bin/env python3
"""Smoke-test the running scene through the temporary game DevAPI."""

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
            ok &= check("endpoints", "game.state" in endpoints.get("result", []), endpoints)
            endpoint_names = endpoints.get("result", [])
            ok &= check(
                "ui/input endpoints",
                all(name in endpoint_names for name in ("ui.tree", "ui.element", "ui.click", "ui.drag", "ui.scroll", "input.pointer", "input.wheel", "input.gesture")),
                endpoints,
            )

            ui_tree = bus.request("ui.tree")
            ui_ids = [item.get("id") for item in ui_tree.get("result", [])]
            ok &= check("ui tree exposes viewport", "scene.viewport" in ui_ids, ui_tree)
            ok &= check("ui tree exposes test ui", all(item in ui_ids for item in ("test.ui", "test.label", "test.button")), ui_tree)
            viewport = bus.request("ui.element", {"id": "scene.viewport"})
            ok &= check("ui element uses string id", viewport.get("result", {}).get("id") == "scene.viewport", viewport)
            button0 = bus.request("ui.element", {"id": "test.button"}).get("result", {})
            ok &= check("ui button text initial", button0.get("role") == "button" and button0.get("text") == "Click me", button0)

            state0 = bus.observe()
            ok &= check("state shape", state0.get("shape") == "cube", state0)

            batch1 = bus.batch([
                ("input.key", {"key": "D", "mode": "tap"}),
                ("frame.wait", {"frames": 1}),
                ("game.state", {}),
            ])
            ok &= check("input batch waits for frame", isinstance(batch1, list) and len(batch1) == 3, batch1)
            state1 = batch1[2]["result"]
            ok &= check("input changes shape", state1.get("shape") != state0.get("shape"), f"{state0.get('shape')} -> {state1.get('shape')}")

            state2 = bus.key_tap("W")
            ok &= check("input changes render mode", state2.get("render_mode") != state1.get("render_mode"), f"{state1.get('render_mode')} -> {state2.get('render_mode')}")

            double_tap = bus.batch([
                ("input.key", {"key": "D", "mode": "tap"}),
                ("input.key", {"key": "D", "mode": "tap"}),
                ("frame.wait", {"frames": 4}),
                ("game.state", {}),
            ])
            ok &= check("same-key taps stay ordered", isinstance(double_tap, list) and len(double_tap) == 4, double_tap)
            state3 = double_tap[3]["result"]
            expected_shape = (state2.get("shape_index") + 2) % 4
            ok &= check("double tap advances twice", state3.get("shape_index") == expected_shape, f"{state2.get('shape_index')} -> {state3.get('shape_index')}")

            state4 = bus.click_ui("scene.viewport", wait_frames=2)
            ok &= check("ui click reaches game input", isinstance(state4.get("grabbed"), bool), state4)

            state_button = bus.click_ui("test.button", wait_frames=2)
            button1 = bus.request("ui.element", {"id": "test.button"}).get("result", {})
            label1 = bus.request("ui.element", {"id": "test.label"}).get("result", {})
            ok &= check("ui button click changes button text", button1.get("text") == "Clicked 1", button1)
            ok &= check("ui button click changes label text", label1.get("text") == "Label: clicked 1", label1)
            ok &= check("state exposes ui text", state_button.get("test_button_text") == "Clicked 1" and state_button.get("test_label_text") == "Label: clicked 1", state_button)

            state5 = bus.scroll_ui("scene.viewport", dy=-120)
            ok &= check("ui scroll changes camera", state5.get("camera_distance") != state_button.get("camera_distance"), f"{state_button.get('camera_distance')} -> {state5.get('camera_distance')}")

            state6 = bus.gesture("scroll", {"x": 480, "y": 320, "dy": 120})
            ok &= check("gesture scroll changes camera", state6.get("camera_distance") != state5.get("camera_distance"), f"{state5.get('camera_distance')} -> {state6.get('camera_distance')}")

            drag = bus.batch([
                ("ui.drag", {"id": "scene.viewport", "dx": 80, "dy": 0, "frames": 4}),
                ("frame.wait", {"frames": 6}),
                ("game.state", {}),
            ])
            ok &= check("ui drag batch", isinstance(drag, list) and len(drag) == 3, drag)

        print("\n=== %s ===" % ("ALL PASSED" if ok else "FAILED"))
        return 0 if ok else 1
    except DevApiError as exc:
        print("FAIL devapi ::", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
