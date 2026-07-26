"""Finite-WAV FFmpeg process-loopback feasibility adapter.

This module deliberately stops short of the recorder backend contract. FFmpeg
captures an owned HWND directly through ``gdigrab`` and combines it with
isolated process audio. Production streaming and clock synchronization remain
open.
"""

from __future__ import annotations

import ctypes
import json
import math
import os
import signal
import subprocess
import time
from ctypes import wintypes
from fractions import Fraction
from pathlib import Path
from threading import Event
from typing import Callable, Sequence


class FfmpegProcessLoopbackSpikeError(RuntimeError):
    """A stable failure raised by the finite-WAV FFmpeg spike."""


class _JobBasicLimitInformation(ctypes.Structure):
    _fields_ = [
        ("PerProcessUserTimeLimit", ctypes.c_longlong),
        ("PerJobUserTimeLimit", ctypes.c_longlong),
        ("LimitFlags", wintypes.DWORD),
        ("MinimumWorkingSetSize", ctypes.c_size_t),
        ("MaximumWorkingSetSize", ctypes.c_size_t),
        ("ActiveProcessLimit", wintypes.DWORD),
        ("Affinity", ctypes.c_size_t),
        ("PriorityClass", wintypes.DWORD),
        ("SchedulingClass", wintypes.DWORD),
    ]


class _IoCounters(ctypes.Structure):
    _fields_ = [
        ("ReadOperationCount", ctypes.c_ulonglong),
        ("WriteOperationCount", ctypes.c_ulonglong),
        ("OtherOperationCount", ctypes.c_ulonglong),
        ("ReadTransferCount", ctypes.c_ulonglong),
        ("WriteTransferCount", ctypes.c_ulonglong),
        ("OtherTransferCount", ctypes.c_ulonglong),
    ]


class _JobExtendedLimitInformation(ctypes.Structure):
    _fields_ = [
        ("BasicLimitInformation", _JobBasicLimitInformation),
        ("IoInfo", _IoCounters),
        ("ProcessMemoryLimit", ctypes.c_size_t),
        ("JobMemoryLimit", ctypes.c_size_t),
        ("PeakProcessMemoryUsed", ctypes.c_size_t),
        ("PeakJobMemoryUsed", ctypes.c_size_t),
    ]


class _WindowsKillJob:
    def __init__(self) -> None:
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
        kernel32.CreateJobObjectW.restype = wintypes.HANDLE
        kernel32.SetInformationJobObject.argtypes = [
            wintypes.HANDLE,
            ctypes.c_int,
            ctypes.c_void_p,
            wintypes.DWORD,
        ]
        kernel32.SetInformationJobObject.restype = wintypes.BOOL
        kernel32.AssignProcessToJobObject.argtypes = [
            wintypes.HANDLE,
            wintypes.HANDLE,
        ]
        kernel32.AssignProcessToJobObject.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel32.CloseHandle.restype = wintypes.BOOL
        self._kernel32 = kernel32
        self._handle = kernel32.CreateJobObjectW(None, None)
        if not self._handle:
            raise OSError(ctypes.get_last_error(), "CreateJobObjectW failed")
        information = _JobExtendedLimitInformation()
        information.BasicLimitInformation.LimitFlags = 0x00002000
        if not kernel32.SetInformationJobObject(
            self._handle,
            9,
            ctypes.byref(information),
            ctypes.sizeof(information),
        ):
            error = ctypes.get_last_error()
            self.close()
            raise OSError(error, "SetInformationJobObject failed")

    def assign(self, process: subprocess.Popen) -> None:
        if not self._kernel32.AssignProcessToJobObject(
            self._handle, wintypes.HANDLE(process._handle)
        ):
            raise OSError(
                ctypes.get_last_error(), "AssignProcessToJobObject failed"
            )

    def close(self) -> None:
        if self._handle:
            self._kernel32.CloseHandle(self._handle)
            self._handle = None


