import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image

from tools.assets.atomic_io import save_image_atomic, write_json_atomic, write_text_atomic


class AtomicIoTest(unittest.TestCase):
    def test_write_text_atomic_keeps_existing_file_on_failure(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "report.md"
            target.write_text("old complete report\n", encoding="utf-8")
            original_write_text = Path.write_text

            def failing_write_text(path, text, *args, **kwargs):
                if Path(path).name.startswith(".report.md."):
                    Path(path).write_bytes(b"partial report")
                    raise RuntimeError("simulated interrupted report write")
                return original_write_text(path, text, *args, **kwargs)

            with patch.object(Path, "write_text", failing_write_text):
                with self.assertRaises(RuntimeError):
                    write_text_atomic(target, "new report\n")

            self.assertEqual(target.read_text(encoding="utf-8"), "old complete report\n")
            self.assertEqual(list(root.glob(".report.md.*.tmp")), [])

    def test_write_json_atomic_adds_trailing_newline(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "report.json"
            write_json_atomic(target, {"verdict": "pass"})
            self.assertTrue(target.read_text(encoding="utf-8").endswith("\n"))

    def test_save_image_atomic_keeps_existing_png_on_save_failure(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "proof.png"
            Image.new("RGBA", (4, 4), (10, 20, 30, 255)).save(target)

            def failing_save(_image, path, *args, **kwargs):
                Path(path).write_bytes(b"partial-png")
                raise RuntimeError("simulated interrupted image save")

            with patch.object(Image.Image, "save", failing_save):
                with self.assertRaises(RuntimeError):
                    save_image_atomic(Image.new("RGBA", (4, 4), (200, 0, 0, 255)), target)

            restored = Image.open(target).convert("RGBA")
            self.assertEqual(restored.getpixel((0, 0)), (10, 20, 30, 255))
            self.assertEqual(list(root.glob(".proof.png.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
