import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from capture_contracts import CaptureContractError
from capture_media_fixture import (
    FixtureSpec,
    generate_synthetic_fixture,
    validate_synthetic_fixture,
)


FFMPEG_AVAILABLE = bool(shutil.which("ffmpeg") and shutil.which("ffprobe"))


@unittest.skipUnless(FFMPEG_AVAILABLE, "FFmpeg and ffprobe are required")
class SyntheticMediaFixtureTest(unittest.TestCase):
    def test_fixture_has_expected_streams_timing_and_decoded_sync_markers(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "synthetic-av.mkv"
            spec = FixtureSpec(
                width=160,
                height=90,
                fps=30,
                duration_seconds=2,
                marker_times=(0.25, 1.75),
            )

            manifest = generate_synthetic_fixture(output, spec)
            report = validate_synthetic_fixture(output, spec)

            self.assertEqual(manifest["schema"], "ai_studio.capture.av_fixture.v1")
            self.assertEqual(manifest["sha256"], report["sha256"])
            self.assertEqual(report["structural"]["status"], "pass")
            self.assertEqual(report["timestamp"]["status"], "pass")
            self.assertEqual(report["content_sync"]["status"], "pass")
            self.assertEqual(report["video"]["width"], 160)
            self.assertEqual(report["video"]["height"], 90)
            self.assertEqual(report["video"]["codec"], "h264")
            self.assertEqual(report["audio"]["sample_rate"], 48000)
            self.assertEqual(report["audio"]["codec"], "pcm_s16le")
            self.assertTrue(report["timestamp"]["video"]["monotonic"])
            self.assertTrue(report["timestamp"]["audio"]["monotonic"])
            self.assertLessEqual(
                report["timestamp"]["video"]["max_gap_seconds"], 2 / spec.fps
            )
            self.assertLessEqual(
                report["timestamp"]["audio"]["max_gap_seconds"], 2 / spec.fps
            )
            self.assertEqual(len(report["content_sync"]["markers"]), 2)
            for marker in report["content_sync"]["markers"]:
                self.assertLessEqual(marker["absolute_offset_seconds"], 1 / spec.fps)

    def test_wrong_expected_dimensions_fail_with_stable_code(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "wrong-size.mkv"
            actual = FixtureSpec(width=160, height=90, fps=30, duration_seconds=1)
            generate_synthetic_fixture(output, actual)
            expected = FixtureSpec(width=320, height=180, fps=30, duration_seconds=1)

            with self.assertRaises(CaptureContractError) as raised:
                validate_synthetic_fixture(output, expected)

            self.assertEqual(raised.exception.code, "AV_VALIDATION_FAILED")
            self.assertIn("remediation", raised.exception.safe_details)

    def test_truncated_fixture_never_validates(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "truncated.mkv"
            spec = FixtureSpec(width=160, height=90, fps=30, duration_seconds=1)
            generate_synthetic_fixture(output, spec)
            data = output.read_bytes()
            output.write_bytes(data[: max(1, len(data) // 3)])

            with self.assertRaises(CaptureContractError) as raised:
                validate_synthetic_fixture(output, spec)

            self.assertEqual(raised.exception.code, "AV_VALIDATION_FAILED")

    def test_missing_markers_fail_at_content_sync_layer(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "no-markers.mkv"
            actual = FixtureSpec(
                width=160,
                height=90,
                fps=30,
                duration_seconds=2,
                marker_times=(),
            )
            expected = FixtureSpec(
                width=160,
                height=90,
                fps=30,
                duration_seconds=2,
                marker_times=(0.25, 1.75),
            )
            generate_synthetic_fixture(output, actual)

            with self.assertRaises(CaptureContractError) as raised:
                validate_synthetic_fixture(output, expected)

            self.assertEqual(raised.exception.safe_details["stage"], "content-sync")

    def test_unexpected_duplicate_marker_fails_at_content_sync_layer(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "extra-marker.mkv"
            actual = FixtureSpec(
                width=160,
                height=90,
                fps=30,
                duration_seconds=2,
                marker_times=(0.25, 1.0, 1.75),
            )
            expected = FixtureSpec(
                width=160,
                height=90,
                fps=30,
                duration_seconds=2,
                marker_times=(0.25, 1.75),
            )
            generate_synthetic_fixture(output, actual)

            with self.assertRaises(CaptureContractError) as raised:
                validate_synthetic_fixture(output, expected)

            self.assertEqual(raised.exception.safe_details["stage"], "content-sync")

    def test_shifted_audio_marker_fails_at_content_sync_layer(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.mkv"
            shifted = Path(directory) / "shifted.mkv"
            spec = FixtureSpec(
                width=160,
                height=90,
                fps=30,
                duration_seconds=2,
                marker_times=(0.25, 1.25),
            )
            generate_synthetic_fixture(source, spec)
            subprocess.run(
                [
                    shutil.which("ffmpeg"),
                    "-hide_banner",
                    "-loglevel",
                    "error",
                    "-y",
                    "-i",
                    str(source),
                    "-filter_complex",
                    "[0:a]adelay=150:all=1,atrim=duration=2[a]",
                    "-map",
                    "0:v:0",
                    "-map",
                    "[a]",
                    "-c:v",
                    "copy",
                    "-c:a",
                    "pcm_s16le",
                    str(shifted),
                ],
                check=True,
            )

            with self.assertRaises(CaptureContractError) as raised:
                validate_synthetic_fixture(shifted, spec)

            self.assertEqual(raised.exception.safe_details["stage"], "content-sync")


if __name__ == "__main__":
    unittest.main()
