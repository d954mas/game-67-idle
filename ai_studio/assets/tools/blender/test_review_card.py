import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from ai_studio.assets.tools.blender import review_card
from ai_studio.assets.tools.blender.lib import report as R


class Card(unittest.TestCase):
    def test_card_holds_every_view_and_finding(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            images = []
            for view in ("front", "three_quarter"):
                for render_pass in ("beauty", "wire"):
                    name = f"{view}_{render_pass}.png"
                    Image.new("RGBA", (64, 64), (200, 80, 40, 255)).save(tmp / name)
                    images.append({"view": view, "pass": render_pass, "file": name, "size": [64, 64]})
            for extra in ({"view": "lineup", "base_view": "front", "pass": "beauty", "members": ["crate", "old"]},
                          {"view": "pose", "base_view": "three_quarter", "pass": "beauty", "animation": "Walk",
                           "time": 0.4}):
                name = f"{extra['view']}.png"
                Image.new("RGBA", (128, 64), (40, 200, 80, 255)).save(tmp / name)
                images.append({**extra, "file": name, "size": [128, 64]})
            views = {"asset": "crate.glb", "sha256": "ab" * 32, "blender": "5.2.3",
                     "stats": {"triangles": 12, "vertices": 8, "materials": ["wood"], "textures": [],
                               "bounds": {"min": [0, 0, 0], "max": [1, 1, 1]}},
                     "images": images}
            (tmp / "views.json").write_text(json.dumps(views), encoding="utf-8")
            report = R.make_report("mesh_check", "crate.glb", [
                R.finding("boundary_edges", "high", "4 open edges", "fill the hole")])
            card = review_card.compose(tmp / "views.json", [report], tmp / "card.png", cell=96)
            with Image.open(card) as image:
                self.assertGreater(image.width, 2 * 96)
                # header + lineup row + two grid rows + pose row
                self.assertGreater(image.height, 112 + 3 * 96 + 48)
                # lineup and pose images never become grid columns of their own
                self.assertLess(image.width, 3 * 96 + 600)


class PoseRows(unittest.TestCase):
    def test_every_pose_frame_is_shown_even_with_one_view_column(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            Image.new("RGBA", (64, 64), (200, 80, 40, 255)).save(tmp / "front.png")
            images = [{"view": "front", "pass": "beauty", "file": "front.png", "size": [64, 64]}]
            heights = []
            for count in (6, 8):
                for k in range(count):
                    Image.new("RGBA", (64, 64), (40, 80, 200, 255)).save(tmp / f"pose{k}.png")
                poses = [{"view": "pose", "base_view": "front", "pass": "beauty", "animation": "Walk",
                          "time": k / 10, "file": f"pose{k}.png", "size": [64, 64]} for k in range(count)]
                views = {"asset": "a.glb", "stats": {}, "images": images + poses}
                (tmp / "views.json").write_text(json.dumps(views), encoding="utf-8")
                card = review_card.compose(tmp / "views.json", [], tmp / f"card{count}.png", cell=96)
                with Image.open(card) as image:
                    heights.append(image.height)
            # one view column fits one 48 px frame per row: two more poses add two rows
            self.assertGreaterEqual(heights[1] - heights[0], 2 * 48)


if __name__ == "__main__":
    unittest.main()
