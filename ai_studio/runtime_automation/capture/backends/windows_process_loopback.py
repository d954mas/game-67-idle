"""Windows process-tree loopback helper contract.

The native executable is intentionally kept behind this small Python boundary:
the recorder owns validation and stable error families while the helper owns
only the Windows audio API interaction.
"""

from __future__ import annotations

import ctypes
import json
import math
import os
import subprocess
import wave
from ctypes import wintypes
from numbers import Real
from pathlib import Path
from typing import Callable
from uuid import uuid4


MAXIMUM_DURATION_MS = 21_600_000
_HELPER_STARTUP_MARGIN_SECONDS = 20.0


class ProcessLoopbackError(RuntimeError):
    """A stable recorder-facing process-loopback failure."""


class _FileTime(ctypes.Structure):
    _fields_ = [
        ("low", wintypes.DWORD),
        ("high", wintypes.DWORD),
    ]


def query_process_creation_time_100ns(pid: int) -> int:
    """Read the stable Windows creation-time identity of an owned process."""
    if isinstance(pid, bool) or not isinstance(pid, int) or pid <= 0:
        raise ProcessLoopbackError("pid must be a positive integer")
    if os.name != "nt":
        raise ProcessLoopbackError(
            "BACKEND_UNAVAILABLE: process identity requires Windows"
        )
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.GetProcessTimes.argtypes = [
        wintypes.HANDLE,
        ctypes.POINTER(_FileTime),
        ctypes.POINTER(_FileTime),
        ctypes.POINTER(_FileTime),
        ctypes.POINTER(_FileTime),
    ]
    kernel32.GetProcessTimes.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    process_query_limited_information = 0x1000
    handle = kernel32.OpenProcess(process_query_limited_information, False, pid)
    if not handle:
        raise ProcessLoopbackError(
            "PROCESS_IDENTITY_UNAVAILABLE: "
            + str(ctypes.WinError(ctypes.get_last_error()))
        )
    try:
        creation = _FileTime()
        exit_time = _FileTime()
        kernel_time = _FileTime()
        user_time = _FileTime()
        if not kernel32.GetProcessTimes(
            handle,
            ctypes.byref(creation),
            ctypes.byref(exit_time),
            ctypes.byref(kernel_time),
            ctypes.byref(user_time),
        ):
            raise ProcessLoopbackError(
                "PROCESS_IDENTITY_UNAVAILABLE: "
                + str(ctypes.WinError(ctypes.get_last_error()))
            )
        return (creation.high << 32) | creation.low
    finally:
        kernel32.CloseHandle(handle)


def _duration_milliseconds(duration_seconds: float) -> int:
    if (
        isinstance(duration_seconds, bool)
        or not isinstance(duration_seconds, Real)
        or not math.isfinite(duration_seconds)
    ):
        raise ProcessLoopbackError("duration must be a finite number")
    duration_ms = round(float(duration_seconds) * 1000)
    if duration_ms < 1:
        raise ProcessLoopbackError("duration must be at least one millisecond")
    if duration_ms > MAXIMUM_DURATION_MS:
        raise ProcessLoopbackError("duration cannot exceed six hours")
    return duration_ms


def build_capture_command(
    helper: Path,
    *,
    pid: int,
    expected_creation_time_100ns: int,
    output: Path,
    duration_seconds: float,
) -> list[str]:
    if isinstance(pid, bool) or not isinstance(pid, int) or pid <= 0:
        raise ProcessLoopbackError("pid must be a positive integer")
    if (
        isinstance(expected_creation_time_100ns, bool)
        or not isinstance(expected_creation_time_100ns, int)
        or expected_creation_time_100ns <= 0
    ):
        raise ProcessLoopbackError(
            "expected_creation_time_100ns must be a positive integer"
        )
    duration_ms = _duration_milliseconds(duration_seconds)

    return [
        str(helper),
        "--pid",
        str(pid),
        "--expected-creation-time-100ns",
        str(expected_creation_time_100ns),
        "--include-tree",
        "--output",
        str(output),
        "--duration-ms",
        str(duration_ms),
    ]


def _helper_report(stdout: str) -> dict:
    lines = [line for line in stdout.splitlines() if line.strip()]
    if not lines:
        return {}
    try:
        value = json.loads(lines[-1])
    except json.JSONDecodeError as exc:
        raise ProcessLoopbackError(
            f"BACKEND_EXITED: helper returned invalid JSON: {exc}"
        ) from exc
    if not isinstance(value, dict):
        raise ProcessLoopbackError("BACKEND_EXITED: helper report must be an object")
    return value


