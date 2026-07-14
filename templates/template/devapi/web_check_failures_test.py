import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


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


if __name__ == "__main__":
    unittest.main()
