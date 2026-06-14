import unittest

from PIL import Image, ImageDraw

from tools.assets.chroma_key_alpha import (
    is_dark_magenta_edge_spill_like,
    is_dark_purple_halo_like,
    is_magenta_edge_spill_like,
    key_to_alpha,
    resize_rgba_premultiplied,
)


class ChromaKeyAlphaTests(unittest.TestCase):
    def test_key_to_alpha_bleeds_transparent_edge_rgb(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((6, 6, 17, 17), fill=(150, 90, 50, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((5, 12))

        self.assertEqual(alpha, 0)
        self.assertNotEqual((red, green, blue), (255, 0, 255))
        self.assertLess(abs(red - 150), 40)
        self.assertLess(abs(green - 90), 40)
        self.assertLess(abs(blue - 50), 40)

    def test_aggressive_decontamination_repairs_visible_purple_specks(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((5, 5, 18, 18), fill=(160, 100, 55, 255))
        image.putpixel((12, 6), (130, 8, 96, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((12, 6))

        self.assertEqual(alpha, 255)
        self.assertFalse(red > 75 and blue > 75 and green < 120 and min(red, blue) - green > 20)

    def test_aggressive_decontamination_repairs_dark_one_pixel_purple_edge(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((5, 5, 18, 18), fill=(140, 86, 38, 255))
        image.putpixel((18, 12), (64, 0, 64, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((18, 12))

        self.assertEqual(alpha, 255)
        self.assertFalse(is_dark_purple_halo_like(red, green, blue))

    def test_aggressive_decontamination_repairs_near_black_purple_edge(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((5, 5, 18, 18), fill=(140, 86, 38, 255))
        image.putpixel((18, 12), (38, 2, 45, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((18, 12))

        self.assertEqual(alpha, 255)
        self.assertFalse(is_dark_purple_halo_like(red, green, blue))

    def test_aggressive_decontamination_repairs_maroon_magenta_edge_spill(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((5, 5, 18, 18), fill=(142, 86, 38, 255))
        image.putpixel((12, 5), (128, 48, 72, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((12, 5))

        self.assertEqual(alpha, 255)
        self.assertFalse(is_magenta_edge_spill_like(red, green, blue))

    def test_aggressive_decontamination_repairs_dark_magenta_edge_spill(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((5, 5, 18, 18), fill=(142, 86, 38, 255))
        image.putpixel((18, 12), (55, 20, 45, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((18, 12))

        self.assertEqual(alpha, 255)
        self.assertFalse(is_dark_magenta_edge_spill_like(red, green, blue))

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


if __name__ == "__main__":
    unittest.main()
