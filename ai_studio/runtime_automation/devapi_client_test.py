import base64
import os
import tempfile
import unittest

from devapi_client import DEFAULT_DEVAPI_PORT, DevApiClient, DevApiError, write_engine_capture_payload_png


class FakeDevApiClient(DevApiClient):
    def __init__(self):
        self.calls = []

    def result(self, method, params=None):
        self.calls.append(("result", method, params or {}))
        if method == "endpoints":
            return {"commands": [{"method": "ui.tree"}, {"method": 42}, {"group": "bad"}, "legacy.ping"]}
        if method == "frame.current":
            return {"frame": 12}
        if method == "game.state":
            return {"state": "ok"}
        return {"method": method, "params": params or {}}

    def wait_frames(self, frames=1):
        self.calls.append(("wait_frames", frames, {}))
        return {"frame": 11}


class EngineCapturePayloadTest(unittest.TestCase):
    def test_default_port_matches_engine_devapi_default(self):
        self.assertEqual(DEFAULT_DEVAPI_PORT, 17890)

    def test_runtime_helper_modules_are_importable_after_client_import(self):
        __import__("pixel_health")
        __import__("png_io")

    def test_endpoint_methods_flattens_engine_command_listing(self):
        client = FakeDevApiClient()
        self.assertEqual(client.endpoint_methods(), {"ui.tree", "legacy.ping"})

    def test_click_ui_defaults_to_engine_owned_frame_observation(self):
        client = FakeDevApiClient()
        self.assertEqual(client.click_ui("settings/gear"), {"frame": 12})
        self.assertNotIn(("result", "game.state", {}), client.calls)

    def test_click_ui_can_observe_game_state_when_a_game_registers_it(self):
        client = FakeDevApiClient()
        self.assertEqual(client.click_ui("settings/gear", observe="game.state"), {"state": "ok"})

    def test_write_engine_capture_payload_decodes_base64_png(self):
        png = b"\x89PNG\r\n\x1a\npayload"
        payload = {
            "width": 2,
            "height": 1,
            "format": "png",
            "data": base64.b64encode(png).decode("ascii"),
        }
        with tempfile.TemporaryDirectory() as tmp:
            out_path = os.path.join(tmp, "shot.png")
            self.assertEqual(write_engine_capture_payload_png(payload, out_path), out_path)
            with open(out_path, "rb") as handle:
                self.assertEqual(handle.read(), png)

    def test_write_engine_capture_payload_rejects_non_png_payload(self):
        payload = {
            "width": 2,
            "height": 1,
            "format": "ppm",
            "data": base64.b64encode(b"payload").decode("ascii"),
        }
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(DevApiError):
                write_engine_capture_payload_png(payload, os.path.join(tmp, "shot.png"))


if __name__ == "__main__":
    unittest.main()
