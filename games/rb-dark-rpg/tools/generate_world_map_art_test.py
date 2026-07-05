#!/usr/bin/env python3

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from math import hypot

from PIL import Image

import generate_world_map_art as map_art


def point_segment_distance(point, a, b) -> float:
    px, py = point
    ax, ay = a
    bx, by = b
    vx = bx - ax
    vy = by - ay
    wx = px - ax
    wy = py - ay
    c2 = vx * vx + vy * vy
    t = 0.0 if c2 == 0.0 else max(0.0, min(1.0, (wx * vx + wy * vy) / c2))
    qx = ax + t * vx
    qy = ay + t * vy
    return hypot(px - qx, py - qy)


def point_polyline_distance(point, path) -> float:
    return min(point_segment_distance(point, path[i], path[i + 1]) for i in range(len(path) - 1))


def point_polygon_boundary_distance(point, poly) -> float:
    return min(point_segment_distance(point, poly[i], poly[(i + 1) % len(poly)]) for i in range(len(poly)))


class WorldMapArtGenerationTest(unittest.TestCase):
    def test_runtime_locations_map_to_expected_atlas_bounds(self) -> None:
        post = map_art.location_to_pixel(*map_art.LOCATION_POINTS["hub_last_post"])
        gate = map_art.location_to_pixel(*map_art.LOCATION_POINTS["hub_gate_outskirts"])
        mill = map_art.location_to_pixel(*map_art.LOCATION_POINTS["old_mill"])

        self.assertLess(post[0], gate[0])
        self.assertLess(gate[0], mill[0])
        self.assertGreater(post[1], mill[1])
        for point in [post, gate, mill]:
            self.assertGreater(point[0], 24)
            self.assertLess(point[0], map_art.MAP_WIDTH - 24)
            self.assertGreater(point[1], 24)
            self.assertLess(point[1], map_art.MAP_HEIGHT - 24)

    def test_generated_png_has_expected_dimensions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "ash_border_map.png"
            map_art.build_map_art().save(out)

            self.assertGreater(out.stat().st_size, 100_000)
            with Image.open(out) as img:
                self.assertEqual(img.size, (map_art.MAP_WIDTH, map_art.MAP_HEIGHT))
                self.assertEqual(img.mode, "RGBA")

    def test_region_gates_are_explicit_border_markers(self) -> None:
        self.assertGreaterEqual(len(map_art.REGION_POLYGONS), 3)
        self.assertEqual(
            [gate["id"] for gate in map_art.REGION_GATES],
            ["last_post_to_outskirts", "outskirts_to_mill"],
        )
        self.assertIn(map_art.CURRENT_REGION_ID, map_art.REGION_POLYGONS)
        self.assertGreater(map_art.GATE_OPEN_WIDTH, map_art.ROAD_CORE_WIDTH * 3)
        self.assertGreater(map_art.GATE_OPEN_HEIGHT, map_art.ROAD_HIGHLIGHT_WIDTH * 4)
        self.assertLess(map_art.ROAD_HIGHLIGHT_WIDTH, map_art.GATE_OPEN_HEIGHT / 4)

        open_count = 0
        for gate in map_art.REGION_GATES:
            x, y = gate["center"]
            self.assertGreater(x, 0)
            self.assertLess(x, map_art.MAP_WIDTH)
            self.assertGreater(y, 0)
            self.assertLess(y, map_art.MAP_HEIGHT)
            self.assertIsInstance(gate["open"], bool)
            open_count += 1 if gate["open"] else 0
        self.assertGreaterEqual(open_count, 2)

    def test_region_gates_sit_on_borders_and_roads(self) -> None:
        road_paths = map_art.road_paths()
        for gate in map_art.REGION_GATES:
            point = gate["center"]
            self.assertLess(
                min(point_polygon_boundary_distance(point, poly) for poly in map_art.REGION_POLYGONS.values()),
                16.0,
                gate["id"],
            )
            self.assertLess(
                min(point_polyline_distance(point, path) for path in road_paths.values()),
                22.0,
                gate["id"],
            )

    def test_old_mill_marker_sits_inside_old_mill_region(self) -> None:
        mill = map_art.location_to_pixel(*map_art.LOCATION_POINTS["old_mill"])

        self.assertTrue(map_art.point_in_poly(*mill, map_art.REGION_POLYGONS["old_mill"]))
        self.assertFalse(map_art.point_in_poly(*mill, map_art.REGION_POLYGONS["hub_gate_outskirts"]))

    def test_landmarks_do_not_escape_their_regions(self) -> None:
        for point in map_art.LAST_POST_LANDMARK_POLY:
            self.assertTrue(map_art.point_in_poly(*point, map_art.REGION_POLYGONS["hub_last_post"]))
        for point in map_art.OLD_MILL_YARD_POLY:
            self.assertTrue(map_art.point_in_poly(*point, map_art.REGION_POLYGONS["old_mill"]))


if __name__ == "__main__":
    unittest.main()
