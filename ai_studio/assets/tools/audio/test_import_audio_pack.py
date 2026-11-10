import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from import_audio_pack import import_pack, target_format


class TargetFormatTest(unittest.TestCase):
    def test_high_rate_surround_becomes_48k_stereo(self):
        self.assertEqual(target_format(6, 96000), (2, 48000))

    def test_lower_rate_and_mono_are_kept(self):
        self.assertEqual(target_format(1, 44100), (1, 44100))


@unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "needs ffmpeg")
class ImportPackTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.src = os.path.join(self.dir, "tone.wav")
        subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=1:sample_rate=96000",
                        "-ac", "6", "-c:a", "pcm_s24le", self.src], check=True)
        self.entry = {
            "dir": os.path.join(self.dir, "pack"),
            "pack": {"pack": "p", "title": "P", "kind": "audio", "license": "L", "commercial_use": "true",
                     "redistribution_allowed": "false", "tags": ["sfx"]},
            "items": [{"src": self.src, "resource": "files/lib/tone.wav", "asset_id": "p__tone", "title": "Tone",
                       "description": "A test tone.", "tags": ["test", "sfx"]}],
        }

    def tearDown(self):
        shutil.rmtree(self.dir)

    def test_converts_and_writes_manifest(self):
        self.assertEqual(import_pack(self.entry, 1), (1, []))
        with wave.open(os.path.join(self.dir, "pack", "files", "lib", "tone.wav"), "rb") as handle:
            self.assertEqual((handle.getnchannels(), handle.getframerate(), handle.getsampwidth()), (2, 48000, 2))
        with open(os.path.join(self.dir, "pack", "assets.jsonl"), encoding="utf-8") as handle:
            row = json.loads(handle.readline())
        self.assertEqual(row["tags"], ["sfx", "test", "short"])
        self.assertEqual(row["description"], "A test tone. 48kHz 16-bit stereo, 1.0 s.")
        self.assertEqual((row["license"], row["redistribution_allowed"]), ("L", "false"))
        with open(os.path.join(self.dir, "pack", "pack.json"), encoding="utf-8") as handle:
            self.assertEqual(json.load(handle)["pack"], "p")

    def test_rerun_keeps_converted_file(self):
        import_pack(self.entry, 1)
        dst = os.path.join(self.dir, "pack", "files", "lib", "tone.wav")
        before = os.path.getmtime(dst)
        import_pack(self.entry, 1)
        self.assertEqual(os.path.getmtime(dst), before)

    def test_unreadable_source_is_reported_not_written(self):
        self.entry["items"][0]["src"] = os.path.join(self.dir, "missing.wav")
        count, failed = import_pack(self.entry, 1)
        self.assertEqual((count, len(failed)), (0, 1))


if __name__ == "__main__":
    unittest.main()
