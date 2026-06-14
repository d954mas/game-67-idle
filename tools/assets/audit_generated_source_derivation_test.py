import json
import tempfile
import unittest
from pathlib import Path
from subprocess import run

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/assets/audit_generated_source_derivation.py"


def write_manifest(
    tmp: Path,
    source: Path,
    output: Path,
    *,
    allow_procedural: bool = False,
    key_color: str | None = None,
) -> Path:
    crop = {
        "id": "panel",
        "kind": "slice9",
        "rect": [0, 0, 96, 64],
        "output": str(output),
        "slice9": {"left": 16, "top": 16, "right": 16, "bottom": 16},
    }
    if allow_procedural:
        crop["allow_procedural_redraw"] = True
    manifest = {
        "schema": "game.art_crop_manifest",
        "version": 1,
        "sources": [
            {
                "id": "test_source",
                "path": str(source),
                "crops": [crop],
            }
        ],
    }
    if key_color:
        manifest["green_screen"] = {"mode": "chroma_key", "key": key_color}
    path = tmp / "manifest.json"
    path.write_text(json.dumps(manifest), encoding="utf-8")
    return path


def make_source(path: Path) -> None:
    image = Image.new("RGBA", (96, 64), (255, 0, 255, 255))
    draw = ImageDraw.Draw(image)
    for y in range(10, 54):
        red = 70 + y
        green = 40 + y // 3
        blue = 24
        draw.line((12, y, 83, y), fill=(red, green, blue, 255))
    draw.rectangle((12, 10, 83, 53), outline=(210, 170, 80, 255), width=3)
    image.save(path)


def make_derived_output(source: Path, output: Path) -> None:
    image = Image.open(source).convert("RGBA")
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels[x, y]
            if red == 255 and green == 0 and blue == 255:
                pixels[x, y] = (red, green, blue, 0)
            else:
                pixels[x, y] = (red, green, blue, alpha)
    image.save(output)


def make_green_source(path: Path) -> None:
    image = Image.new("RGBA", (96, 64), (0, 255, 0, 255))
    draw = ImageDraw.Draw(image)
    draw.rectangle((12, 10, 83, 53), fill=(80, 46, 24, 255), outline=(210, 170, 80, 255), width=3)
    image.save(path)


def make_green_derived_output(source: Path, output: Path) -> None:
    image = Image.open(source).convert("RGBA")
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels[x, y]
            if red == 0 and green == 255 and blue == 0:
                pixels[x, y] = (red, green, blue, 0)
            else:
                pixels[x, y] = (red, green, blue, alpha)
    image.save(output)


class GeneratedSourceDerivationAuditTests(unittest.TestCase):
    def test_passes_directly_derived_source_crop(self):
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            source = tmp / "source.png"
            output = tmp / "output.png"
            make_source(source)
            make_derived_output(source, output)
            manifest = write_manifest(tmp, source, output)

            result = run(
                ["python", str(SCRIPT), "--crop-manifest", str(manifest)],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("pass: checked 1 generated-source asset", result.stdout)

    def test_rejects_same_size_procedural_redraw(self):
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            source = tmp / "source.png"
            output = tmp / "output.png"
            make_source(source)
            image = Image.new("RGBA", (96, 64), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rounded_rectangle((8, 8, 87, 55), radius=10, fill=(30, 80, 160, 255), outline=(240, 240, 240, 255), width=4)
            image.save(output)
            manifest = write_manifest(tmp, source, output)

            result = run(
                ["python", str(SCRIPT), "--crop-manifest", str(manifest)],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("too many changed visible pixels", result.stdout)

    def test_passes_directly_derived_green_key_source_crop(self):
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            source = tmp / "source.png"
            output = tmp / "output.png"
            make_green_source(source)
            make_green_derived_output(source, output)
            manifest = write_manifest(tmp, source, output, key_color="#00ff00")

            result = run(
                ["python", str(SCRIPT), "--crop-manifest", str(manifest)],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("pass: checked 1 generated-source asset", result.stdout)

    def test_skips_explicit_procedural_exception(self):
        with tempfile.TemporaryDirectory() as tmp_name:
            tmp = Path(tmp_name)
            source = tmp / "source.png"
            output = tmp / "output.png"
            make_source(source)
            Image.new("RGBA", (96, 64), (40, 40, 40, 255)).save(output)
            manifest = write_manifest(tmp, source, output, allow_procedural=True)

            result = run(
                ["python", str(SCRIPT), "--crop-manifest", str(manifest)],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("pass: checked 0 generated-source asset(s), skipped 1", result.stdout)


if __name__ == "__main__":
    unittest.main()
