"""Controlled audiovisual fixture generation and decoded validation.

The fixture is intentionally small and deterministic enough for contract tests.
It contains a changing video test pattern with frame numbers, 48 kHz tone, and
co-timed full-frame flashes / PCM impulses.

Filter syntax: https://ffmpeg.org/ffmpeg-filters.html
Packet/stream inspection: https://ffmpeg.org/ffprobe.html
"""

from __future__ import annotations

import array
import hashlib
import json
import math
import os
import shutil
import struct
import subprocess
import tempfile
import wave
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Sequence

from capture_contracts import CaptureContractError, canonical_json_bytes


@dataclass(frozen=True)
class FixtureSpec:
    width: int = 320
    height: int = 180
    fps: int = 30
    duration_seconds: float = 2.0
    sample_rate: int = 48000
    marker_times: tuple[float, ...] = (0.25, 0.75)

    def __post_init__(self) -> None:
        if not (64 <= self.width <= 1920 and 64 <= self.height <= 1080):
            raise ValueError("fixture dimensions must be between 64 and 1920x1080")
        if not (1 <= self.fps <= 120):
            raise ValueError("fixture FPS must be between 1 and 120")
        if not (0.5 <= self.duration_seconds <= 30):
            raise ValueError("fixture duration must be between 0.5 and 30 seconds")
        if self.sample_rate != 48000:
            raise ValueError("WP0 fixture sample rate is fixed at 48 kHz")
        if any(not (0 < marker < self.duration_seconds) for marker in self.marker_times):
            raise ValueError("fixture markers must lie inside the duration")


def _failure(
    message: str,
    *,
    stage: str,
    details: dict[str, Any] | None = None,
) -> CaptureContractError:
    safe = {"stage": stage}
    safe.update(details or {})
    return CaptureContractError(
        "AV_VALIDATION_FAILED",
        message,
        remediation="Regenerate the controlled fixture and inspect the bounded media diagnostics.",
        details=safe,
    )


def _tool(name: str) -> str:
    resolved = shutil.which(name)
    if not resolved:
        raise _failure(f"{name} is unavailable", stage="tool-discovery")
    return resolved


def _run(arguments: Sequence[str], *, stage: str, timeout: float = 30) -> bytes:
    try:
        result = subprocess.run(
            list(arguments),
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.SubprocessError) as error:
        raise _failure(
            f"media tool failed during {stage}",
            stage=stage,
            details={"error": str(error)[:1000]},
        ) from error
    if result.returncode != 0:
        raise _failure(
            f"media tool returned {result.returncode} during {stage}",
            stage=stage,
            details={"stderr": result.stderr.decode("utf-8", "replace")[-2000:]},
        )
    return result.stdout


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _write_sync_wav(path: Path, spec: FixtureSpec) -> None:
    sample_count = round(spec.duration_seconds * spec.sample_rate)
    marker_samples = {round(marker * spec.sample_rate) for marker in spec.marker_times}
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(spec.sample_rate)
        chunk = bytearray()
        for index in range(sample_count):
            tone = 0.08 * math.sin(2 * math.pi * 440 * index / spec.sample_rate)
            impulse = 0.0
            if any(marker <= index < marker + 8 for marker in marker_samples):
                impulse = 0.9
            sample = max(-1.0, min(1.0, tone + impulse))
            chunk.extend(struct.pack("<h", round(sample * 32767)))
            if len(chunk) >= 64 * 1024:
                output.writeframesraw(chunk)
                chunk.clear()
        if chunk:
            output.writeframesraw(chunk)


def _video_filter(spec: FixtureSpec) -> str:
    flashes = "+".join(
        f"between(t,{marker:.9f},{marker + 1 / spec.fps:.9f})"
        for marker in spec.marker_times
    ) or "0"
    return (
        f"testsrc2=size={spec.width}x{spec.height}:rate={spec.fps}:"
        f"duration={spec.duration_seconds},"
        "drawtext=text='%{n}':x=6:y=6:fontsize=18:fontcolor=white:"
        "box=1:boxcolor=black@0.6,"
        f"drawbox=x=0:y=0:w=iw:h=ih:color=white:t=fill:enable='{flashes}'"
    )


