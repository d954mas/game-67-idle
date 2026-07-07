#!/usr/bin/env python3
"""OFFLINE unit tests: no model download, no network. Never call
``rembg.new_session`` here -- ``BaseSession.__init__`` downloads the ONNX
checkpoint immediately (see ``birefnet_smoke.py`` for the LIVE model run).
The session-injection path exercises rembg's REAL ``remove()``/
``naive_cutout()`` plumbing against a stub session object -- rembg's own
``remove()`` only ever calls ``session.predict(img)``, so a stub with just
that method is not brittle.
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.image.birefnet_cutout.birefnet_cutout import (
    ALLOWED_MODELS,
    REPORT_SCHEMA,
    _check_model_allowed,
    birefnet_cutout,
    main,
    run,
)

FORBIDDEN_MODEL = "RMBG-2.0"


class _StubSession:
    """Minimal stand-in for a rembg ``BaseSession``: only the ``predict``
    method that ``rembg.remove()`` actually calls. Returns an all-opaque
    L-mode mask sized to the image, so the whole round trip through rembg's
    real cutout compositing runs end-to-end without any real model."""

    def predict(self, img: Image.Image, *args, **kwargs):
        return [Image.new("L", img.size, 255)]


def _tiny_rgb(width: int = 16, height: int = 12, color=(10, 20, 30)) -> Image.Image:
    return Image.new("RGB", (width, height), color)


class ModelAllowlistTests(unittest.TestCase):
    def test_allowed_model_passes(self) -> None:
        self.assertEqual(ALLOWED_MODELS, ("birefnet-general",))
        _check_model_allowed("birefnet-general")  # must not raise

    def test_forbidden_rmbg_model_is_refused_with_license_message(self) -> None:
        with self.assertRaises(ValueError) as ctx:
            _check_model_allowed(FORBIDDEN_MODEL)
        message = str(ctx.exception)
        self.assertIn(FORBIDDEN_MODEL, message)
        self.assertIn("NON-COMMERCIAL", message)
        self.assertIn("birefnet-general", message)

    def test_arbitrary_unknown_model_is_refused(self) -> None:
        with self.assertRaises(ValueError):
            _check_model_allowed("some-other-session")

    def test_birefnet_cutout_entry_point_rejects_forbidden_model_before_touching_rembg(self) -> None:
        # No session/image work should happen for a disallowed model -- the
        # allowlist check must fire first, so this must never try to import
        # or call rembg.new_session (which would attempt a network download).
        with self.assertRaises(ValueError):
            birefnet_cutout(_tiny_rgb(), model=FORBIDDEN_MODEL)


class SessionInjectionTests(unittest.TestCase):
    def test_stub_session_round_trips_through_real_rembg_remove(self) -> None:
        image = _tiny_rgb(16, 12)
        result = birefnet_cutout(image, session=_StubSession())
        self.assertEqual(result.mode, "RGBA")
        self.assertEqual(result.size, (16, 12))
        alpha = np.asarray(result)[..., 3]
        self.assertTrue(np.all(alpha == 255), "stub predict() returned an opaque mask; alpha must be fully opaque")

    def test_stub_session_preserves_native_size_on_non_square_image(self) -> None:
        image = _tiny_rgb(37, 21)
        result = birefnet_cutout(image, session=_StubSession())
        self.assertEqual(result.size, (37, 21))

    def test_injected_session_still_enforces_model_allowlist(self) -> None:
        with self.assertRaises(ValueError):
            birefnet_cutout(_tiny_rgb(), model=FORBIDDEN_MODEL, session=_StubSession())


class CliValidationTests(unittest.TestCase):
    def test_run_refuses_missing_source_file(self) -> None:
        missing = Path("this/path/does/not/exist.png")
        with self.assertRaises(FileNotFoundError) as ctx:
            run(missing, Path("tmp/unused_out.png"))
        self.assertIn(str(missing), str(ctx.exception))

    def test_run_refuses_forbidden_model_before_checking_source(self) -> None:
        # Model check runs before the source-exists check -- a bad model is
        # refused even for a source path that also doesn't exist.
        missing = Path("this/path/also/does/not/exist.png")
        with self.assertRaises(ValueError) as ctx:
            run(missing, Path("tmp/unused_out.png"), model=FORBIDDEN_MODEL)
        self.assertIn(FORBIDDEN_MODEL, str(ctx.exception))

    def test_main_exits_cleanly_on_missing_source(self) -> None:
        argv = ["birefnet_cutout.py", "--in", "no/such/file.png", "--out", "tmp/unused_out.png"]
        with patch.object(sys, "argv", argv):
            with self.assertRaises(SystemExit) as ctx:
                main()
        self.assertIn("source image missing", str(ctx.exception))

    def test_main_exits_cleanly_on_forbidden_model(self) -> None:
        argv = [
            "birefnet_cutout.py",
            "--in", "no/such/file.png",
            "--out", "tmp/unused_out.png",
            "--model", FORBIDDEN_MODEL,
        ]
        with patch.object(sys, "argv", argv):
            with self.assertRaises(SystemExit) as ctx:
                main()
        self.assertIn(FORBIDDEN_MODEL, str(ctx.exception))


class ReportShapeTests(unittest.TestCase):
    def test_report_schema_constant_matches_readme_contract(self) -> None:
        self.assertEqual(REPORT_SCHEMA, "ai_studio.image.birefnet_cutout_report.v1")


if __name__ == "__main__":
    unittest.main()
