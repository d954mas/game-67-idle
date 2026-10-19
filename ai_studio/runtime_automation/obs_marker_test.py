from __future__ import annotations

import unittest
from pathlib import Path

from ai_studio.runtime_automation.obs_marker import (
    MARKER_COLOR_ARGB,
    build_marker_probe_command,
    parse_marker_receipt,
)


class ObsMarkerTest(unittest.TestCase):
    def test_builds_a_marker_probe_for_decoded_frames(self) -> None:
        command = build_marker_probe_command(Path("ffmpeg.exe"), Path("take.mkv"))

        self.assertEqual(command[:7], ["ffmpeg.exe", "-hide_banner", "-loglevel", "error", "-i", "take.mkv", "-an"])
        self.assertIn("colorkey=0xff00ff:0.1:0", command[8])
        self.assertIn("blackdetect=d=0:pix_th=0.1:pic_th=0.99", command[8])
        self.assertEqual(command[-3:], ["-f", "null", "-"])
        self.assertEqual(MARKER_COLOR_ARGB, 0xFFFF00FF)

    def test_receipt_uses_decoded_frame_pts_and_indexes(self) -> None:
        receipt = parse_marker_receipt(
            "frame:12 pts:19584 pts_time:1.275\n"
            "lavfi.black_start=1.275\n"
            "frame:17 pts:29376 pts_time:1.9125\n"
            "lavfi.black_end=1.9125\n"
        )

        self.assertEqual(receipt, {
            "startSeconds": 1.275,
            "endSeconds": 1.9125,
            "startFrame": 12,
            "endFrame": 17,
            "markerFrames": 5,
        })

    def test_receipt_rejects_absent_incomplete_or_ambiguous_marker_runs(self) -> None:
        cases = [
            "frame:1 pts:100 pts_time:0.1\n",
            "frame:1 pts:100 pts_time:0.1\nlavfi.black_start=0.1\n",
            (
                "frame:1 pts:100 pts_time:0.1\nlavfi.black_start=0.1\n"
                "frame:2 pts:200 pts_time:0.2\nlavfi.black_end=0.2\n"
                "frame:3 pts:300 pts_time:0.3\nlavfi.black_start=0.3\n"
                "frame:4 pts:400 pts_time:0.4\nlavfi.black_end=0.4\n"
            ),
        ]

        for stdout in cases:
            with self.subTest(stdout=stdout), self.assertRaisesRegex(RuntimeError, "one complete contiguous"):
                parse_marker_receipt(stdout)

    def test_receipt_parses_actual_ffmpeg_fixture(self) -> None:
        receipt = parse_marker_receipt(
            "frame:2    pts:200     pts_time:0.2\n"
            "lavfi.black_start=0.2\n"
            "frame:6    pts:600     pts_time:0.6\n"
            "lavfi.black_end=0.6\n"
        )
        self.assertEqual(receipt["startFrame"], 2)
        self.assertEqual(receipt["endFrame"], 6)
        self.assertEqual(receipt["markerFrames"], 4)
        self.assertAlmostEqual(receipt["startSeconds"], 0.2, places=6)
        self.assertAlmostEqual(receipt["endSeconds"], 0.6, places=6)


if __name__ == "__main__":
    unittest.main()
