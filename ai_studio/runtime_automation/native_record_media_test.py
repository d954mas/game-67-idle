import array
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

try:
    from . import native_record_media as media
except ImportError:
    import native_record_media as media

_audio_stats, _video_stats = media._audio_stats, media._video_stats


class DecodedMediaTests(unittest.TestCase):
    def test_video_counts_real_changes_and_freezes(self):
        stream = io.BytesIO(b"#tb 0: 1/60\n0,0,0,1,8,a\n0,1,1,1,8,a\n0,2,2,1,8,b\n")
        self.assertEqual(_video_stats(stream), {"decoded_frames": 3, "distinct_frames": 2, "longest_identical_run": 2})

    def test_video_rejects_timestamp_gaps_and_reversal(self):
        for pts in ("3", "0"):
            with self.subTest(pts=pts), self.assertRaises(RuntimeError):
                _video_stats(io.BytesIO(f"0,0,0,1,8,a\n0,1,1,1,8,b\n0,2,{pts},1,8,c\n".encode()))

    def test_audio_measures_stereo_and_silence(self):
        values = array.array("f", [.5, -.5, .5, -.5])
        sound = _audio_stats(io.BytesIO(values.tobytes()))
        self.assertEqual(sound["decoded_audio_frames"], 2)
        self.assertEqual(sound["peak"], .5)
        self.assertAlmostEqual(sound["rms_dbfs"], -6.0206, places=4)
        self.assertLess(_audio_stats(io.BytesIO(bytes(16)))["rms_dbfs"], -100)

    def test_audio_rejects_nonfinite_or_incomplete_stereo(self):
        for values in ([float("nan"), 0], [float("inf"), 0], [.5]):
            with self.subTest(values=values), self.assertRaises(RuntimeError):
                _audio_stats(io.BytesIO(array.array("f", values).tobytes()))


class MediaContractTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "take.mp4"
        self.path.write_bytes(b"fixture")
        self.info = {"streams": [
            {"codec_type": "video", "codec_name": "h264", "width": 640, "height": 360, "avg_frame_rate": "60/1", "start_time": "0"},
            {"codec_type": "audio", "codec_name": "aac", "sample_rate": "48000", "channels": 2, "start_time": "0"},
        ]}
        self.frames = {"decoded_frames": 60, "distinct_frames": 60, "longest_identical_run": 1}
        self.sound = {"decoded_audio_frames": 48128, "peak": .25, "rms_dbfs": -20}

    def validate(self, **kwargs):
        with patch.object(media.subprocess, "run", return_value=SimpleNamespace(stdout=json.dumps(self.info))), \
             patch.object(media, "_decode", side_effect=[self.frames, self.sound]):
            return media.validate_native_media(self.path, width=640, height=360, fps=60,
                                               receipt={"frames": 60, "audio_frames": 48000}, **kwargs)

    def test_accepts_aac_padding_with_real_decoded_frames(self):
        self.assertEqual(self.validate(expect_audio=True, expect_motion=True)["decoded_audio_frames"], 48128)

    def test_silent_static_take_requires_explicit_activity_expectation(self):
        self.frames["distinct_frames"] = 1
        self.sound.update(peak=0, rms_dbfs=-240)
        self.validate()
        for expectation in ({"expect_audio": True}, {"expect_motion": True}):
            with self.assertRaises(RuntimeError):
                self.validate(**expectation)

    def test_rejects_truncated_decoded_video_and_audio(self):
        self.frames["decoded_frames"] = 59
        with self.assertRaises(RuntimeError):
            self.validate()
        self.frames["decoded_frames"] = 60
        self.sound["decoded_audio_frames"] = 24000
        with self.assertRaises(RuntimeError):
            self.validate()

    def test_rejects_wrong_size_rate_codec_or_channels(self):
        for index, key, value in [(0, "width", 320), (0, "avg_frame_rate", "30/1"),
                                  (0, "codec_name", "vp9"), (1, "channels", 1),
                                  (1, "sample_rate", "44100")]:
            with self.subTest(key=key):
                original = self.info["streams"][index][key]
                self.info["streams"][index][key] = value
                with self.assertRaises(RuntimeError):
                    self.validate()
                self.info["streams"][index][key] = original

    def test_rejects_missing_audio_stream(self):
        self.info["streams"].pop()
        with self.assertRaises(RuntimeError):
            self.validate()

    def test_rejects_delayed_audio_even_when_lengths_and_frame_counts_match(self):
        self.info["streams"][1]["start_time"] = ".25"
        with self.assertRaisesRegex(RuntimeError, "timestamps are misaligned"):
            self.validate()


if __name__ == "__main__":
    unittest.main()
