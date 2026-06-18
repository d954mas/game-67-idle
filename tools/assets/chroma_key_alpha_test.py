import unittest

from PIL import Image, ImageDraw

from tools.assets.chroma_key_alpha import (
    is_dark_magenta_edge_spill_like,
    is_dark_purple_halo_like,
    is_green_screen_spill_like,
    is_magenta_edge_spill_like,
    is_source_key_spill_like,
    key_to_alpha,
    resize_rgba_premultiplied,
)


class ChromaKeyAlphaTests(unittest.TestCase):
    def test_key_to_alpha_zeroes_fully_transparent_rgb_after_cleanup(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((6, 6, 17, 17), fill=(150, 90, 50, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((5, 12))

        self.assertEqual(alpha, 0)
        self.assertEqual((red, green, blue), (0, 0, 0))

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

    def test_key_to_alpha_removes_visible_green_screen_edge_spill(self) -> None:
        image = Image.new("RGBA", (24, 24), (255, 0, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((5, 5, 18, 18), fill=(142, 86, 38, 255))
        image.putpixel((18, 12), (19, 205, 9, 255))

        cleaned = key_to_alpha(image, aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((18, 12))

        self.assertEqual(alpha, 0)
        self.assertFalse(is_green_screen_spill_like(red, green, blue))

    def test_key_to_alpha_removes_muted_green_source_key_edge_cast(self) -> None:
        image = Image.new("RGBA", (24, 24), (0, 255, 0, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((5, 5, 18, 18), fill=(142, 86, 38, 255))
        image.putpixel((18, 12), (57, 88, 5, 255))

        cleaned = key_to_alpha(image, key=(0, 255, 0), aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((18, 12))

        self.assertTrue(is_source_key_spill_like(57, 88, 5, (0, 255, 0)))
        self.assertEqual(alpha, 0)
        self.assertFalse(is_source_key_spill_like(red, green, blue, (0, 255, 0)))

    def test_key_to_alpha_decontaminates_cyan_shadow_without_erasing_it(self) -> None:
        image = Image.new("RGBA", (320, 320), (0, 255, 255, 255))
        draw = ImageDraw.Draw(image)
        draw.rectangle((80, 80, 220, 220), fill=(44, 34, 58, 255))
        image.putpixel((221, 150), (20, 128, 132, 180))

        cleaned = key_to_alpha(image, key=(0, 255, 255), aggressive_visible_decontaminate=True)
        red, green, blue, alpha = cleaned.getpixel((221, 150))

        self.assertGreater(alpha, 12)
        self.assertFalse(is_source_key_spill_like(red, green, blue, (0, 255, 255)))

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