def generate_synthetic_fixture(path: Path | str, spec: FixtureSpec) -> dict[str, Any]:
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    ffmpeg = _tool("ffmpeg")
    with tempfile.NamedTemporaryFile(
        prefix=f".{output.name}.",
        suffix=".fixture.wav",
        dir=output.parent,
        delete=False,
    ) as temporary:
        wav_path = Path(temporary.name)
    with tempfile.NamedTemporaryFile(
        prefix=f".{output.name}.",
        suffix=".staging.mkv",
        dir=output.parent,
        delete=False,
    ) as temporary:
        staging = Path(temporary.name)
    manifest_path = output.with_suffix(output.suffix + ".fixture.json")
    _write_sync_wav(wav_path, spec)
    try:
        _run(
            [
                ffmpeg,
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-f",
                "lavfi",
                "-i",
                _video_filter(spec),
                "-i",
                str(wav_path),
                "-map",
                "0:v:0",
                "-map",
                "1:a:0",
                "-c:v",
                "libx264",
                "-preset",
                "ultrafast",
                "-tune",
                "zerolatency",
                "-pix_fmt",
                "yuv420p",
                "-g",
                str(spec.fps),
                "-c:a",
                "pcm_s16le",
                "-shortest",
                "-metadata",
                "fixture_schema=ai_studio.capture.av_fixture.v1",
                str(staging),
            ],
            stage="fixture-generation",
        )
        if not staging.is_file() or staging.stat().st_size == 0:
            raise _failure("FFmpeg produced no fixture", stage="fixture-generation")
        os.replace(staging, output)
    finally:
        wav_path.unlink(missing_ok=True)
        staging.unlink(missing_ok=True)

    spec_payload = asdict(spec)
    spec_payload["marker_times"] = list(spec.marker_times)
    manifest = {
        "schema": "ai_studio.capture.av_fixture.v1",
        "spec": spec_payload,
        "sha256": _sha256_file(output),
        "bytes": output.stat().st_size,
    }
    with tempfile.NamedTemporaryFile(
        prefix=f".{manifest_path.name}.",
        suffix=".tmp",
        dir=manifest_path.parent,
        delete=False,
    ) as temporary:
        manifest_staging = Path(temporary.name)
        temporary.write(canonical_json_bytes(manifest) + b"\n")
    os.replace(manifest_staging, manifest_path)
    return manifest


def _probe(path: Path) -> dict[str, Any]:
    output = _run(
        [
            _tool("ffprobe"),
            "-v",
            "error",
            "-count_frames",
            "-show_streams",
            "-show_format",
            "-show_packets",
            "-of",
            "json",
            str(path),
        ],
        stage="ffprobe",
    )
    try:
        return json.loads(output)
    except json.JSONDecodeError as error:
        raise _failure("ffprobe returned invalid JSON", stage="ffprobe") from error


def _fraction(value: str) -> float:
    numerator, denominator = value.split("/", 1)
    return int(numerator) / int(denominator)


def _packet_timing(
    packets: Sequence[dict[str, Any]],
    stream_index: int,
    expected_duration: float,
    max_gap: float,
) -> dict[str, Any]:
    selected = [
        packet for packet in packets if int(packet.get("stream_index", -1)) == stream_index
    ]
    try:
        starts = [float(packet["pts_time"]) for packet in selected]
        durations = [float(packet.get("duration_time", 0) or 0) for packet in selected]
    except (KeyError, TypeError, ValueError) as error:
        raise _failure("packet timestamp is unavailable", stage="timestamp") from error
    if not starts:
        raise _failure("stream has no timestamped packets", stage="timestamp")
    monotonic = all(right >= left for left, right in zip(starts, starts[1:]))
    gaps = [
        max(0.0, right - (left + duration))
        for left, duration, right in zip(starts, durations, starts[1:])
    ]
    largest_gap = max(gaps, default=0.0)
    last_duration = durations[-1]
    end = starts[-1] + last_duration
    end_drift = abs(end - expected_duration)
    if not monotonic or largest_gap > max_gap or end_drift > max_gap:
        raise _failure(
            "fixture packet timestamps are outside tolerance",
            stage="timestamp",
            details={
                "monotonic": monotonic,
                "max_gap_seconds": largest_gap,
                "end_drift_seconds": end_drift,
            },
        )
    return {
        "monotonic": monotonic,
        "first_pts_seconds": starts[0],
        "last_end_seconds": end,
        "max_gap_seconds": largest_gap,
        "end_drift_seconds": end_drift,
        "packet_count": len(selected),
    }