def _terminate_owned_tree(
    process: subprocess.Popen,
    *,
    force: bool,
    windows_job: _WindowsKillJob | None,
) -> None:
    if os.name != "nt":
        if process.poll() is not None:
            return
        try:
            os.killpg(
                os.getpgid(process.pid),
                signal.SIGKILL if force else signal.SIGTERM,
            )
        except (ProcessLookupError, PermissionError):
            pass
        return
    if windows_job is not None:
        windows_job.close()
        if process.poll() is not None:
            return
        try:
            process.kill() if force else process.terminate()
        except OSError:
            pass
        return
    if process.poll() is not None:
        return
    try:
        process.kill() if force else process.terminate()
    except OSError:
        pass
def run_owned_command(
    command: Sequence[str],
    *,
    deadline_monotonic: float,
    cancel_event: Event | None = None,
    popen_factory: Callable = subprocess.Popen,
    _allow_test_executable: bool = False,
) -> dict:
    """Run one allowlisted no-child media process with an absolute deadline."""
    if (
        isinstance(deadline_monotonic, bool)
        or not isinstance(deadline_monotonic, (int, float))
        or not math.isfinite(deadline_monotonic)
    ):
        raise FfmpegProcessLoopbackSpikeError(
            "deadline_monotonic must be a finite number"
        )
    executable_name = Path(command[0]).name.lower() if command else ""
    allowed_basenames = {
        "ffmpeg",
        "ffmpeg.exe",
        "windows_process_loopback",
        "windows_process_loopback.exe",
    }
    if executable_name not in allowed_basenames and not _allow_test_executable:
        raise FfmpegProcessLoopbackSpikeError(
            f"BACKEND_UNAVAILABLE: executable basename {executable_name!r} "
            "is not accepted by the private spike filter"
        )
    popen_options = {
        "stdout": subprocess.PIPE,
        "stderr": subprocess.PIPE,
        "text": True,
        "shell": False,
    }
    if os.name == "nt":
        popen_options["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
    else:
        popen_options["start_new_session"] = True
    try:
        process = popen_factory(list(command), **popen_options)
    except (FileNotFoundError, PermissionError, OSError) as exc:
        raise FfmpegProcessLoopbackSpikeError(
            f"BACKEND_UNAVAILABLE: media process could not launch: {exc}"
        ) from exc

    windows_job = None
    try:
        if os.name == "nt":
            windows_job = _WindowsKillJob()
            try:
                windows_job.assign(process)
            except OSError as exc:
                windows_job.close()
                windows_job = None
                if process.poll() is None:
                    raise FfmpegProcessLoopbackSpikeError(
                        f"BACKEND_UNAVAILABLE: media process job assignment "
                        f"failed: {exc}"
                    ) from exc

        status = "completed"
        stdout = ""
        stderr = ""
        while True:
            if cancel_event is not None and cancel_event.is_set():
                status = "cancelled"
                break
            remaining = deadline_monotonic - time.monotonic()
            if remaining <= 0:
                status = "timeout"
                break
            try:
                stdout, stderr = process.communicate(
                    timeout=min(0.05, remaining)
                )
                break
            except subprocess.TimeoutExpired:
                continue

        if status != "completed":
            _terminate_owned_tree(
                process, force=False, windows_job=windows_job
            )
            windows_job = None
            try:
                stdout, stderr = process.communicate(timeout=2.0)
            except subprocess.TimeoutExpired:
                _terminate_owned_tree(process, force=True, windows_job=None)
                try:
                    stdout, stderr = process.communicate(timeout=2.0)
                except subprocess.TimeoutExpired:
                    if process.stdout is not None:
                        process.stdout.close()
                    if process.stderr is not None:
                        process.stderr.close()
                    try:
                        process.wait(timeout=1.0)
                    except subprocess.TimeoutExpired:
                        pass

        return {
            "status": status,
            "pid": process.pid,
            "returnCode": process.returncode,
            "stdout": stdout,
            "stderr": stderr,
            "aliveAfterCleanup": process.poll() is None,
        }
    except UnicodeDecodeError as exc:
        raise FfmpegProcessLoopbackSpikeError(
            "BACKEND_EXITED: media process returned invalid text"
        ) from exc
    except OSError as exc:
        raise FfmpegProcessLoopbackSpikeError(
            f"BACKEND_EXITED: media process communication failed: {exc}"
        ) from exc
    finally:
        try:
            _terminate_owned_tree(
                process, force=True, windows_job=windows_job
            )
        except Exception:
            if windows_job is not None:
                windows_job.close()
        try:
            process.communicate(timeout=2.0)
        except (OSError, subprocess.TimeoutExpired):
            if process.stdout is not None:
                process.stdout.close()
            if process.stderr is not None:
                process.stderr.close()
            try:
                process.wait(timeout=1.0)
            except (OSError, subprocess.TimeoutExpired):
                pass


def _positive_integer(value: int, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise FfmpegProcessLoopbackSpikeError(f"{name} must be a positive integer")
    return value


def _canvas_filter(width: int, height: int) -> str:
    return (
        f"scale={width}:{height}:force_original_aspect_ratio=decrease,"
        f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:black"
    )


def build_video_preflight_command(
    ffmpeg: Path,
    *,
    hwnd: int,
    output: Path,
    width: int,
    height: int,
) -> list[str]:
    _positive_integer(hwnd, "hwnd")
    _positive_integer(width, "width")
    _positive_integer(height, "height")
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-f",
        "gdigrab",
        "-draw_mouse",
        "0",
        "-i",
        f"hwnd=0x{hwnd:x}",
        "-frames:v",
        "1",
        "-vf",
        _canvas_filter(width, height),
        "-an",
        "-update",
        "1",
        str(output),
    ]


def build_video_command(
    ffmpeg: Path,
    *,
    hwnd: int,
    source_x: int,
    source_y: int,
    source_width: int,
    source_height: int,
    output: Path,
    duration_seconds: float,
    fps: int,
    width: int,
    height: int,
) -> list[str]:
    _positive_integer(hwnd, "hwnd")
    _positive_integer(source_width, "source_width")
    _positive_integer(source_height, "source_height")
    _positive_integer(fps, "fps")
    _positive_integer(width, "width")
    _positive_integer(height, "height")
    if duration_seconds <= 0:
        raise FfmpegProcessLoopbackSpikeError(
            "duration_seconds must be greater than zero"
        )

    video_filter = f"setpts=PTS-STARTPTS,fps={fps},{_canvas_filter(width, height)}"
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-f",
        "gdigrab",
        "-framerate",
        str(fps),
        "-draw_mouse",
        "0",
        "-i",
        f"hwnd=0x{hwnd:x}",
        "-t",
        f"{duration_seconds:.3f}",
        "-vf",
        video_filter,
        "-an",
        "-c:v",
        "h264_nvenc",
        "-preset",
        "p4",
        "-tune",
        "hq",
        "-rc",
        "constqp",
        "-qp",
        "18",
        "-pix_fmt",
        "yuv420p",
        "-f",
        "matroska",
        str(output),
    ]


