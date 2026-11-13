import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import export_mp3 as mp3

RATE = mp3.REFERENCE_RATE


def band_noise(low, high, seconds, seed):
    noise = np.random.default_rng(seed).standard_normal(int(RATE * seconds))
    spectrum = np.fft.rfft(noise)
    freq = np.fft.rfftfreq(len(noise), 1.0 / RATE)
    spectrum[(freq < low) | (freq >= high)] = 0
    return np.fft.irfft(spectrum, len(noise)).astype(np.float32)


def decay(pcm, seconds):
    return pcm * np.exp(-np.arange(len(pcm)) / (RATE * seconds)).astype(np.float32)


def hiss_burst():
    """Most of the energy above 6.5 kHz, like a coin pour: the 24 kHz lowpass takes its top."""
    return decay(band_noise(6500, 9000, 0.6, 1) + 0.15 * band_noise(500, 6500, 0.6, 2), 0.2)


def thump():
    """A low hit with a short broadband attack."""
    t = np.arange(int(RATE * 0.4)) / RATE
    body = np.sin(2 * np.pi * 70 * t) + 0.5 * np.sin(2 * np.pi * 140 * t)
    return decay(body.astype(np.float32) + 0.2 * decay(band_noise(200, 3000, 0.4, 3), 0.01), 0.08)


@unittest.skipUnless(shutil.which("ffmpeg"), "needs ffmpeg")
class RateRuleTest(unittest.TestCase):
    def test_high_frequency_burst_is_rejected_at_24k(self):
        shaped, _ = mp3.shape_effect(hiss_burst(), RATE)
        result = mp3.choose_rate(shaped, mp3.EFFECT["quality"], candidates=(24000,))
        self.assertEqual(result.rate, RATE)
        self.assertFalse(result.trials[0]["accepted"])
        self.assertGreaterEqual(result.trials[0]["balance_shift_db"], mp3.MAX_BALANCE_SHIFT_DB)

    def test_low_thump_is_accepted_at_24k(self):
        shaped, _ = mp3.shape_effect(thump(), RATE)
        result = mp3.choose_rate(shaped, mp3.EFFECT["quality"])
        self.assertEqual(result.rate, 24000)
        self.assertEqual([trial["rate"] for trial in result.trials], [24000])
        self.assertLess(result.trials[0]["balance_shift_db"], mp3.MAX_BALANCE_SHIFT_DB)

    def test_shape_then_resample_keeps_the_level(self):
        shaped, _ = mp3.shape_effect(thump(), RATE)
        reference = mp3.decode_mp3(mp3.encode(shaped, RATE, 9))
        lower = mp3.decode_mp3(mp3.export_at(shaped, 24000, 9).data)
        self.assertLess(abs(mp3.level_change_db(reference, lower)), 0.3)

    def test_balance_shift_is_zero_for_the_same_signal(self):
        pcm = hiss_burst()
        self.assertAlmostEqual(mp3.balance_shift_db(pcm, pcm * 0.5), 0.0, places=6)


@unittest.skipUnless(shutil.which("ffmpeg"), "needs ffmpeg")
class GaplessTest(unittest.TestCase):
    def test_export_has_the_header_and_decodes_to_its_length(self):
        shaped, _ = mp3.shape_effect(thump(), RATE)
        for rate in (RATE, 24000):
            result = mp3.export_at(shaped, rate, 9)
            self.assertTrue(mp3.gapless_header(result.data))
            self.assertEqual(len(mp3.decode_mp3(result.data, rate)), result.samples)

    def test_padding_is_exact_on_both_sides_of_the_muxer_window(self):
        # libavformat records the padding one frame short when len mod frame size is 1..46.
        noise = np.random.default_rng(4)
        for rate, frame in ((24000, 576), (RATE, 1152)):
            for remainder in (0, 1, 46, 47):
                with self.subTest(rate=rate, remainder=remainder):
                    samples = frame * 16 + remainder
                    data = mp3.encode((0.3 * noise.standard_normal(samples)).astype(np.float32), rate, 9)
                    self.assertEqual(len(mp3.decode_mp3(data, rate)), samples)
                    tag = mp3._info_tag(data)
                    stored = int.from_bytes(data[tag["lame"] + 34:tag["lame"] + 36], "big")
                    self.assertEqual(stored, mp3._crc16(data[tag["frame"]:tag["lame"] + 34]))

    def test_plain_mp3_has_no_header(self):
        with tempfile.TemporaryDirectory() as scratch:
            out = Path(scratch) / "plain.mp3"
            subprocess.run(["ffmpeg", "-v", "error", "-f", "f32le", "-ar", str(RATE), "-ac", "1", "-i", "-",
                            "-c:a", "libmp3lame", "-write_xing", "0", str(out)], input=thump().tobytes(), check=True)
            self.assertFalse(mp3.gapless_header(out.read_bytes()))

    def test_loop_resample_keeps_the_seam(self):
        t = np.arange(RATE) / RATE
        loop = np.sin(2 * np.pi * 441 * t).astype(np.float32)
        out = mp3.resample(loop, RATE, 24000, loop=True)
        self.assertEqual(len(out), 24000)
        steps = np.abs(np.diff(np.concatenate([out, out[:1]])))
        self.assertLess(steps[-1], steps[:-1].max() * 1.01)


if __name__ == "__main__":
    unittest.main()