def _decode_video_means(
    path: Path, width: int, height: int, expected_frames: int
) -> list[float]:
    arguments = [
        _tool("ffmpeg"),
        "-v",
        "error",
        "-i",
        str(path),
        "-map",
        "0:v:0",
        "-f",
        "rawvideo",
        "-pix_fmt",
        "gray",
        "-",
    ]
    frame_bytes = width * height
    means: list[float] = []
    with tempfile.TemporaryFile() as diagnostics:
        try:
            process = subprocess.Popen(
                arguments,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=diagnostics,
            )
        except OSError as error:
            raise _failure(
                "media tool failed during video-decode",
                stage="video-decode",
                details={"error": str(error)[:1000]},
            ) from error
        assert process.stdout is not None
        partial = bytearray()
        try:
            with process.stdout:
                while True:
                    block = process.stdout.read(
                        min(frame_bytes - len(partial), 1024 * 1024)
                    )
                    if not block:
                        break
                    partial.extend(block)
                    if len(partial) == frame_bytes:
                        means.append(sum(partial) / frame_bytes)
                        partial.clear()
                        if len(means) > expected_frames:
                            process.kill()
                            raise _failure(
                                "decoded video exceeds its declared frame count",
                                stage="video-decode",
                                details={"expected_frames": expected_frames},
                            )
            return_code = process.wait(timeout=30)
        except (OSError, subprocess.SubprocessError, CaptureContractError):
            process.kill()
            process.wait()
            raise
        if return_code != 0:
            diagnostics.seek(0, os.SEEK_END)
            size = diagnostics.tell()
            diagnostics.seek(max(0, size - 2000))
            stderr = diagnostics.read().decode("utf-8", "replace")
            raise _failure(
                f"media tool returned {return_code} during video-decode",
                stage="video-decode",
                details={"stderr": stderr},
            )
    if partial or len(means) != expected_frames:
        raise _failure(
            "decoded video does not contain its declared complete frames",
            stage="video-decode",
            details={
                "decoded_frames": len(means),
                "partial_bytes": len(partial),
                "expected_frames": expected_frames,
            },
        )
    return means


def _decode_audio_samples(path: Path, sample_rate: int) -> array.array:
    raw = _run(
        [
            _tool("ffmpeg"),
            "-v",
            "error",
            "-i",
            str(path),
            "-map",
            "0:a:0",
            "-f",
            "s16le",
            "-ac",
            "1",
            "-ar",
            str(sample_rate),
            "-",
        ],
        stage="audio-decode",
    )
    if not raw or len(raw) % 2:
        raise _failure("decoded audio is incomplete", stage="audio-decode")
    samples = array.array("h")
    samples.frombytes(raw)
    if os.sys.byteorder != "little":
        samples.byteswap()
    return samples


def _marker_peaks(
    values: Sequence[float],
    *,
    threshold: float,
    prominence: float,
    absolute: bool,
) -> list[int]:
    magnitudes = [abs(value) if absolute else value for value in values]
    candidates = [
        index for index, value in enumerate(magnitudes) if value >= threshold
    ]
    groups: list[list[int]] = []
    for index in candidates:
        if not groups or index != groups[-1][-1] + 1:
            groups.append([index])
        else:
            groups[-1].append(index)

    peaks: list[int] = []
    for group in groups:
        peak = max(group, key=lambda index: magnitudes[index])
        left = max(0, group[0] - 2)
        right = min(len(values), group[-1] + 3)
        background = [
            magnitudes[index]
            for index in range(left, right)
            if index < group[0] or index > group[-1]
        ]
        baseline = max(background, default=0)
        if magnitudes[peak] - baseline >= prominence:
            peaks.append(peak)
    return peaks


