"""Export game sounds and music as mono MP3 with a measured sample-rate rule.

A sound is shaped once at REFERENCE_RATE (effects: trim, fade-out, peak gain;
music: integrated loudness), then the shaped PCM is resampled with soxr to each
candidate rate and encoded with libmp3lame. `choose_rate` keeps the lowest
candidate whose decoded result stays within MAX_LEVEL_CHANGE_DB in level and
MAX_BALANCE_SHIFT_DB in spectral balance of the reference-rate encode, and
falls back to the reference rate. The rule and its calibration are in
README.md#how-to-compress-game-audio.

    python ai_studio/assets/tools/audio/export_mp3.py <source> <out.mp3> [--music] [--rate HZ]

Prints the chosen rate and the measured numbers as JSON. Needs ffmpeg built
with libmp3lame and soxr on PATH.
"""

import argparse
import json
import math
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

REFERENCE_RATE = 44100
CANDIDATE_RATES = (24000, 32000)

# One audible calibration point backs MAX_BALANCE_SHIFT_DB: a coin pour at
# 24 kHz measured 1.3 dB and was heard as hiss; 24 inaudible verdicts measured
# 0.92 dB or less. Move it only with new listening evidence.
MAX_BALANCE_SHIFT_DB = 1.0
MAX_LEVEL_CHANGE_DB = 1.0

EFFECT = {"quality": 9, "trim_db": -50.0, "fade_ms": 8.0, "peak_db": -1.0}
MUSIC = {"quality": 7, "lufs": -18.0, "peak_db": -1.0}

SPECTRUM_FRAME = 2048
SPECTRUM_HOP = 512
BALANCE_BANDS_HZ = tuple((low, low + 1000) for low in range(1000, 7000, 1000))
BALANCE_TOTAL_HZ = (100, 7500)
# A band this far under the whole is inaudible next to it, and its share
# swings by tens of dB on codec noise alone.
BALANCE_FLOOR_DB = -60.0
# Circular context around a loop before resampling, so the filter sees the
# loop's own other end at the seam instead of silence.
LOOP_PAD_SECONDS = 0.1


def _ffmpeg(args, data=None):
    return subprocess.run(["ffmpeg", "-v", "error", *args], input=data, check=True, capture_output=True).stdout


def _pcm(raw):
    return np.frombuffer(raw, dtype=np.float32).copy()


def decode(path, rate=REFERENCE_RATE, extra_filter=""):
    """Mono float32 PCM of any file ffmpeg reads, resampled with soxr to `rate`."""
    chain = ",".join(part for part in (extra_filter, "aresample=resampler=soxr") if part)
    return _pcm(_ffmpeg(["-i", str(path), "-af", chain, "-ac", "1", "-ar", str(rate), "-f", "f32le", "-"]))


def decode_mp3(data, rate=REFERENCE_RATE):
    # From a file: the decoder honours the LAME delay and padding only when it
    # can read the Xing frame as a header.
    with tempfile.TemporaryDirectory() as scratch:
        path = Path(scratch) / "in.mp3"
        path.write_bytes(data)
        return decode(path, rate)


def _soxr(pcm, rate, target):
    return _pcm(_ffmpeg(["-f", "f32le", "-ar", str(rate), "-ac", "1", "-i", "-", "-af", "aresample=resampler=soxr",
                         "-ar", str(target), "-f", "f32le", "-"], pcm.astype(np.float32).tobytes()))


def _fit(pcm, length):
    return pcm[:length] if len(pcm) >= length else np.pad(pcm, (0, length - len(pcm)))