def _qualify_helper_report(
    report: dict,
    *,
    pid: int,
    expected_creation_time_100ns: int,
    duration_ms: int,
    wav: dict,
) -> None:
    expected_data_bytes = (
        wav["sampleFrames"] * wav["channels"] * wav["sampleWidthBytes"]
    )
    required = {
        "schema": "ai_studio.windows_process_loopback",
        "version": 2,
        "status": "ok",
        "pid": pid,
        "targetCreationTime100ns": expected_creation_time_100ns,
        "durationMs": duration_ms,
        "sampleRate": 48_000,
        "channels": 2,
        "bitsPerSample": 16,
        "dataBytes": expected_data_bytes,
        "discontinuities": 0,
        "timestampErrors": 0,
        "positionGaps": 0,
        "devicePositionRegressions": 0,
        "sampleFrames": wav["sampleFrames"],
    }
    mismatches = [
        f"{key}={report.get(key)!r}, expected {expected!r}"
        for key, expected in required.items()
        if report.get(key) != expected
    ]
    if mismatches:
        raise ProcessLoopbackError(
            "AUDIO_TRACK_UNQUALIFIED: " + "; ".join(mismatches)
        )
    drift_ppm = report.get("qpcDriftPpm")
    if (
        isinstance(drift_ppm, bool)
        or not isinstance(drift_ppm, Real)
        or not math.isfinite(drift_ppm)
    ):
        raise ProcessLoopbackError(
            "AUDIO_TRACK_UNQUALIFIED: qpcDriftPpm must be a finite number"
        )


def _inspect_wav(output: Path) -> dict:
    try:
        with wave.open(str(output), "rb") as wav:
            channels = wav.getnchannels()
            sample_rate = wav.getframerate()
            sample_frames = wav.getnframes()
            sample_width = wav.getsampwidth()
    except (OSError, EOFError, wave.Error) as exc:
        raise ProcessLoopbackError(
            f"AUDIO_TRACK_MISSING: helper did not create a valid WAV: {exc}"
        ) from exc

    if (
        channels != 2
        or sample_rate != 48_000
        or sample_frames <= 0
        or sample_width != 2
    ):
        raise ProcessLoopbackError(
            "AUDIO_TRACK_UNQUALIFIED: helper WAV must be non-empty "
            "48 kHz stereo PCM16"
        )
    expected_file_bytes = 44 + sample_frames * channels * sample_width
    try:
        actual_file_bytes = output.stat().st_size
    except OSError as exc:
        raise ProcessLoopbackError(
            f"AUDIO_TRACK_MISSING: helper WAV cannot be inspected: {exc}"
        ) from exc
    if actual_file_bytes != expected_file_bytes:
        raise ProcessLoopbackError(
            f"AUDIO_TRACK_UNQUALIFIED: WAV size {actual_file_bytes} != "
            f"canonical size {expected_file_bytes}"
        )
    return {
        "channels": channels,
        "sampleRate": sample_rate,
        "sampleFrames": sample_frames,
        "sampleWidthBytes": sample_width,
    }


def capture_process_audio(
    helper: Path,
    *,
    pid: int,
    expected_creation_time_100ns: int,
    output: Path,
    duration_seconds: float,
    runner: Callable = subprocess.run,
) -> dict:
    duration_ms = _duration_milliseconds(duration_seconds)
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
    except OSError as exc:
        raise ProcessLoopbackError(
            f"OUTPUT_WRITE_FAILED: could not prepare output directory: {exc}"
        ) from exc
    staging = output.with_name(f".{output.name}.{uuid4().hex}.partial")
    command = build_capture_command(
        helper,
        pid=pid,
        expected_creation_time_100ns=expected_creation_time_100ns,
        output=staging,
        duration_seconds=duration_ms / 1000,
    )
    try:
        try:
            completed = runner(
                command,
                check=False,
                capture_output=True,
                text=True,
                shell=False,
                timeout=duration_ms / 1000 + _HELPER_STARTUP_MARGIN_SECONDS,
            )
        except subprocess.TimeoutExpired as exc:
            raise ProcessLoopbackError(
                "BACKEND_TIMEOUT: process-loopback helper exceeded its deadline"
            ) from exc
        except UnicodeDecodeError as exc:
            raise ProcessLoopbackError(
                "BACKEND_EXITED: process-loopback helper returned invalid text"
            ) from exc
        except (FileNotFoundError, PermissionError, OSError) as exc:
            raise ProcessLoopbackError(
                f"BACKEND_UNAVAILABLE: process-loopback helper could not launch: {exc}"
            ) from exc

        if completed.returncode != 0:
            diagnostic = (completed.stderr or completed.stdout or "").strip()
            raise ProcessLoopbackError(
                f"BACKEND_EXITED: process-loopback helper exited "
                f"{completed.returncode}: {diagnostic}"
            )

        wav = _inspect_wav(staging)
        report = _helper_report(completed.stdout)
        _qualify_helper_report(
            report,
            pid=pid,
            expected_creation_time_100ns=expected_creation_time_100ns,
            duration_ms=duration_ms,
            wav=wav,
        )
        try:
            os.replace(staging, output)
        except OSError as exc:
            raise ProcessLoopbackError(
                f"OUTPUT_WRITE_FAILED: could not promote validated WAV: {exc}"
            ) from exc
    finally:
        try:
            staging.unlink(missing_ok=True)
        except OSError:
            pass

    return {
        "status": "captured",
        "output": str(output),
        **wav,
        "helperReport": report,
    }