def validate_synthetic_fixture(
    path: Path | str, spec: FixtureSpec
) -> dict[str, Any]:
    source = Path(path)
    if not source.is_file():
        raise _failure("fixture file is missing", stage="preflight")
    probe = _probe(source)
    streams = probe.get("streams", [])
    videos = [stream for stream in streams if stream.get("codec_type") == "video"]
    audios = [stream for stream in streams if stream.get("codec_type") == "audio"]
    if len(videos) != 1 or len(audios) != 1:
        raise _failure(
            "fixture must contain exactly one video and one audio stream",
            stage="structural",
            details={"video_streams": len(videos), "audio_streams": len(audios)},
        )
    video = videos[0]
    audio = audios[0]
    actual = {
        "codec": video.get("codec_name"),
        "width": int(video.get("width", 0)),
        "height": int(video.get("height", 0)),
        "fps": _fraction(video.get("avg_frame_rate", "0/1")),
        "frames": int(video.get("nb_read_frames", 0)),
    }
    audio_actual = {
        "codec": audio.get("codec_name"),
        "sample_rate": int(audio.get("sample_rate", 0)),
        "channels": int(audio.get("channels", 0)),
    }
    expected_frames = round(spec.duration_seconds * spec.fps)
    if (
        actual["width"] != spec.width
        or actual["height"] != spec.height
        or actual["codec"] != "h264"
        or not math.isclose(actual["fps"], spec.fps, abs_tol=1e-6)
        or actual["frames"] != expected_frames
        or audio_actual["sample_rate"] != spec.sample_rate
        or audio_actual["codec"] != "pcm_s16le"
        or audio_actual["channels"] != 1
    ):
        raise _failure(
            "fixture stream structure does not match its declared specification",
            stage="structural",
            details={"video": actual, "audio": audio_actual},
        )
    try:
        duration = float(probe["format"]["duration"])
    except (KeyError, TypeError, ValueError) as error:
        raise _failure("fixture duration is unavailable", stage="timestamp") from error
    if not math.isclose(duration, spec.duration_seconds, abs_tol=1 / spec.fps):
        raise _failure(
            "fixture duration is outside tolerance",
            stage="timestamp",
            details={"expected": spec.duration_seconds, "actual": duration},
        )
    packets = probe.get("packets", [])
    timestamp_report = {
        "status": "pass",
        "duration_seconds": duration,
        "video": _packet_timing(
            packets,
            int(video["index"]),
            spec.duration_seconds,
            2 / spec.fps,
        ),
        "audio": _packet_timing(
            packets,
            int(audio["index"]),
            spec.duration_seconds,
            2 / spec.fps,
        ),
    }

    video_means = _decode_video_means(
        source, spec.width, spec.height, expected_frames
    )
    audio_samples = _decode_audio_samples(source, spec.sample_rate)
    video_indices = _marker_peaks(
        video_means,
        threshold=220,
        prominence=40,
        absolute=False,
    )
    audio_indices = _marker_peaks(
        audio_samples,
        threshold=16_000,
        prominence=10_000,
        absolute=True,
    )
    expected_marker_count = len(spec.marker_times)
    if (
        len(video_indices) != expected_marker_count
        or len(audio_indices) != expected_marker_count
    ):
        raise _failure(
            "decoded media does not contain the declared sync marker signature",
            stage="content-sync",
            details={
                "expected_markers": expected_marker_count,
                "video_markers": len(video_indices),
                "audio_markers": len(audio_indices),
            },
        )

    video_pts = timestamp_report["video"]["first_pts_seconds"]
    audio_pts = timestamp_report["audio"]["first_pts_seconds"]
    markers = []
    for expected, video_index, audio_index in zip(
        spec.marker_times, video_indices, audio_indices
    ):
        video_time = video_pts + video_index / spec.fps
        audio_time = audio_pts + audio_index / spec.sample_rate
        offset = abs(video_time - audio_time)
        video_expected_offset = abs((video_time - video_pts) - expected)
        audio_expected_offset = abs((audio_time - audio_pts) - expected)
        if (
            offset > 1 / spec.fps
            or video_expected_offset > 1 / spec.fps
            or audio_expected_offset > 1 / spec.fps
        ):
            raise _failure(
                "decoded flash and impulse exceed sync tolerance",
                stage="content-sync",
                details={
                    "expected": expected,
                    "offset_seconds": offset,
                    "video_expected_offset_seconds": video_expected_offset,
                    "audio_expected_offset_seconds": audio_expected_offset,
                },
            )
        markers.append(
            {
                "expected_seconds": expected,
                "video_seconds": video_time,
                "audio_seconds": audio_time,
                "absolute_offset_seconds": offset,
            }
        )

    return {
        "schema": "ai_studio.capture.av_fixture_report.v1",
        "sha256": _sha256_file(source),
        "structural": {"status": "pass"},
        "timestamp": timestamp_report,
        "content_sync": {"status": "pass", "markers": markers},
        "video": actual,
        "audio": audio_actual,
        "ffprobe": probe,
    }
