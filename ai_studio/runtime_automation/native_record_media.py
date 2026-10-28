"""Validate saved native recording media independently of its DevAPI receipt."""
from __future__ import annotations

import array
from fractions import Fraction
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile


def _decode(path: Path, options: list[str], consume):
    with tempfile.TemporaryFile() as errors:
        process = subprocess.Popen(
            ["ffmpeg", "-v", "error", "-xerror", "-i", str(path), *options, "pipe:1"],
            stdout=subprocess.PIPE, stderr=errors,
        )
        try:
            result = consume(process.stdout)
            status = process.wait()
            if status:
                errors.seek(0)
                raise RuntimeError("recording decode failed: " + errors.read(4000).decode("utf-8", errors="replace"))
            return result
        finally:
            process.stdout.close()
            if process.poll() is None:
                process.kill()
                process.wait()


def _video_stats(stream):
    frames = 0
    hashes = set()
    previous = None
    repeated = longest = 0
    previous_pts = None
    step = None
    for raw in stream:
        line = raw.decode("ascii").strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(",")
        if len(parts) != 6:
            raise RuntimeError("unexpected decoded frame checksum record")
        pts = int(parts[2])
        if previous_pts is not None:
            delta = pts - previous_pts
            if delta <= 0 or (step is not None and delta != step):
                raise RuntimeError("recording video timestamps are not uniformly increasing")
            step = delta
        previous_pts = pts
        digest = parts[-1].strip()
        hashes.add(digest)
        repeated = repeated + 1 if digest == previous else 1
        longest = max(longest, repeated)
        previous = digest
        frames += 1
    return {"decoded_frames": frames, "distinct_frames": len(hashes), "longest_identical_run": longest}


def _audio_stats(stream):
    samples = 0
    square_sum = 0.0
    peak = 0.0
    for data in iter(lambda: stream.read(65536), b""):
        block = array.array("f")
        block.frombytes(data)
        if sys.byteorder != "little":
            block.byteswap()
        for value in block:
            if not math.isfinite(value):
                raise RuntimeError("decoded recording audio contains non-finite samples")
            peak = max(peak, abs(value))
            square_sum += value * value
        samples += len(block)
    if samples % 2:
        raise RuntimeError("decoded recording audio has an incomplete stereo frame")
    rms = math.sqrt(square_sum / max(1, samples))
    return {"decoded_audio_frames": samples // 2, "peak": peak,
            "rms_dbfs": 20 * math.log10(max(1e-12, rms))}


def validate_native_media(
    path: Path, *, width: int, height: int, fps: int, receipt: dict,
    expect_motion: bool = False, expect_audio: bool = False,
    max_freeze_seconds: float | None = None,
) -> dict:
    """Decode both streams; static/muted takes are allowed unless a shot expects activity."""
    path = Path(path)
    if not path.is_file() or not path.stat().st_size:
        raise RuntimeError("native recorder did not produce a nonempty MP4")
    info = json.loads(subprocess.run(
        ["ffprobe", "-v", "error", "-show_streams", "-of", "json", str(path)],
        check=True, capture_output=True, text=True,
    ).stdout)
    video = [x for x in info.get("streams", []) if x.get("codec_type") == "video"]
    audio = [x for x in info.get("streams", []) if x.get("codec_type") == "audio"]
    if len(video) != 1 or len(audio) != 1:
        raise RuntimeError("native MP4 must contain one video and one audio stream")
    video, audio = video[0], audio[0]
    if video.get("codec_name") != "h264" or audio.get("codec_name") != "aac":
        raise RuntimeError("native MP4 must contain H.264 video and AAC audio")
    if (video.get("width"), video.get("height")) != (width, height):
        raise RuntimeError("recorded video dimensions do not match the requested framebuffer")
    if Fraction(video.get("avg_frame_rate", "0")) != fps:
        raise RuntimeError("recorded video frame rate differs from the requested rate")
    if int(audio.get("sample_rate", 0)) != 48000 or audio.get("channels") != 2:
        raise RuntimeError("recorded audio must be 48 kHz stereo")
    try:
        video_start = float(video.get("start_time", "nan"))
        audio_start = float(audio.get("start_time", "nan"))
    except (TypeError, ValueError) as error:
        raise RuntimeError("recording stream start timestamps are unavailable") from error
    if not math.isfinite(video_start) or not math.isfinite(audio_start):
        raise RuntimeError("recording stream start timestamps are unavailable")
    start_skew_ms = (audio_start - video_start) * 1000
    if abs(start_skew_ms) > 100 + 1000 / fps:
        raise RuntimeError("recorded audio/video start timestamps are misaligned")
    frames = _decode(path, ["-map", "0:v:0", "-an", "-fps_mode", "passthrough", "-f", "framemd5"], _video_stats)
    sound = _decode(path, ["-map", "0:a:0", "-vn", "-f", "f32le", "-ac", "2", "-ar", "48000"], _audio_stats)
    if frames["decoded_frames"] <= 0 or sound["decoded_audio_frames"] <= 0:
        raise RuntimeError("recording has an empty decoded media stream")
    if frames["decoded_frames"] != receipt["frames"]:
        raise RuntimeError("decoded video frame count differs from the recorder receipt")
    # The encoder pads the audio to a video-frame boundary and AAC codes full blocks.
    expected_audio = max(receipt["audio_frames"], receipt["frames"] * 48000 / fps)
    if abs(sound["decoded_audio_frames"] - expected_audio) > 2048:
        raise RuntimeError("decoded audio length differs from the captured mix beyond AAC padding")
    ratio = frames["distinct_frames"] / max(1, frames["decoded_frames"])
    if expect_motion and ratio < .8:
        raise RuntimeError(f"recorded motion has only {ratio:.1%} distinct decoded frames")
    freeze = (frames["longest_identical_run"] - 1) / fps
    if max_freeze_seconds is not None and freeze > max_freeze_seconds:
        raise RuntimeError(f"recording contains a {freeze:.3f}s identical-frame freeze")
    if expect_audio and (sound["peak"] <= .001 or sound["rms_dbfs"] <= -70):
        raise RuntimeError("recording expected audible game sound but decoded only silence or a near-silent signal")
    return {"video_codec": video["codec_name"], "audio_codec": audio["codec_name"],
            "width": width, "height": height, "fps": fps, "audio_rate": 48000, "channels": 2,
            **frames, **sound, "distinct_ratio": ratio, "longest_freeze_seconds": freeze,
            "track_start_skew_ms": start_skew_ms,
            "expect_motion": expect_motion, "expect_audio": expect_audio}