def resample(pcm, rate, target, loop=False):
    """`pcm` at `target` Hz with round(len * target / rate) samples. A loop is
    resampled as a periodic signal, so its seam stays continuous."""
    length = round(len(pcm) * target / rate)
    if target == rate:
        return pcm.astype(np.float32, copy=True)
    if not loop:
        return _fit(_soxr(pcm, rate, target), length)
    step = rate // math.gcd(rate, target)
    units = math.ceil(LOOP_PAD_SECONDS * rate / step)
    pad = units * step
    cycles = np.tile(pcm, -(-pad // len(pcm)))
    out = _soxr(np.concatenate([cycles[-pad:], pcm, cycles[:pad]]), rate, target)
    start = units * (target // math.gcd(rate, target))
    return _fit(out[start:], length)


def db(value):
    return 20.0 * math.log10(max(float(value), 1e-9))


def integrated_lufs(pcm, rate):
    log = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-f", "f32le", "-ar", str(rate), "-ac", "1", "-i", "-",
                          "-af", "ebur128", "-f", "null", "-"], input=pcm.astype(np.float32).tobytes(),
                         check=True, capture_output=True).stderr.decode("utf-8", "replace")
    return float(re.findall(r"I:\s+(-?[\d.]+) LUFS", log)[-1])


def shape_effect(pcm, rate, trim_db=EFFECT["trim_db"], fade_ms=EFFECT["fade_ms"], peak_db=EFFECT["peak_db"]):
    """Trim to the sound (trim_db under its peak), fade out, peak-normalise."""
    peak = float(np.max(np.abs(pcm)))
    loud = np.nonzero(np.abs(pcm) > peak * 10.0 ** (trim_db / 20.0))[0]
    pcm = pcm[loud[0]:loud[-1] + 1].astype(np.float32, copy=True)
    fade = min(len(pcm), int(rate * fade_ms / 1000.0))
    pcm[len(pcm) - fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32)
    gain_db = peak_db - db(peak)
    return pcm * np.float32(10.0 ** (gain_db / 20.0)), {"gain_db": round(gain_db, 2)}


def shape_music(pcm, rate, lufs=MUSIC["lufs"], peak_db=MUSIC["peak_db"]):
    """Gain to `lufs` integrated, lowered further if the peak would pass
    `peak_db`. Never trimmed: the length is the loop."""
    gain_db = min(lufs - integrated_lufs(pcm, rate), peak_db - db(np.max(np.abs(pcm))))
    pcm = pcm * np.float32(10.0 ** (gain_db / 20.0))
    return pcm, {"gain_db": round(gain_db, 2), "lufs": round(integrated_lufs(pcm, rate), 1)}


def _info_tag(data):
    """Offsets of the first frame's Xing/Info header and its LAME (or Lavc) tag,
    or None when the file has no such header."""
    at = 0
    if data[:3] == b"ID3":
        at = 10 + ((data[6] & 0x7F) << 21 | (data[7] & 0x7F) << 14 | (data[8] & 0x7F) << 7 | (data[9] & 0x7F))
    if len(data) < at + 4 or data[at] != 0xFF or (data[at + 1] & 0xE0) != 0xE0:
        return None
    mpeg1 = (data[at + 1] >> 3) & 0x03 == 0x03
    mono = (data[at + 3] >> 6) == 0x03
    crc = 0 if data[at + 1] & 0x01 else 2
    side_info = (17 if mono else 32) if mpeg1 else (9 if mono else 17)
    xing = at + 4 + crc + side_info
    if data[xing:xing + 4] not in (b"Xing", b"Info"):
        return None
    flags = int.from_bytes(data[xing + 4:xing + 8], "big")
    lame = xing + 8 + (4 if flags & 1 else 0) + (4 if flags & 2 else 0) + (100 if flags & 4 else 0)         + (4 if flags & 8 else 0)
    if data[lame:lame + 4] not in (b"LAME", b"Lavc", b"Lavf") or len(data) < lame + 36:
        return None
    frames = int.from_bytes(data[xing + 8:xing + 12], "big") if flags & 1 else None
    return {"frame": at, "lame": lame, "frames": frames, "frame_samples": 1152 if mpeg1 else 576}


def gapless_header(data):
    """True when the first MPEG audio frame is a Xing/Info frame carrying a LAME
    (or Lavc) tag, the one place decoders read the encoder delay and padding."""
    return _info_tag(data) is not None


def _crc16(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def _set_padding(data, samples):
    """Rewrite the LAME tag's end padding so a decoder returns exactly `samples`.
    libavformat's muxer records one frame too little when the padding spans two
    frames (len mod frame size in 1..46), so those files decode up to 46
    samples long."""
    tag = _info_tag(data)
    if tag is None or tag["frames"] is None:
        raise RuntimeError("ffmpeg wrote no Xing/LAME header with a frame count: the export would not be gapless")
    lame = tag["lame"]
    delay = int.from_bytes(data[lame + 21:lame + 24], "big") >> 12
    # Decoders skip delay + 529 samples at the start and padding - 529 at the
    # end of frames * frame_samples, so the 529 cancels out.
    padding = tag["frames"] * tag["frame_samples"] - delay - samples
    if not 0 <= padding < 1 << 12:
        raise RuntimeError(f"LAME padding {padding} does not fit the tag: frames {tag['frames']}, delay {delay}")
    out = bytearray(data)
    out[lame + 21:lame + 24] = (delay << 12 | padding).to_bytes(3, "big")
    # The tag CRC covers the frame up to the CRC field, as LAME writes it.
    out[lame + 34:lame + 36] = _crc16(out[tag["frame"]:lame + 34]).to_bytes(2, "big")
    return bytes(out)


def encode(pcm, rate, quality):
    """Mono libmp3lame VBR at -q:a `quality`, no tags, with a gapless header
    that decodes to exactly len(pcm) samples."""
    # To a file, not a pipe: the muxer writes the Xing/LAME header by seeking
    # back to the first frame, and a pipe silently drops it.
    with tempfile.TemporaryDirectory() as scratch:
        out = Path(scratch) / "export.mp3"
        _ffmpeg(["-f", "f32le", "-ar", str(rate), "-ac", "1", "-i", "-", "-c:a", "libmp3lame", "-q:a", str(quality),
                 "-map_metadata", "-1", "-id3v2_version", "0", "-write_xing", "1", str(out)],
                pcm.astype(np.float32).tobytes())
        data = _set_padding(out.read_bytes(), len(pcm))
    decoded = len(decode_mp3(data, rate))
    if decoded != len(pcm):
        raise RuntimeError(f"export decodes to {decoded} samples, not {len(pcm)}: the export would not be gapless")
    return data


def level_change_db(reference, candidate):
    """RMS level of `candidate` relative to `reference`, in dB."""
    rms = lambda pcm: math.sqrt(float(np.mean(np.square(pcm, dtype=np.float64))))
    return db(rms(candidate)) - db(rms(reference))


def long_term_spectrum(pcm, rate):
    frames = np.lib.stride_tricks.sliding_window_view(np.pad(pcm, (0, max(0, SPECTRUM_FRAME - len(pcm)))),
                                                      SPECTRUM_FRAME)[::SPECTRUM_HOP]
    power = np.mean(np.abs(np.fft.rfft(frames * np.hanning(SPECTRUM_FRAME), axis=1)) ** 2, axis=0)
    return power, np.fft.rfftfreq(SPECTRUM_FRAME, 1.0 / rate)


def _band_balance(pcm, rate):
    power, freq = long_term_spectrum(pcm, rate)
    in_band = lambda low, high: power[(freq >= low) & (freq < high)].sum()
    total = in_band(*BALANCE_TOTAL_HZ) + 1e-20
    shares = [10.0 * math.log10((in_band(low, high) + 1e-20) / total) for low, high in BALANCE_BANDS_HZ]
    return np.maximum(shares, BALANCE_FLOOR_DB)


def balance_shift_db(reference, candidate, rate=REFERENCE_RATE):
    """RMS over 1 kHz bands from 1 to 7 kHz of how much each band's share of
    the 100 Hz-7.5 kHz energy moved, in dB; shares under BALANCE_FLOOR_DB count
    as the floor. Both signals are at `rate`."""
    shift = _band_balance(candidate, rate) - _band_balance(reference, rate)
    return float(np.sqrt(np.mean(shift ** 2)))


@dataclass
class Export:
    rate: int
    data: bytes
    samples: int
    candidates: tuple = ()
    trials: list = field(default_factory=list)

    def provenance(self):
        """The choice as a JSON-ready record: the rule, and what each tried rate measured."""
        if not self.candidates:
            return {"rule": "fixed rate", "trials": []}
        return {"rule": f"lowest of {list(self.candidates)} Hz within {MAX_LEVEL_CHANGE_DB} dB level and "
                        f"{MAX_BALANCE_SHIFT_DB} dB spectral balance of the {REFERENCE_RATE} Hz encode, "
                        f"else {REFERENCE_RATE} Hz",
                "trials": self.trials}


def export_at(shaped, rate, quality, reference_rate=REFERENCE_RATE, loop=False):
    """Resample shaped reference-rate PCM to `rate` and encode it."""
    pcm = resample(shaped, reference_rate, rate, loop)
    return Export(rate, encode(pcm, rate, quality), len(pcm))


def choose_rate(shaped, quality, reference_rate=REFERENCE_RATE, candidates=CANDIDATE_RATES, loop=False):
    """Export at the lowest candidate rate the rule accepts, else at `reference_rate`.
    `shaped` is the finished sound at `reference_rate`."""
    reference = export_at(shaped, reference_rate, quality, reference_rate, loop)
    heard = decode_mp3(reference.data, reference_rate)
    trials = []
    tried = tuple(sorted(rate for rate in candidates if rate < reference_rate))
    for rate in tried:
        candidate = export_at(shaped, rate, quality, reference_rate, loop)
        decoded = decode_mp3(candidate.data, reference_rate)
        level = level_change_db(heard, decoded)
        balance = balance_shift_db(heard, decoded, reference_rate)
        accepted = abs(level) < MAX_LEVEL_CHANGE_DB and balance < MAX_BALANCE_SHIFT_DB
        trials.append({"rate": rate, "bytes": len(candidate.data), "level_change_db": round(level, 2),
                       "balance_shift_db": round(balance, 2), "accepted": accepted})
        if accepted:
            candidate.candidates, candidate.trials = tried, trials
            return candidate
    reference.candidates, reference.trials = tried, trials
    return reference


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source")
    parser.add_argument("out")
    parser.add_argument("--music", action="store_true", help="a loop: loudness-shaped, untrimmed, resampled as periodic")
    parser.add_argument("--rate", type=int, help="skip the rule and export at this rate")
    args = parser.parse_args(argv)
    knobs = MUSIC if args.music else EFFECT
    pcm = decode(args.source)
    shaped, report = (shape_music(pcm, REFERENCE_RATE) if args.music else shape_effect(pcm, REFERENCE_RATE))
    result = (export_at(shaped, args.rate, knobs["quality"], loop=args.music) if args.rate
              else choose_rate(shaped, knobs["quality"], loop=args.music))
    Path(args.out).write_bytes(result.data)
    print(json.dumps({"rate": result.rate, "bytes": len(result.data), "seconds": round(result.samples / result.rate, 3),
                      "shaping": report, **result.provenance()}, indent=1))


if __name__ == "__main__":
    main(sys.argv[1:])
