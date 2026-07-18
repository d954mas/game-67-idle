import base64
import json
import os
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from devapi_client import (
    DEFAULT_DEVAPI_PORT,
    DevApiClient,
    DevApiError,
    connect_existing,
    pick_free_port,
    resolve_launch_port,
    write_engine_capture_payload_png,
)


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


class DuplexResponseFile:
    def __init__(self, response):
        self.response = response
        self.writes = []
        self.closed = False

    def write(self, payload):
        self.writes.append(payload)
        return len(payload)

    def flush(self):
        return None

    def readline(self):
        return self.response

    def close(self):
        self.closed = True


class DevApiSocketBufferingTest(unittest.TestCase):
    def test_large_response_uses_buffered_socket_file(self):
        result = {"format": "png", "data": "x" * 700_000}
        response = (json.dumps({"ok": True, "result": result}) + "\n").encode("utf-8")
        file = DuplexResponseFile(response)
        sock = mock.Mock()
        sock.makefile.return_value = file

        with mock.patch("devapi_client.socket.create_connection", return_value=sock):
            client = DevApiClient(port=19001)
            try:
                self.assertEqual(client.result("capture.frame"), result)
            finally:
                client.close()

        sock.makefile.assert_called_once_with("rwb")
        self.assertEqual(
            json.loads(file.writes[0]),
            {"request_id": "1", "method": "capture.frame", "params": {}},
        )
        self.assertTrue(file.closed)
        sock.close.assert_called_once_with()


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

    def test_player_gated_disables_then_reenables_player_input(self):
        client = FakeDevApiClient()
        with client.player_gated():
            client.click_ui("settings/gear")
        self.assertEqual(
            [(method, params) for (_, method, params) in client.calls if method == "input.set_player_enabled"],
            [("input.set_player_enabled", {"enabled": False}), ("input.set_player_enabled", {"enabled": True})],
        )

    def test_player_gated_reenables_player_input_even_when_body_raises(self):
        client = FakeDevApiClient()
        with self.assertRaises(RuntimeError):
            with client.player_gated():
                raise RuntimeError("boom")
        self.assertEqual(
            [(method, params) for (_, method, params) in client.calls if method == "input.set_player_enabled"],
            [("input.set_player_enabled", {"enabled": False}), ("input.set_player_enabled", {"enabled": True})],
        )

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


class LaunchPortResolutionTest(unittest.TestCase):
    def test_pick_free_port_returns_a_bindable_port(self):
        port = pick_free_port()
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.bind(("127.0.0.1", port))

    def test_resolve_launch_port_explicit_arg_wins_over_env(self):
        with mock.patch.dict(os.environ, {"AI_STUDIO_DEVAPI_PORT": "18000"}, clear=False):
            self.assertEqual(resolve_launch_port(19000), 19000)

    def test_resolve_launch_port_uses_env_override_when_no_arg(self):
        with mock.patch.dict(os.environ, {"AI_STUDIO_DEVAPI_PORT": "18000", "NT_DEVAPI_PORT": ""}, clear=False):
            self.assertEqual(resolve_launch_port(None), 18000)

    def test_resolve_launch_port_falls_back_to_nt_devapi_port_env(self):
        with mock.patch.dict(os.environ, {"NT_DEVAPI_PORT": "18500"}, clear=False):
            os.environ.pop("AI_STUDIO_DEVAPI_PORT", None)
            self.assertEqual(resolve_launch_port(None), 18500)

    def test_resolve_launch_port_picks_free_port_when_no_arg_and_no_env(self):
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop("AI_STUDIO_DEVAPI_PORT", None)
            os.environ.pop("NT_DEVAPI_PORT", None)
            port = resolve_launch_port(None)
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
                probe.bind(("127.0.0.1", port))


class LaunchPreflightDeadChildTest(unittest.TestCase):
    def test_connect_existing_keeps_fast_connect_and_normal_request_timeout(self):
        client = mock.Mock()
        with mock.patch("devapi_client.DevApiClient", return_value=client) as client_type:
            connected = connect_existing(port=19001, timeout=0.1)

        self.assertIs(connected, client)
        client_type.assert_called_once_with(port=19001, timeout=1.0)
        client.sock.settimeout.assert_called_once_with(5.0)

    def test_connect_existing_closes_client_when_request_timeout_is_invalid(self):
        client = mock.Mock()
        client.sock.settimeout.side_effect = ValueError("Timeout value out of range")
        with mock.patch("devapi_client.DevApiClient", return_value=client):
            with self.assertRaises(ValueError):
                connect_existing(port=19001, timeout=0.1, request_timeout=-1.0)

        client.close.assert_called_once_with()

    def test_connect_existing_raises_informative_error_when_child_exits_immediately(self):
        port = pick_free_port()
        proc = subprocess.Popen([sys.executable, "-c", "import sys; sys.exit(7)"])
        try:
            proc.wait(timeout=5)
            with self.assertRaises(DevApiError) as ctx:
                connect_existing(port=port, timeout=3.0, process=proc)
            message = str(ctx.exception)
            self.assertIn("exit code 7", message)
            self.assertIn(str(port), message)
            self.assertIn("port bind failed", message)
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5)

    def test_connect_existing_includes_launch_log_tail_when_available(self):
        port = pick_free_port()
        proc = subprocess.Popen([sys.executable, "-c", "import sys; sys.exit(7)"])
        try:
            proc.wait(timeout=5)
            with tempfile.TemporaryDirectory() as tmp:
                log_path = os.path.join(tmp, "launch.log")
                with open(log_path, "w", encoding="utf-8") as handle:
                    handle.write("some engine startup line\nbind failed on port\n")
                with self.assertRaises(DevApiError) as ctx:
                    connect_existing(port=port, timeout=3.0, process=proc, launch_log_path=log_path)
                self.assertIn("bind failed on port", str(ctx.exception))
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
