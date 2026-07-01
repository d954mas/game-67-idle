import unittest

from PIL import Image, ImageDraw

from ai_studio.assets.tools.cutout.chroma_key_alpha import (
    is_green_screen_spill_like,
    resize_rgba_premultiplied,
)


class ChromaKeyAlphaTests(unittest.TestCase):
    def test_premultiplied_resize_does_not_sample_hidden_green(self) -> None:
        image = Image.new("RGBA", (12, 12), (0, 255, 0, 0))
        draw = ImageDraw.Draw(image)
        draw.rectangle((3, 3, 8, 8), fill=(180, 120, 70, 255))

        resized = resize_rgba_premultiplied(image, (6, 6))
        pixels = resized.load()
        for y in range(resized.height):
            for x in range(resized.width):
                red, green, blue, alpha = pixels[x, y]
                if alpha > 12:
                    self.assertFalse(is_green_screen_spill_like(red, green, blue))
                if alpha == 0:
                    self.assertEqual((red, green, blue), (0, 0, 0))

    def test_premultiplied_resize_does_not_sample_hidden_magenta(self) -> None:
        image = Image.new("RGBA", (12, 12), (255, 0, 255, 0))
        draw = ImageDraw.Draw(image)
        draw.rectangle((3, 3, 8, 8), fill=(180, 120, 70, 255))

        resized = resize_rgba_premultiplied(image, (6, 6))
        pixels = resized.load()
        for y in range(resized.height):
            for x in range(resized.width):
                red, green, blue, alpha = pixels[x, y]
                if alpha > 12:
                    self.assertFalse(red > 115 and blue > 120 and green < 145)
                if alpha == 0:
                    self.assertEqual((red, green, blue), (0, 0, 0))


if __name__ == "__main__":
    unittest.main()
