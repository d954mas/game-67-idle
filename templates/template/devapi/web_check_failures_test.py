import contextlib
import io
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import MagicMock, patch


WEB_TESTS = Path(__file__).resolve().parents[1] / "tests"
sys.path.insert(0, str(WEB_TESTS))
import web_devapi_check as devapi  # noqa: E402
import web_persistence_check as persistence  # noqa: E402


class WebCheckFailureTest(unittest.TestCase):
    def exit_from(self, module, failure):
        output = io.StringIO()
        with patch.object(module, "run", side_effect=failure), \
                patch.object(sys, "argv", [module.__file__]), \
                contextlib.redirect_stdout(output), \
                self.assertRaises(SystemExit) as raised:
            module.main()
        return raised.exception.code, output.getvalue()

    def test_product_failure_is_not_reported_as_host_skip(self):
        failure_type = getattr(persistence, "CheckFailure", None)
        self.assertIsNotNone(failure_type, "web checks need a distinct product-failure outcome")
        for module in (persistence, devapi):
            with self.subTest(module=module.__name__):
                code, output = self.exit_from(module, failure_type("broken product proof"))
                self.assertEqual(code, 1)
                self.assertIn("FAIL: broken product proof", output)

    def test_missing_host_prerequisite_remains_skip(self):
        for module in (persistence, devapi):
            with self.subTest(module=module.__name__):
                code, output = self.exit_from(module, persistence.Skip("missing host tool"))
                self.assertEqual(code, 2)
                self.assertIn("SKIP", output)

    def test_failed_wasm_configure_is_a_product_failure(self):
        completed = MagicMock(returncode=7)
        with patch.object(persistence, "find_emscripten_toolchain_file", return_value="toolchain.cmake"), \
                patch.object(persistence.subprocess, "run", return_value=completed), \
                self.assertRaisesRegex(persistence.CheckFailure, "cmake configure failed"):
            persistence.build_wasm_devapi()

    def test_missing_devapi_html_is_a_product_failure(self):
        with TemporaryDirectory() as build_dir, \
                patch.object(devapi, "build_wasm_devapi", return_value=build_dir), \
                self.assertRaisesRegex(persistence.CheckFailure, "no index.html shell"):
            devapi.run(None, 8935, 9334)

    def test_set_and_save_rejections_are_product_failures(self):
        for rejected_method in ("game.state.set", "game.state.save"):
            with self.subTest(method=rejected_method), TemporaryDirectory() as build_dir:
                (Path(build_dir) / "index.html").write_text("", encoding="utf8")
                cdp = MagicMock()
                responses = {
                    "game.state.set": {"ok": rejected_method != "game.state.set"},
                    "game.state.save": {"ok": rejected_method != "game.state.save"},
                }
                cdp.devapi_submit.side_effect = lambda method, _params: responses[method]
                server = MagicMock()
                with patch.object(persistence, "build_wasm_devapi", return_value=build_dir), \
                        patch.object(persistence, "find_chrome", return_value="chrome"), \
                        patch.object(persistence, "serve_dir", return_value=server), \
                        patch.object(persistence, "launch_chrome", return_value=MagicMock()), \
                        patch.object(persistence, "cdp_page_ws_url", return_value="ws://fake"), \
                        patch.object(persistence, "Cdp", return_value=cdp), \
                        self.assertRaisesRegex(persistence.CheckFailure, rejected_method.replace(".", r"\.")):
                    persistence.run("build", None, 8934, 9333, False)


if __name__ == "__main__":
    unittest.main()
