import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import responsive_viewports as rv  # noqa: E402

sys.path.insert(0, str(rv.RUNTIME_AUTOMATION if hasattr(rv, "RUNTIME_AUTOMATION") else rv.REPO_ROOT / "ai_studio" / "runtime_automation"))
from png_io import read_png, write_png_rgb  # noqa: E402


class FakeGame:
    def __init__(self, viewport):
        self.viewport = viewport
        self.waits = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        return None

    def result(self, method, params=None):
        if method == "ui.tree":
            return {
                "width": self.viewport.width,
                "height": self.viewport.height,
                "viewport": {"x": 0, "y": 0, "w": self.viewport.width, "h": self.viewport.height},
                "nodes": [{"id_string": "settings/gear"}, {"id_string": ""}],
            }
        if method == "view":
            return {"width": self.viewport.width, "height": self.viewport.height}
        raise AssertionError(f"unexpected method: {method}")

    def wait_frames(self, frames=1):
        self.waits.append(frames)
        return {"frame": len(self.waits)}

    def capture_screenshot(self, output, wait_frames=1, audit=True):
        color = bytes((self.viewport.width % 255, self.viewport.height % 255, 96))
        write_png_rgb(output, 2, 1, color * 2)
        return output


class FakeManualGame(FakeGame):
    def __init__(self, viewport):
        super().__init__(viewport)
        self.time_mode = "run"
        self.steps = []
        self.capture_wait_frames = None

    def endpoint_methods(self):
        return {"time.step", "time.set_mode", "ui.tree", "capture.frame"}

    def result(self, method, params=None):
        params = params or {}
        if method == "time.set_mode":
            self.time_mode = params["mode"]
            return {"mode": self.time_mode}
        if method == "time.step":
            if self.time_mode != "manual":
                raise AssertionError("manual mode required")
            self.steps.append(params["count"])
            return {"frames": params["count"]}
        return super().result(method, params)

    def wait_frames(self, frames=1):
        raise TimeoutError("frame.wait is intentionally unavailable")

    def capture_screenshot(self, output, wait_frames=1, audit=True):
        self.capture_wait_frames = wait_frames
        return super().capture_screenshot(output, wait_frames=wait_frames, audit=audit)


class ResponsiveViewportsTest(unittest.TestCase):
    def test_parse_viewport_accepts_named_size(self):
        viewport = rv.parse_viewport("phone=390x844")
        self.assertEqual((viewport.name, viewport.width, viewport.height, viewport.orientation), ("phone", 390, 844, "portrait"))

    def test_selected_viewports_can_filter_landscape(self):
        viewports = rv.selected_viewports([], landscape_only=True)
        self.assertTrue(viewports)
        self.assertTrue(all(viewport.orientation == "landscape" for viewport in viewports))

    def test_make_contact_sheet_writes_combined_png(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = root / "a.png"
            b = root / "b.png"
            write_png_rgb(str(a), 2, 1, b"\x00\x00\x00\xff\x00\x00")
            write_png_rgb(str(b), 1, 2, b"\x00\xff\x00\x00\x00\xff")
            out = root / "sheet.png"
            self.assertEqual(rv.make_contact_sheet([{"name": "a", "screenshot": str(a)}, {"name": "b", "screenshot": str(b)}], out, columns=2, padding=1), str(out))
            width, height, _color_type, _pixels = read_png(str(out))
            self.assertEqual((width, height), (7, 4))

    def test_run_matrix_writes_qclr_summary_and_screenshots(self):
        with tempfile.TemporaryDirectory() as tmp:
            viewports = [rv.ViewportCase("wide", 640, 360), rv.ViewportCase("phone", 390, 844)]
            summary = rv.run_matrix(lambda viewport: FakeGame(viewport), viewports, Path(tmp), audit=False, warmup_frames=1)
            self.assertEqual(summary["quality_rule"], "QCLR_002")
            self.assertEqual(len(summary["viewports"]), 2)
            self.assertTrue(Path(summary["summary"]).exists())
            self.assertTrue(Path(summary["contact_sheet"]).exists())
            self.assertTrue(Path(summary["viewports"][0]["screenshot"]).exists())

    def test_run_matrix_calls_prepare_before_capture(self):
        calls = []

        def prepare(game, viewport):
            calls.append((viewport.name, list(game.waits)))
            return {"prepared": viewport.window_size}

        with tempfile.TemporaryDirectory() as tmp:
            viewports = [rv.ViewportCase("wide", 640, 360)]
            summary = rv.run_matrix(lambda viewport: FakeGame(viewport), viewports, Path(tmp), audit=False, warmup_frames=2, prepare=prepare)
            self.assertEqual(calls, [("wide", [2])])
            self.assertEqual(summary["viewports"][0]["scenario_result"], {"prepared": "640x360"})

    def test_capture_uses_manual_time_when_native_frame_wait_is_unavailable(self):
        with tempfile.TemporaryDirectory() as tmp:
            viewport = rv.ViewportCase("manual", 480, 640)
            game = FakeManualGame(viewport)
            rv.capture_viewport(game, viewport, Path(tmp), audit=False, warmup_frames=5)
            self.assertEqual(game.steps, [5, 2])
            self.assertEqual(game.capture_wait_frames, 0)
            self.assertEqual(game.time_mode, "run")


if __name__ == "__main__":
    unittest.main()