def build_mux_command(
    ffmpeg: Path,
    *,
    video: Path,
    audio: Path,
    output: Path,
) -> list[str]:
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-i",
        str(video),
        "-i",
        str(audio),
        "-map",
        "0:v:0",
        "-map",
        "1:a:0",
        "-c:v",
        "copy",
        "-c:a",
        "flac",
        "-shortest",
        "-f",
        "matroska",
        str(output),
    ]


def inspect_master(
    ffprobe: Path,
    output: Path,
    *,
    runner: Callable = subprocess.run,
    expected_width: int | None = None,
    expected_height: int | None = None,
    expected_fps: int | None = None,
    expected_duration_seconds: float | None = None,
    minimum_frame_ratio: float = 0.90,
    duration_tolerance_seconds: float = 0.25,
    expected_video_codec: str = "h264",
    expected_audio_codec: str = "flac",
    timeout_seconds: float = 15.0,
) -> dict:
    if (
        isinstance(timeout_seconds, bool)
        or not isinstance(timeout_seconds, (int, float))
        or not math.isfinite(timeout_seconds)
        or timeout_seconds <= 0
    ):
        raise FfmpegProcessLoopbackSpikeError(
            "timeout_seconds must be a positive finite number"
        )
    command = [
        str(ffprobe),
        "-v",
        "error",
        "-count_frames",
        "-show_entries",
        "stream=index,codec_type,codec_name,width,height,avg_frame_rate,"
        "sample_rate,channels,nb_read_frames:format=duration,size",
        "-of",
        "json",
        str(output),
    ]
    try:
        completed = runner(
            command,
            check=False,
            capture_output=True,
            text=True,
            shell=False,
            timeout=timeout_seconds,
        )
    except subprocess.TimeoutExpired as exc:
        raise FfmpegProcessLoopbackSpikeError(
            "AV_VALIDATION_FAILED: ffprobe exceeded its deadline"
        ) from exc
    except UnicodeDecodeError as exc:
        raise FfmpegProcessLoopbackSpikeError(
            "AV_VALIDATION_FAILED: ffprobe returned invalid text"
        ) from exc
    except (FileNotFoundError, PermissionError, OSError) as exc:
        raise FfmpegProcessLoopbackSpikeError(
            f"BACKEND_UNAVAILABLE: ffprobe could not launch: {exc}"
        ) from exc
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "").strip()
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: ffprobe exited {completed.returncode}: {detail}"
        )
    try:
        probe = json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: invalid ffprobe JSON: {exc}"
        ) from exc

    streams = probe.get("streams", []) if isinstance(probe, dict) else []
    video_streams = [
        stream
        for stream in streams
        if isinstance(stream, dict) and stream.get("codec_type") == "video"
    ]
    audio_streams = [
        stream
        for stream in streams
        if isinstance(stream, dict) and stream.get("codec_type") == "audio"
    ]
    if len(video_streams) != 1 or len(audio_streams) != 1:
        raise FfmpegProcessLoopbackSpikeError(
            "AV_VALIDATION_FAILED: master must contain exactly one video "
            "and one audio stream"
        )

    video = video_streams[0]
    audio = audio_streams[0]
    try:
        duration = float(probe["format"]["duration"])
        size = int(probe["format"]["size"])
        width = int(video["width"])
        height = int(video["height"])
        sample_rate = int(audio["sample_rate"])
        channels = int(audio["channels"])
        read_frames = int(video["nb_read_frames"])
        average_frame_rate = Fraction(video["avg_frame_rate"])
    except (KeyError, TypeError, ValueError, ZeroDivisionError) as exc:
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: incomplete ffprobe metadata: {exc}"
        ) from exc
    if (
        not math.isfinite(duration)
        or duration <= 0
        or size <= 0
        or width <= 0
        or height <= 0
        or sample_rate != 48_000
        or channels != 2
        or read_frames <= 0
    ):
        raise FfmpegProcessLoopbackSpikeError(
            "AV_VALIDATION_FAILED: invalid duration, size, dimensions, "
            "or 48 kHz stereo audio contract"
        )
    if video.get("codec_name") != expected_video_codec:
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: video codec {video.get('codec_name')!r} "
            f"!= {expected_video_codec!r}"
        )
    if audio.get("codec_name") != expected_audio_codec:
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: audio codec {audio.get('codec_name')!r} "
            f"!= {expected_audio_codec!r}"
        )
    if expected_width is not None and width != expected_width:
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: video width {width} != {expected_width}"
        )
    if expected_height is not None and height != expected_height:
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: video height {height} != {expected_height}"
        )
    if expected_fps is not None and average_frame_rate != Fraction(expected_fps, 1):
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: average frame rate "
            f"{average_frame_rate} != {expected_fps}/1"
        )
    if (
        expected_duration_seconds is not None
        and abs(duration - expected_duration_seconds) > duration_tolerance_seconds
    ):
        raise FfmpegProcessLoopbackSpikeError(
            f"AV_VALIDATION_FAILED: duration {duration:.6f}s differs from "
            f"{expected_duration_seconds:.6f}s by more than "
            f"{duration_tolerance_seconds:.6f}s"
        )
    if expected_fps is not None and expected_duration_seconds is not None:
        minimum_frames = int(
            expected_fps * expected_duration_seconds * minimum_frame_ratio
        )
        if read_frames < minimum_frames:
            raise FfmpegProcessLoopbackSpikeError(
                f"AV_VALIDATION_FAILED: video decoded {read_frames} frames; "
                f"expected at least {minimum_frames}"
            )

    return {
        "status": "valid",
        "path": str(output),
        "durationSeconds": duration,
        "bytes": size,
        "video": {
            "codec": video.get("codec_name"),
            "width": width,
            "height": height,
            "averageFrameRate": str(average_frame_rate),
            "decodedFrames": read_frames,
        },
        "audio": {
            "codec": audio.get("codec_name"),
            "sampleRate": sample_rate,
            "channels": channels,
        },
    }
