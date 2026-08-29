#!/usr/bin/env python3
"""Record one game window and its own audio with an isolated OBS process."""

from __future__ import annotations

import argparse
import ctypes
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from concurrent.futures import ThreadPoolExecutor
from ctypes import wintypes
from dataclasses import dataclass
from datetime import datetime
from fractions import Fraction
from pathlib import Path
from typing import Callable, Sequence


RUNTIME_ROOT = Path(__file__).resolve().parent
REPO_ROOT = RUNTIME_ROOT.parents[1]
if str(RUNTIME_ROOT) not in sys.path:
    sys.path.insert(0, str(RUNTIME_ROOT))

from capture.backends.windows_process_loopback import (  # noqa: E402
    capture_process_audio,
    query_process_creation_time_100ns,
)
from pixel_health import analyze_png, assert_pixel_health  # noqa: E402


@dataclass(frozen=True)
class CaptureSettings:
    width: int
    height: int
    fps: int


@dataclass(frozen=True)
class CapturePaths:
    root: Path
    master: Path
    edit: Path
    metadata: Path

    @classmethod
    def from_root(cls, root: Path) -> "CapturePaths":
        return cls(
            root=root,
            master=root / "master.mkv",
            edit=root / "edit.mp4",
            metadata=root / "capture.json",
        )

    def staging(self) -> "CapturePaths":
        return CapturePaths(
            root=self.root,
            master=self.root / ".master.partial.mkv",
            edit=self.root / ".edit.partial.mp4",
            metadata=self.root / ".capture.partial.json",
        )


PRESETS = {
    "landscape": CaptureSettings(1920, 1080, 30),
    "social": CaptureSettings(1080, 1920, 30),
    "square": CaptureSettings(1080, 1080, 30),
}
OBS_DEFAULT = Path(r"C:\Program Files\obs-studio\bin\64bit\obs64.exe")


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
    command = [
        str(ffprobe), "-v", "error", "-count_packets", "-show_entries",
        "stream=codec_type,codec_name,width,height,avg_frame_rate,"
        "sample_rate,channels,nb_read_packets:format=duration,size",
        "-of", "json", str(output),
    ]
    try:
        completed = runner(
            command, check=False, capture_output=True, text=True, shell=False,
            timeout=timeout_seconds,
        )
    except (OSError, subprocess.TimeoutExpired, UnicodeDecodeError) as exc:
        raise RuntimeError(f"ffprobe failed: {exc}") from exc
    if completed.returncode:
        detail = (completed.stderr or completed.stdout or "").strip()
        raise RuntimeError(f"ffprobe exited {completed.returncode}: {detail}")
    try:
        probe = json.loads(completed.stdout)
        streams = probe["streams"]
        video_streams = [item for item in streams if item.get("codec_type") == "video"]
        audio_streams = [item for item in streams if item.get("codec_type") == "audio"]
        if len(video_streams) != 1 or len(audio_streams) != 1:
            raise RuntimeError("recording must contain one video and one audio stream")
        video, audio = video_streams[0], audio_streams[0]
        duration = float(probe["format"]["duration"])
        size = int(probe["format"]["size"])
        width, height = int(video["width"]), int(video["height"])
        fps = Fraction(video["avg_frame_rate"])
        frames = int(video["nb_read_packets"])
        sample_rate, channels = int(audio["sample_rate"]), int(audio["channels"])
    except RuntimeError:
        raise
    except (KeyError, TypeError, ValueError, ZeroDivisionError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"incomplete ffprobe metadata: {exc}") from exc

    if not math.isfinite(duration) or min(duration, size, width, height, frames) <= 0:
        raise RuntimeError("recording has invalid duration, size, dimensions, or frame count")
    if sample_rate != 48_000 or channels != 2:
        raise RuntimeError("recording audio must be 48 kHz stereo")
    if video.get("codec_name") != expected_video_codec:
        raise RuntimeError(f"unexpected video codec: {video.get('codec_name')!r}")
    if audio.get("codec_name") != expected_audio_codec:
        raise RuntimeError(f"unexpected audio codec: {audio.get('codec_name')!r}")
    if expected_width is not None and width != expected_width:
        raise RuntimeError(f"video width {width} != {expected_width}")
    if expected_height is not None and height != expected_height:
        raise RuntimeError(f"video height {height} != {expected_height}")
    if expected_fps is not None and fps != expected_fps:
        raise RuntimeError(f"average frame rate {fps} != {expected_fps}")
    if (
        expected_duration_seconds is not None
        and abs(duration - expected_duration_seconds) > duration_tolerance_seconds
    ):
        raise RuntimeError(
            f"duration {duration:.3f}s differs from {expected_duration_seconds:.3f}s"
        )
    if expected_fps is not None and expected_duration_seconds is not None:
        minimum_frames = int(expected_fps * expected_duration_seconds * minimum_frame_ratio)
        if frames < minimum_frames:
            raise RuntimeError(f"video decoded {frames} frames; expected at least {minimum_frames}")

    return {
        "status": "valid",
        "path": str(output),
        "durationSeconds": duration,
        "bytes": size,
        "video": {
            "codec": video["codec_name"],
            "width": width,
            "height": height,
            "averageFrameRate": str(fps),
            "decodedFrames": frames,
        },
        "audio": {
            "codec": audio["codec_name"],
            "sampleRate": sample_rate,
            "channels": channels,
        },
    }


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Record a game window and its own audio into master.mkv and edit.mp4."
    )
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--pid", type=int, help="PID of an already running game")
    source.add_argument("--exe", type=Path, help="game executable to launch")
    parser.add_argument(
        "--preset",
        choices=sorted(PRESETS),
        default="social",
        help="output frame; default: social (1080x1920)",
    )
    parser.add_argument("--size", help="override output size, for example 1920x1080")
    parser.add_argument("--fps", type=int, help="override output FPS")
    parser.add_argument(
        "--seconds",
        type=float,
        default=30,
        help="take duration after the two-second source warmup; default: 30",
    )
    parser.add_argument(
        "--countdown",
        type=int,
        default=3,
        help="seconds before OBS starts; default: 3",
    )
    parser.add_argument(
        "--out",
        type=Path,
        help="take directory; default: tmp/captures/<timestamp>",
    )
    parser.add_argument(
        "--obs",
        type=Path,
        help=r"override OBS executable; default: C:\Program Files\obs-studio\...",
    )
    parser.add_argument(
        "--keep-game",
        action="store_true",
        help="leave a game launched by this command running",
    )
    return parser.parse_args(argv)


def resolve_capture_settings(
    preset: str,
    size: str | None,
    fps: int | None,
) -> CaptureSettings:
    settings = PRESETS[preset]
    width = settings.width
    height = settings.height
    if size is not None:
        parts = size.lower().split("x")
        if len(parts) != 2 or not all(part.isdigit() for part in parts):
            raise ValueError("size must use WIDTHxHEIGHT, for example 1080x1920")
        width, height = (int(part) for part in parts)
    resolved_fps = settings.fps if fps is None else fps
    if width < 2 or height < 2 or width % 2 or height % 2:
        raise ValueError("capture width and height must be positive even numbers")
    if resolved_fps not in (24, 25, 30, 50, 60):
        raise ValueError("fps must be one of 24, 25, 30, 50, or 60")
    return CaptureSettings(width, height, resolved_fps)


def resolve_obs_capture_settings(settings: CaptureSettings) -> CaptureSettings:
    return CaptureSettings(settings.width, settings.height, settings.fps)


def build_master_command(
    ffmpeg: Path,
    source: Path,
    audio: Path,
    output: Path,
    *,
    start_seconds: float,
    duration_seconds: float,
    width: int,
    height: int,
    fps: int,
) -> list[str]:
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-i",
        str(source),
        "-i",
        str(audio),
        "-t",
        f"{duration_seconds:.3f}",
        "-map",
        "0:v:0",
        "-map",
        "1:a:0",
        "-vf",
        (
            f"trim=start={start_seconds:.3f},setpts=PTS-STARTPTS,"
            f"fps={fps},scale={width}:{height}:flags=lanczos"
        ),
        "-af",
        (
            f"apad=whole_dur={duration_seconds:.3f},"
            f"atrim=duration={duration_seconds:.3f},asetpts=PTS-STARTPTS"
        ),
        "-c:v",
        "h264_nvenc",
        "-preset",
        "p4",
        "-tune",
        "hq",
        "-rc",
        "vbr",
        "-cq",
        "18",
        "-b:v",
        "0",
        "-pix_fmt",
        "yuv420p",
        "-c:a",
        "aac",
        "-b:a",
        "192k",
        "-movflags",
        "+faststart",
        str(output),
    ]


def build_edit_command(
    ffmpeg: Path,
    master: Path,
    output: Path,
) -> list[str]:
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-i",
        str(master),
        "-map",
        "0:v:0",
        "-map",
        "0:a:0",
        "-c:v",
        "copy",
        "-c:a",
        "copy",
        "-movflags",
        "+faststart",
        str(output),
    ]


def build_freezedetect_command(ffmpeg: Path, video: Path) -> list[str]:
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "info",
        "-i",
        str(video),
        "-an",
        "-vf",
        "freezedetect=n=0.0001:d=0.100",
        "-f",
        "null",
        "-",
    ]


def build_obs_launch_command(obs_executable: Path) -> list[str]:
    return [
        str(obs_executable),
        "--portable",
        "--multi",
        "--disable-updater",
        "--disable-shutdown-check",
        "--startrecording",
    ]


def window_descriptor(
    *,
    title: str,
    class_name: str,
    executable_name: str,
) -> str:
    if not title or not class_name or not executable_name:
        raise ValueError("window title, class, and executable name are required")
    return f"{title}:{class_name}:{executable_name}"


def build_window_capture_settings(descriptor: str) -> dict:
    return {
        "window": descriptor,
        "priority": 0,
        "method": 2,
        "cursor": False,
        "client_area": True,
        "compatibility": False,
        "capture_audio": False,
    }


def _default_output_root() -> Path:
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    return REPO_ROOT / "tmp" / "captures" / timestamp


def _prepare_outputs(paths: CapturePaths) -> None:
    existing = [
        path
        for path in (paths.master, paths.edit, paths.metadata)
        if path.exists()
    ]
    if existing:
        raise RuntimeError(
            f"take directory already contains output files: {paths.root}; "
            "choose another --out"
        )
    paths.root.mkdir(parents=True, exist_ok=True)
    staging = paths.staging()
    for path in (staging.master, staging.edit, staging.metadata):
        path.unlink(missing_ok=True)


def _publish_take(paths: CapturePaths, staging: CapturePaths) -> None:
    published: list[Path] = []
    try:
        for source, target in (
            (staging.master, paths.master),
            (staging.edit, paths.edit),
            (staging.metadata, paths.metadata),
        ):
            os.replace(source, target)
            published.append(target)
    except OSError as exc:
        for target in reversed(published):
            try:
                target.unlink(missing_ok=True)
            except OSError:
                pass
        raise RuntimeError(f"could not publish completed take: {exc}") from exc


def _find_game_root(executable: Path) -> Path:
    for candidate in (executable.parent, *executable.parents):
        if (candidate / "game.json").is_file():
            return candidate
    return executable.parent


def build_launch_command(
    executable: Path,
    settings: CaptureSettings,
) -> list[str]:
    longest_side = max(settings.width, settings.height)
    scale = min(1.0, 1280 / longest_side)
    width = max(2, round(settings.width * scale / 2) * 2)
    height = max(2, round(settings.height * scale / 2) * 2)
    return [str(executable), "--window-size", f"{width}x{height}"]


def _launch_game(
    executable: Path,
    settings: CaptureSettings,
) -> subprocess.Popen:
    executable = executable.resolve()
    if not executable.is_file():
        raise RuntimeError(f"game executable does not exist: {executable}")
    return subprocess.Popen(
        build_launch_command(executable, settings),
        cwd=str(_find_game_root(executable)),
        shell=False,
    )


def _stop_process(process: subprocess.Popen, timeout_seconds: float = 5.0) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def _wait_for_window(pid: int, timeout_seconds: float = 20.0) -> int:
    from capture_window import find_window_for_pid

    deadline = time.monotonic() + timeout_seconds
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            return find_window_for_pid(pid)
        except RuntimeError as exc:
            last_error = exc
        time.sleep(0.1)
    raise RuntimeError(
        f"no visible game window appeared for PID {pid} within "
        f"{timeout_seconds:g} seconds"
    ) from last_error


def _wait_for_stable_window(
    pid: int,
    *,
    stable_seconds: float = 2.0,
    timeout_seconds: float = 20.0,
) -> int:
    from capture_window import find_window_for_pid

    deadline = time.monotonic() + timeout_seconds
    candidate: int | None = None
    candidate_since = time.monotonic()
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            current = find_window_for_pid(pid)
            last_error = None
            if current != candidate:
                candidate = current
                candidate_since = time.monotonic()
            elif time.monotonic() - candidate_since >= stable_seconds:
                return current
        except RuntimeError as exc:
            last_error = exc
            candidate = None
            candidate_since = time.monotonic()
        time.sleep(0.1)
    raise RuntimeError(
        f"game window did not remain stable for {stable_seconds:g} seconds"
    ) from last_error


def _window_title(hwnd: int) -> str:
    if os.name != "nt":
        raise RuntimeError("OBS game recording currently supports Windows only")
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
    user32.GetWindowTextLengthW.restype = ctypes.c_int
    user32.GetWindowTextW.argtypes = [
        wintypes.HWND,
        wintypes.LPWSTR,
        ctypes.c_int,
    ]
    user32.GetWindowTextW.restype = ctypes.c_int
    title_buffer = ctypes.create_unicode_buffer(
        max(512, user32.GetWindowTextLengthW(hwnd) + 1)
    )
    user32.GetWindowTextW(hwnd, title_buffer, len(title_buffer))
    if not title_buffer.value:
        raise RuntimeError("the game window has no title")
    return title_buffer.value


def _window_title_and_class(hwnd: int) -> tuple[str, str]:
    if os.name != "nt":
        raise RuntimeError("OBS game recording currently supports Windows only")
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.GetClassNameW.argtypes = [
        wintypes.HWND,
        wintypes.LPWSTR,
        ctypes.c_int,
    ]
    user32.GetClassNameW.restype = ctypes.c_int
    class_buffer = ctypes.create_unicode_buffer(256)
    if not user32.GetClassNameW(hwnd, class_buffer, len(class_buffer)):
        raise RuntimeError("could not resolve the game window class")
    return _window_title(hwnd), class_buffer.value


def _visible_top_level_windows() -> list[int]:
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    enum_proc = ctypes.WINFUNCTYPE(
        wintypes.BOOL,
        wintypes.HWND,
        wintypes.LPARAM,
    )
    user32.EnumWindows.argtypes = [enum_proc, wintypes.LPARAM]
    user32.EnumWindows.restype = wintypes.BOOL
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    user32.IsWindowVisible.restype = wintypes.BOOL
    windows: list[int] = []

    @enum_proc
    def callback(candidate: int, _param: int) -> bool:
        if user32.IsWindowVisible(candidate):
            windows.append(candidate)
        return True

    if not user32.EnumWindows(callback, 0):
        raise RuntimeError("could not enumerate visible windows")
    return windows


def _should_tag_window(
    hwnd: int,
    *,
    title: str,
    class_name: str,
    visible_windows: Callable[[], Sequence[int]] = _visible_top_level_windows,
    identity_for_window: Callable[[int], tuple[str, str]] = _window_title_and_class,
) -> bool:
    for candidate in visible_windows():
        if candidate == hwnd:
            continue
        try:
            if identity_for_window(candidate) == (title, class_name):
                return True
        except RuntimeError:
            continue
    return False


def _set_window_title(hwnd: int, title: str) -> None:
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    user32.SetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPCWSTR]
    user32.SetWindowTextW.restype = wintypes.BOOL
    if not user32.SetWindowTextW(hwnd, title):
        raise RuntimeError("could not set the game window title")


def _tag_window_title(
    hwnd: int,
    pid: int,
    *,
    read_title: Callable[[int], str] = _window_title,
    write_title: Callable[[int, str], None] = _set_window_title,
) -> str:
    original_title = read_title(hwnd)
    write_title(hwnd, f"{original_title} [capture-{pid}]")
    return original_title


def _window_identity(hwnd: int, executable_name: str) -> tuple[str, str, str]:
    title, class_name = _window_title_and_class(hwnd)
    return title, class_name, executable_name


def _resolve_obs_executable(override: Path | None) -> Path:
    candidates = []
    if override is not None:
        candidates.append(override)
    env_obs = os.environ.get("OBS_EXE")
    if env_obs:
        candidates.append(Path(env_obs))
    candidates.append(OBS_DEFAULT)
    path_obs = shutil.which("obs64")
    if path_obs:
        candidates.append(Path(path_obs))
    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if resolved.is_file():
            return resolved
    raise RuntimeError(
        "OBS Studio is required. Install it in the default location or pass --obs."
    )


class _AudioStartGate:
    def __init__(self) -> None:
        if os.name != "nt":
            raise RuntimeError("process audio capture currently supports Windows only")
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateEventW.argtypes = [
            wintypes.LPVOID,
            wintypes.BOOL,
            wintypes.BOOL,
            wintypes.LPCWSTR,
        ]
        kernel32.CreateEventW.restype = wintypes.HANDLE
        kernel32.SetEvent.argtypes = [wintypes.HANDLE]
        kernel32.SetEvent.restype = wintypes.BOOL
        kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel32.WaitForSingleObject.restype = wintypes.DWORD
        kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel32.CloseHandle.restype = wintypes.BOOL
        token = uuid.uuid4().hex
        self.ready_event_name = f"Local\\ai-studio-audio-ready-{token}"
        self.start_event_name = f"Local\\ai-studio-audio-start-{token}"
        self._kernel32 = kernel32
        self._ready = kernel32.CreateEventW(None, True, False, self.ready_event_name)
        self._start = kernel32.CreateEventW(None, True, False, self.start_event_name)
        if not self._ready or not self._start:
            self.close()
            raise RuntimeError("could not create process-audio start events")

    def wait_ready(self, timeout_seconds: float = 25.0) -> None:
        result = self._kernel32.WaitForSingleObject(
            self._ready,
            round(timeout_seconds * 1000),
        )
        if result != 0:
            raise RuntimeError("process-audio helper did not become ready")

    def start(self) -> None:
        if not self._kernel32.SetEvent(self._start):
            raise RuntimeError("could not start process-audio capture")

    def close(self) -> None:
        for handle_name in ("_ready", "_start"):
            handle = getattr(self, handle_name, None)
            if handle:
                self._kernel32.CloseHandle(handle)
                setattr(self, handle_name, None)


def _default_audio_helper() -> Path:
    return (
        REPO_ROOT
        / "tmp"
        / "capture"
        / "windows-process-loopback-build"
        / "Release"
        / "windows_process_loopback.exe"
    )


def _ensure_audio_helper() -> Path:
    helper = _default_audio_helper().resolve()
    source = RUNTIME_ROOT / "capture" / "native" / "windows_process_loopback"
    source_file = source / "windows_process_loopback.cpp"
    if helper.is_file() and source_file.is_file() and (
        helper.stat().st_mtime_ns >= source_file.stat().st_mtime_ns
    ):
        return helper
    cmake = shutil.which("cmake")
    build = REPO_ROOT / "tmp" / "capture" / "windows-process-loopback-build"
    if not cmake or not (source / "CMakeLists.txt").is_file():
        raise RuntimeError(
            "the Windows game-audio helper is missing and CMake is unavailable"
        )
    print("Building the game-audio helper (one-time setup)...", flush=True)
    for command in (
        [cmake, "-S", str(source), "-B", str(build)],
        [cmake, "--build", str(build), "--config", "Release"],
    ):
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            shell=False,
            timeout=120,
        )
        if completed.returncode != 0:
            detail = (completed.stderr or completed.stdout).strip()
            raise RuntimeError(f"could not build the game-audio helper: {detail}")
    if not helper.is_file():
        raise RuntimeError(f"audio-helper build did not create {helper}")
    return helper


def _obs_install_root(obs_executable: Path) -> Path:
    executable = obs_executable.resolve()
    try:
        root = executable.parents[2]
    except IndexError as exc:
        raise RuntimeError(f"unexpected OBS executable layout: {executable}") from exc
    if not (root / "data" / "obs-studio").is_dir():
        raise RuntimeError(f"OBS data directory is missing beside {executable}")
    return root


def _portable_session_root() -> Path:
    base = (REPO_ROOT / "tmp" / "obs-recorder-sessions").resolve()
    base.mkdir(parents=True, exist_ok=True)
    return Path(tempfile.mkdtemp(prefix="take-", dir=base))


def _mirror_obs_install(source_root: Path, session_root: Path) -> Path:
    target_root = session_root / "obs-studio"
    target_root.mkdir(parents=True)
    for source in source_root.rglob("*"):
        target = target_root / source.relative_to(source_root)
        if source.is_dir():
            target.mkdir(exist_ok=True)
            continue
        try:
            os.link(source, target)
        except OSError:
            shutil.copy2(source, target)
    (target_root / "portable_mode").touch()
    executable = target_root / "bin" / "64bit" / "obs64.exe"
    if not executable.is_file():
        raise RuntimeError("portable OBS mirror is incomplete")
    return executable


def _source_common(name: str, source_id: str, settings: dict, source_uuid: str) -> dict:
    return {
        "prev_ver": 503382018,
        "name": name,
        "uuid": source_uuid,
        "id": source_id,
        "versioned_id": source_id,
        "settings": settings,
        "mixers": 255,
        "sync": 0,
        "flags": 0,
        "volume": 1.0,
        "balance": 0.5,
        "enabled": True,
        "muted": False,
        "push-to-mute": False,
        "push-to-mute-delay": 0,
        "push-to-talk": False,
        "push-to-talk-delay": 0,
        "hotkeys": {},
        "deinterlace_mode": 0,
        "deinterlace_field_order": 0,
        "monitoring_type": 0,
        "private_settings": {},
    }


def _scene_collection(descriptor: str, settings: CaptureSettings) -> dict:
    game_uuid = str(uuid.uuid4())
    scene_uuid = str(uuid.uuid4())
    video_item = {
        "name": "Game",
        "source_uuid": game_uuid,
        "visible": True,
        "locked": True,
        "rot": 0.0,
        "pos": {"x": 0.0, "y": 0.0},
        "scale": {"x": 1.0, "y": 1.0},
        "align": 5,
        "bounds_type": 2,
        "bounds_align": 0,
        "bounds": {"x": float(settings.width), "y": float(settings.height)},
        "crop_left": 0,
        "crop_top": 0,
        "crop_right": 0,
        "crop_bottom": 0,
        "id": 1,
        "group_item_backup": False,
        "scale_filter": "bicubic",
        "blend_method": "default",
        "blend_type": "normal",
        "show_transition": {"duration": 0},
        "hide_transition": {"duration": 0},
        "private_settings": {},
    }
    scene = _source_common(
        "Scene",
        "scene",
        {
            "id_counter": 1,
            "custom_size": False,
            "items": [video_item],
        },
        scene_uuid,
    )
    scene["mixers"] = 0
    scene["hotkeys"] = {"OBSBasic.SelectScene": []}
    game = _source_common(
        "Game",
        "window_capture",
        build_window_capture_settings(descriptor),
        game_uuid,
    )
    return {
        "current_scene": "Scene",
        "current_program_scene": "Scene",
        "scene_order": [{"name": "Scene"}],
        "name": "Recorder",
        "sources": [scene, game],
        "groups": [],
        "quick_transitions": [],
        "transitions": [],
        "saved_projectors": [],
        "current_transition": "Fade",
        "transition_duration": 300,
        "preview_locked": True,
        "scaling_enabled": False,
        "scaling_level": 0,
        "scaling_off_x": 0.0,
        "scaling_off_y": 0.0,
        "virtual-camera": {"type2": 3},
        "modules": {
            "scripts-tool": [],
            "auto-scene-switcher": {
                "interval": 300,
                "non_matching_scene": "",
                "switch_if_not_matching": False,
                "active": False,
                "switches": [],
            },
        },
    }


def _write_obs_configuration(
    portable_root: Path,
    *,
    descriptor: str,
    settings: CaptureSettings,
    recording_directory: Path,
) -> None:
    config_root = portable_root / "config" / "obs-studio"
    profile_root = config_root / "basic" / "profiles" / "Recorder"
    scenes_root = config_root / "basic" / "scenes"
    profile_root.mkdir(parents=True, exist_ok=True)
    scenes_root.mkdir(parents=True, exist_ok=True)
    (config_root / "global.ini").write_text(
        """[General]
Pre19Defaults=false
Pre21Defaults=false
Pre23Defaults=false
Pre24.1Defaults=false
MaxLogs=4
InfoIncrement=-1
ProcessPriority=Normal
EnableAutoUpdates=false
ConfirmOnExit=false
HotkeyFocusType=NeverDisableHotkeys
BrowserHWAccel=false
YtDockCleanupDone=true
FirstRun=true
LastVersion=503382018

[Video]
Renderer=Direct3D 11

[BasicWindow]
PreviewEnabled=false
PreviewProgramMode=false
SnappingEnabled=false
SysTrayEnabled=false
SysTrayWhenStarted=false
SaveProjectors=false
ShowTransitions=false
ShowListboxToolbars=false
ShowStatusBar=false
ShowSourceIcons=false
ShowContextToolbars=false

[Audio]
DisableAudioDucking=true

[Basic]
Profile=Recorder
ProfileDir=Recorder
SceneCollection=Recorder
SceneCollectionFile=Recorder
ConfigOnNewProfile=false

[OBSWebSocket]
FirstLoad=false
ServerEnabled=false
AlertsEnabled=false
AuthRequired=true
""",
        encoding="utf-8",
    )
    recording_path = str(recording_directory.resolve()).replace("\\", "/")
    (profile_root / "basic.ini").write_text(
        f"""[General]
Name=Recorder

[Video]
BaseCX={settings.width}
BaseCY={settings.height}
OutputCX={settings.width}
OutputCY={settings.height}
FPSType=0
FPSCommon={settings.fps}
ScaleType=bicubic
ColorFormat=NV12
ColorSpace=709
ColorRange=Partial
SdrWhiteLevel=300
HdrNominalPeakLevel=1000

[Audio]
SampleRate=48000
ChannelSetup=Stereo
DesktopDevice1=Disabled
DesktopDevice2=Disabled
AuxDevice1=Disabled
AuxDevice2=Disabled
AuxDevice3=Disabled
AuxDevice4=Disabled

[Output]
Mode=Simple
FilenameFormatting=recording-%CCYY-%MM-%DD-%hh-%mm-%ss
OverwriteIfExists=false
DelayEnable=false
Reconnect=false
BindIP=default
IPFamily=IPv4+IPv6

[SimpleOutput]
FilePath={recording_path}
RecFormat2=mkv
RecQuality=Small
RecEncoder=nvenc
RecAudioEncoder=aac
RecTracks=1
VBitrate=12000
ABitrate=192
UseAdvanced=false
RecRB=false
""",
        encoding="utf-8",
    )
    (scenes_root / "Recorder.json").write_text(
        json.dumps(_scene_collection(descriptor, settings), ensure_ascii=False),
        encoding="utf-8",
    )


def _wait_for_finalized_recording(
    recording: Path,
    *,
    timeout_seconds: float = 2.0,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> None:
    deadline = monotonic() + timeout_seconds
    previous_size = -1
    while monotonic() <= deadline:
        try:
            size = recording.stat().st_size
        except OSError:
            size = 0
        if size > 0 and size == previous_size:
            return
        previous_size = size
        sleep(0.1)
    raise RuntimeError("OBS recording file was not finalized after shutdown")


def _wait_for_recording_file(
    recording_directory: Path,
    process: subprocess.Popen,
    timeout_seconds: float = 35.0,
) -> Path:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(
                f"OBS exited before recording started (code {process.returncode})"
            )
        candidates = sorted(
            recording_directory.glob("*.mkv"),
            key=lambda path: path.stat().st_mtime_ns,
        )
        if candidates and candidates[-1].stat().st_size > 0:
            return candidates[-1]
        time.sleep(0.1)
    raise RuntimeError("OBS did not start recording within 35 seconds")


def _request_obs_close(pid: int) -> int:
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    enum_proc = ctypes.WINFUNCTYPE(
        wintypes.BOOL,
        wintypes.HWND,
        wintypes.LPARAM,
    )
    user32.EnumWindows.argtypes = [enum_proc, wintypes.LPARAM]
    user32.EnumWindows.restype = wintypes.BOOL
    user32.GetWindowThreadProcessId.argtypes = [
        wintypes.HWND,
        ctypes.POINTER(wintypes.DWORD),
    ]
    user32.GetWindowThreadProcessId.restype = wintypes.DWORD
    user32.PostMessageW.argtypes = [
        wintypes.HWND,
        wintypes.UINT,
        wintypes.WPARAM,
        wintypes.LPARAM,
    ]
    user32.PostMessageW.restype = wintypes.BOOL
    posted = 0

    @enum_proc
    def callback(hwnd: int, _param: int) -> bool:
        nonlocal posted
        process_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(process_id))
        if process_id.value == pid and user32.PostMessageW(hwnd, 0x0010, 0, 0):
            posted += 1
        return True

    if not user32.EnumWindows(callback, 0):
        raise RuntimeError("could not enumerate OBS windows for shutdown")
    return posted


def _stop_obs(
    process: subprocess.Popen,
    timeout_seconds: float = 3.0,
    *,
    force: bool = False,
) -> int:
    if process.poll() is not None:
        return int(process.returncode)
    if _request_obs_close(process.pid) == 0:
        if force:
            _stop_process(process)
            return int(process.returncode)
        raise RuntimeError("OBS has no closable window for a safe recording stop")
    try:
        return process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired as exc:
        if force:
            _stop_process(process)
            return int(process.returncode)
        raise RuntimeError("OBS did not stop after a safe recording close") from exc


def _safe_remove_session(session_root: Path) -> None:
    base = (REPO_ROOT / "tmp" / "obs-recorder-sessions").resolve()
    resolved = session_root.resolve()
    if resolved.parent != base or not resolved.name.startswith("take-"):
        raise RuntimeError(f"refusing to remove unexpected OBS session: {resolved}")
    shutil.rmtree(resolved, ignore_errors=True)


def _require_tool(name: str) -> Path:
    resolved = shutil.which(name)
    if not resolved:
        raise RuntimeError(f"{name} is required and must be available in PATH")
    return Path(resolved)


def _run_media(command: list[str], timeout_seconds: float) -> subprocess.CompletedProcess:
    completed = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=timeout_seconds,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).strip()
        raise RuntimeError(f"media command failed: {detail}")
    return completed


def _parse_content_marker(value: str) -> float:
    try:
        marker = float(value.strip())
    except ValueError as exc:
        raise RuntimeError("could not measure the OBS media marker") from exc
    if not math.isfinite(marker) or marker < 0:
        raise RuntimeError("could not measure the OBS media marker")
    return marker


def _measure_content_marker(ffprobe: Path, recording: Path) -> float:
    completed = _run_media(
        [
            str(ffprobe),
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=nw=1:nk=1",
            str(recording),
        ],
        timeout_seconds=10.0,
    )
    return _parse_content_marker(completed.stdout)


def _parse_freezedetect_log(
    log: str,
    *,
    media_duration_seconds: float | None = None,
) -> dict:
    durations: list[float] = []
    active_start: float | None = None
    for event, value in re.findall(
        r"freeze_(start|duration):\s*([0-9]+(?:\.[0-9]+)?)",
        log,
    ):
        if event == "start":
            active_start = float(value)
        else:
            durations.append(float(value))
            active_start = None
    if active_start is not None and media_duration_seconds is not None:
        durations.append(max(0.0, media_duration_seconds - active_start))
    return {
        "freezeCount": len(durations),
        "maxFreezeSeconds": max(durations, default=0.0),
    }


def _assert_final_temporal_health(
    ffmpeg: Path,
    video: Path,
    max_freeze_seconds: float,
    *,
    media_duration_seconds: float | None = None,
) -> dict:
    completed = _run_media(
        build_freezedetect_command(ffmpeg, video),
        timeout_seconds=30.0,
    )
    health = _parse_freezedetect_log(
        f"{completed.stdout or ''}\n{completed.stderr or ''}",
        media_duration_seconds=media_duration_seconds,
    )
    if health["maxFreezeSeconds"] > max_freeze_seconds:
        raise RuntimeError(
            "final video contains a freeze of "
            f"{health['maxFreezeSeconds']:.3f}s, exceeding "
            f"{max_freeze_seconds:.3f}s"
        )
    return {"maxAllowedFreezeSeconds": max_freeze_seconds, **health}


def _content_start_offset_after_prepare(
    recording_detected_at: float,
    recording_prepare: Callable[[], None] | None,
    *,
    monotonic: Callable[[], float] = time.monotonic,
) -> float:
    if recording_prepare is not None:
        recording_prepare()
    return monotonic() - recording_detected_at


def _extract_frame(
    ffmpeg: Path,
    video: Path,
    output: Path,
    seek_seconds: float,
) -> None:
    _run_media(
        [
            str(ffmpeg),
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-ss",
            f"{seek_seconds:.3f}",
            "-i",
            str(video),
            "-frames:v",
            "1",
            str(output),
        ],
        timeout_seconds=20,
    )


def _extract_preflight_frame(
    ffmpeg: Path,
    video: Path,
    output: Path,
    seek_seconds: float,
) -> dict:
    _extract_frame(ffmpeg, video, output, seek_seconds)
    health = analyze_png(str(output))
    if health.luma_max < 8.0:
        raise RuntimeError("OBS source preflight produced a near-black frame")
    return {
        "uniqueColors": health.unique_colors,
        "uniqueBuckets": health.unique_buckets,
        "lumaMean": round(health.luma_mean, 3),
        "lumaRange": round(health.luma_range, 3),
        "lumaStdev": round(health.luma_stdev, 3),
    }


def _extract_health_frame(
    ffmpeg: Path,
    video: Path,
    output: Path,
    seek_seconds: float,
) -> dict:
    _extract_frame(ffmpeg, video, output, seek_seconds)
    health = assert_pixel_health(str(output), min_luma_stdev=8.0)
    return {
        "uniqueColors": health.unique_colors,
        "uniqueBuckets": health.unique_buckets,
        "lumaRange": round(health.luma_range, 3),
        "lumaStdev": round(health.luma_stdev, 3),
    }


def _preflight_obs_source(
    ffmpeg: Path,
    recording: Path,
    output: Path,
) -> dict:
    previous_seek = 0.0
    rejection: RuntimeError | None = None
    for seek_seconds in (1.0, 2.5, 4.0):
        time.sleep(seek_seconds - previous_seek)
        previous_seek = seek_seconds
        try:
            return _extract_preflight_frame(ffmpeg, recording, output, seek_seconds)
        except RuntimeError as exc:
            rejection = exc
    if rejection is None:
        raise RuntimeError("OBS source preflight did not run")
    raise rejection


def _show_window_for_obs(
    hwnd: int,
    bring_window_forward: Callable[[int], object],
) -> None:
    bring_window_forward(hwnd)


def _settle_game_capture(
    hwnd: int,
    *,
    hide_game_window: bool,
    bring_window_forward: Callable[[int], object],
    background_window: Callable[[int], None],
    sleep: Callable[[float], None] = time.sleep,
) -> None:
    if hide_game_window:
        background_window(hwnd)
    else:
        bring_window_forward(hwnd)
    sleep(0.25)


def _audio_levels(ffmpeg: Path, master: Path) -> dict:
    completed = subprocess.run(
        [
            str(ffmpeg),
            "-hide_banner",
            "-i",
            str(master),
            "-af",
            "volumedetect",
            "-f",
            "null",
            "NUL",
        ],
        check=False,
        capture_output=True,
        text=True,
        shell=False,
        timeout=30,
    )
    if completed.returncode != 0:
        raise RuntimeError("could not measure recorded audio")
    mean_match = re.search(r"mean_volume:\s*(-?[\d.]+) dB", completed.stderr)
    max_match = re.search(r"max_volume:\s*(-?[\d.]+) dB", completed.stderr)
    if not mean_match or not max_match:
        raise RuntimeError("recorded audio did not expose measurable levels")
    mean_db = float(mean_match.group(1))
    max_db = float(max_match.group(1))
    return {
        "meanVolumeDb": mean_db,
        "maxVolumeDb": max_db,
        "active": max_db > -90.0,
    }


def _obs_log_summary(portable_root: Path) -> dict:
    logs = sorted(
        (portable_root / "config" / "obs-studio" / "logs").glob("*.txt"),
        key=lambda path: path.stat().st_mtime_ns,
    )
    if not logs:
        return {"status": "missing"}
    text = logs[-1].read_text(encoding="utf-8-sig", errors="replace")
    dropped = re.findall(r"Number of lagged frames due to rendering lag/stalls: (\d+)", text)
    output = re.findall(r"Total frames output: (\d+)", text)
    return {
        "status": "available",
        "totalFramesOutput": int(output[-1]) if output else None,
        "renderLagFrames": int(dropped[-1]) if dropped else None,
        "nvenc": "NVENC" in text,
        "windowCapture": "window_capture" in text or "game.exe" in text,
    }


def _obs_wgc_service_failure(portable_root: Path) -> str | None:
    logs = sorted(
        (portable_root / "config" / "obs-studio" / "logs").glob("*.txt"),
        key=lambda path: path.stat().st_mtime_ns,
    )
    if not logs:
        return None
    text = logs[-1].read_text(encoding="utf-8-sig", errors="replace")
    if re.search(
        r"CreateForWindow\s+\(0x80070424\)",
        text,
        flags=re.IGNORECASE,
    ) is None:
        return None
    return (
        "OBS Windows Graphics Capture could not start "
        "(CreateForWindow 0x80070424 / ERROR_SERVICE_DOES_NOT_EXIST). "
        "The required per-user Windows capture service is unavailable. On a "
        "managed host, run capture through the approved interactive host "
        "execution path as the active console user. If it is already running "
        "there, inspect CaptureService availability instead of retrying OBS."
    )


def _stop_obs_and_detect_service_failure(
    process: subprocess.Popen,
    portable_root: Path,
) -> str | None:
    _stop_obs(process, force=True)
    return _obs_wgc_service_failure(portable_root)


def _record_audio_with_driver(
    capture_audio: Callable[[], dict],
    recording_driver: Callable[[], None] | None,
    *,
    wait_audio_ready: Callable[[], None] | None = None,
    before_driver: Callable[[], None] | None = None,
    start_audio: Callable[[], None] | None = None,
) -> dict:
    if (
        recording_driver is None
        and wait_audio_ready is None
        and before_driver is None
        and start_audio is None
    ):
        return capture_audio()
    with ThreadPoolExecutor(max_workers=2) as executor:
        audio_future = executor.submit(capture_audio)
        try:
            if wait_audio_ready is not None:
                wait_audio_ready()
            if before_driver is not None:
                before_driver()
            if start_audio is not None:
                start_audio()
            driver_future = (
                executor.submit(recording_driver)
                if recording_driver is not None
                else None
            )
            audio_result = audio_future.result()
            if driver_future is not None:
                driver_future.result()
            return audio_result
        except Exception:
            if start_audio is not None:
                start_audio()
            raise


def record_take(
    *,
    pid: int,
    executable_name: str,
    output_root: Path,
    settings: CaptureSettings,
    duration_seconds: float,
    countdown: int,
    obs_override: Path | None = None,
    recording_prepare: Callable[[], None] | None = None,
    recording_driver: Callable[[], None] | None = None,
    hide_game_window: bool = False,
    max_freeze_seconds: float | None = None,
) -> dict:
    if os.name != "nt":
        raise RuntimeError("OBS game recording currently supports Windows only")
    if pid <= 0:
        raise ValueError("pid must be positive")
    if duration_seconds <= 0:
        raise ValueError("seconds must be positive")
    if countdown < 0:
        raise ValueError("countdown cannot be negative")
    if max_freeze_seconds is not None and max_freeze_seconds < 0:
        raise ValueError("max_freeze_seconds cannot be negative")
    started_at = time.monotonic()

    from capture_window import (
        background_window,
        bring_window_forward,
        release_topmost,
        restore_window_interaction,
    )

    paths = CapturePaths.from_root(output_root.resolve())
    _prepare_outputs(paths)
    staging = paths.staging()
    ffmpeg = _require_tool("ffmpeg")
    ffprobe = _require_tool("ffprobe")
    obs = _resolve_obs_executable(obs_override)
    audio_helper = _ensure_audio_helper()
    obs_settings = resolve_obs_capture_settings(settings)
    hwnd = _wait_for_stable_window(pid)
    session_root = _portable_session_root()
    obs_process: subprocess.Popen | None = None
    recording_directory = paths.root / ".obs-recording"
    health_frame = paths.root / ".obs-health.png"
    audio_path = paths.root / ".game.partial.wav"
    tagged_window: int | None = None
    original_window_title: str | None = None
    audio_path.unlink(missing_ok=True)
    recording_directory.mkdir(exist_ok=False)
    try:
        title, class_name, executable_name = _window_identity(hwnd, executable_name)
        if _should_tag_window(hwnd, title=title, class_name=class_name):
            original_window_title = _tag_window_title(hwnd, pid)
            tagged_window = hwnd
            title, class_name, executable_name = _window_identity(hwnd, executable_name)
        descriptor = window_descriptor(
            title=title,
            class_name=class_name,
            executable_name=executable_name,
        )
        _show_window_for_obs(hwnd, bring_window_forward)
        portable_obs = _mirror_obs_install(_obs_install_root(obs), session_root)
        portable_root = portable_obs.parents[2]
        _write_obs_configuration(
            portable_root,
            descriptor=descriptor,
            settings=obs_settings,
            recording_directory=recording_directory,
        )
        for remaining in range(countdown, 0, -1):
            print(f"Recording in {remaining}...", flush=True)
            time.sleep(1)
        preflight_attempts = 0
        preflight_health: dict | None = None
        recorded: Path | None = None
        recording_detected_at = 0.0
        for attempt in range(1, 4):
            preflight_attempts = attempt
            print(
                f"Starting isolated OBS (attempt {attempt}/3)...",
                flush=True,
            )
            obs_process = subprocess.Popen(
                build_obs_launch_command(portable_obs),
                cwd=str(portable_obs.parent),
                shell=False,
            )
            try:
                recorded = _wait_for_recording_file(
                    recording_directory,
                    obs_process,
                )
            except RuntimeError as exc:
                service_failure = _stop_obs_and_detect_service_failure(
                    obs_process,
                    portable_root,
                )
                obs_process = None
                if service_failure is not None:
                    raise RuntimeError(service_failure) from exc
                raise
            recording_detected_at = time.monotonic()
            current_hwnd = _wait_for_stable_window(
                pid,
                stable_seconds=0.5,
                timeout_seconds=5.0,
            )
            rejection: str | None = None
            if current_hwnd != hwnd:
                if tagged_window is not None and original_window_title is not None:
                    _set_window_title(tagged_window, original_window_title)
                hwnd = current_hwnd
                title, class_name, executable_name = _window_identity(
                    hwnd,
                    executable_name,
                )
                if _should_tag_window(hwnd, title=title, class_name=class_name):
                    original_window_title = _tag_window_title(hwnd, pid)
                    tagged_window = hwnd
                else:
                    original_window_title = None
                    tagged_window = None
                rejection = "game window changed during OBS startup"
            else:
                _show_window_for_obs(hwnd, bring_window_forward)
                print(
                    "Checking live OBS pixels before REC...",
                    flush=True,
                )
                try:
                    preflight_health = _preflight_obs_source(
                        ffmpeg,
                        recorded,
                        health_frame,
                    )
                except RuntimeError as exc:
                    rejection = str(exc)
            if rejection is None:
                break
            service_failure = _stop_obs_and_detect_service_failure(
                obs_process,
                portable_root,
            )
            obs_process = None
            health_frame.unlink(missing_ok=True)
            for obsolete in recording_directory.glob("*.mkv"):
                obsolete.unlink(missing_ok=True)
            if service_failure is not None:
                raise RuntimeError(service_failure)
            if attempt == 3:
                raise RuntimeError(
                    f"OBS window source stayed unhealthy: {rejection}"
                )
            title, class_name, executable_name = _window_identity(
                hwnd,
                executable_name,
            )
            _write_obs_configuration(
                portable_root,
                descriptor=window_descriptor(
                    title=title,
                    class_name=class_name,
                    executable_name=executable_name,
                ),
                settings=obs_settings,
                recording_directory=recording_directory,
            )
            print(f"OBS source rejected ({rejection}); restarting...", flush=True)
        if recorded is None or preflight_health is None:
            raise RuntimeError("OBS source preflight did not complete")
        _settle_game_capture(
            hwnd,
            hide_game_window=hide_game_window,
            bring_window_forward=bring_window_forward,
            background_window=background_window,
        )
        audio_gate = _AudioStartGate()
        content_start_offset = 0.0
        capture_started_at = 0.0

        def begin_content() -> None:
            nonlocal content_start_offset, capture_started_at
            if recording_prepare is not None:
                recording_prepare()
            content_start_offset = _measure_content_marker(ffprobe, recorded)
            capture_started_at = time.monotonic()
            print(f"REC | {duration_seconds:g} seconds", flush=True)

        try:
            audio_capture = _record_audio_with_driver(
                lambda: capture_process_audio(
                    audio_helper,
                    pid=pid,
                    expected_creation_time_100ns=query_process_creation_time_100ns(pid),
                    output=audio_path,
                    duration_seconds=duration_seconds,
                    ready_event_name=audio_gate.ready_event_name,
                    start_event_name=audio_gate.start_event_name,
                ),
                recording_driver,
                wait_audio_ready=audio_gate.wait_ready,
                before_driver=begin_content,
                start_audio=audio_gate.start,
            )
        finally:
            audio_gate.close()
        obs_exit_code = _stop_obs(obs_process, timeout_seconds=2.0, force=True)
        obs_process = None
        _wait_for_finalized_recording(recorded)
        capture_finished_at = time.monotonic()
        export_started_at = capture_finished_at
        if not recorded.is_file() or recorded.stat().st_size <= 0:
            raise RuntimeError("OBS stopped without a usable recording file")
        minimum_master_duration = duration_seconds + content_start_offset
        source_inspection = inspect_master(
            ffprobe,
            recorded,
            expected_width=obs_settings.width,
            expected_height=obs_settings.height,
            expected_fps=obs_settings.fps,
            expected_audio_codec="aac",
            timeout_seconds=max(20, minimum_master_duration),
        )
        if source_inspection["durationSeconds"] < minimum_master_duration - 0.5:
            raise RuntimeError(
                "OBS recording ended before the requested content duration"
            )
        _run_media(
            build_master_command(
                ffmpeg,
                recorded,
                audio_path,
                staging.master,
                start_seconds=content_start_offset,
                duration_seconds=duration_seconds,
                width=settings.width,
                height=settings.height,
                fps=settings.fps,
            ),
            timeout_seconds=max(30, duration_seconds * 2),
        )
        recorded.unlink(missing_ok=True)
        audio_path.unlink(missing_ok=True)
        master = inspect_master(
            ffprobe,
            staging.master,
            expected_width=settings.width,
            expected_height=settings.height,
            expected_fps=settings.fps,
            expected_duration_seconds=duration_seconds,
            duration_tolerance_seconds=0.75,
            expected_audio_codec="aac",
            timeout_seconds=max(20, duration_seconds),
        )
        master["path"] = str(paths.master)
        _run_media(
            build_edit_command(ffmpeg, staging.master, staging.edit),
            timeout_seconds=max(20, duration_seconds),
        )
        edit = inspect_master(
            ffprobe,
            staging.edit,
            expected_width=settings.width,
            expected_height=settings.height,
            expected_duration_seconds=duration_seconds,
            duration_tolerance_seconds=0.75,
            expected_audio_codec="aac",
            timeout_seconds=max(20, duration_seconds),
        )
        edit["path"] = str(paths.edit)
        minimum_edit_frames = int(settings.fps * duration_seconds * 0.95)
        if edit["video"]["decodedFrames"] < minimum_edit_frames:
            raise RuntimeError(
                "editor MP4 contains fewer frames than the requested take"
            )
        edit["video"]["nominalFrameRate"] = f"{settings.fps}/1"
        temporal_health = None
        if max_freeze_seconds is not None:
            temporal_health = _assert_final_temporal_health(
                ffmpeg,
                staging.edit,
                max_freeze_seconds,
                media_duration_seconds=edit["durationSeconds"],
            )
        pixel_health = _extract_health_frame(
            ffmpeg,
            staging.edit,
            health_frame,
            min(duration_seconds / 2, 2.0),
        )
        audio_levels = _audio_levels(ffmpeg, staging.edit)
        completed_at = time.monotonic()
        result = {
            "status": "captured",
            "backend": "obs-window-capture-wgc",
            "pid": pid,
            "durationSeconds": duration_seconds,
            "sourceTrimSeconds": round(content_start_offset, 6),
            "sourceTrim": {
                "method": "ffprobe-format-duration",
                "seconds": round(content_start_offset, 6),
                "toleranceSeconds": round(1 / settings.fps, 6),
            },
            "width": settings.width,
            "height": settings.height,
            "fps": settings.fps,
            "syncQualification": (
                "host-aligned OBS video trim plus process-loopback audio start; "
                "content-marker offset unmeasured"
            ),
            "master": str(paths.master),
            "edit": str(paths.edit),
            "inspection": {
                "obsSource": {
                    "durationSeconds": source_inspection["durationSeconds"],
                    "bytes": source_inspection["bytes"],
                    "video": source_inspection["video"],
                    "audio": source_inspection["audio"],
                },
                "master": master,
                "edit": edit,
            },
            "pixelHealth": pixel_health,
            "preflightPixelHealth": preflight_health,
            "temporalHealth": temporal_health,
            "audio": audio_levels,
            "audioCapture": {
                "source": "windows-process-loopback",
                "sampleFrames": audio_capture["sampleFrames"],
            },
            "recordingDriver": recording_driver is not None,
            "recordingPrepare": recording_prepare is not None,
            "phaseTimingSeconds": {
                "startup": round(recording_detected_at - started_at, 3),
                "preflight": round(capture_started_at - recording_detected_at, 3),
                "capture": round(capture_finished_at - capture_started_at, 3),
                "export": round(completed_at - export_started_at, 3),
                "total": round(completed_at - started_at, 3),
            },
            "obs": {
                "exitCode": obs_exit_code,
                "isolatedPortableProfile": True,
                "previewEnabled": False,
                "sourceStartAttempts": preflight_attempts,
                **_obs_log_summary(portable_root),
            },
        }
        staging.metadata.write_text(
            json.dumps(result, indent=2) + "\n",
            encoding="utf-8",
        )
        _publish_take(paths, staging)
        return result
    finally:
        if obs_process is not None:
            try:
                _stop_obs(obs_process, force=True)
            except (OSError, RuntimeError):
                _stop_process(obs_process)
        if tagged_window is not None and original_window_title is not None:
            try:
                _set_window_title(tagged_window, original_window_title)
            except RuntimeError:
                pass
        restore_window_interaction(hwnd)
        release_topmost(hwnd)
        health_frame.unlink(missing_ok=True)
        audio_path.unlink(missing_ok=True)
        try:
            for leftover in recording_directory.iterdir():
                if leftover.is_file():
                    leftover.unlink(missing_ok=True)
            recording_directory.rmdir()
        except OSError:
            pass
        _safe_remove_session(session_root)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    launched: subprocess.Popen | None = None
    try:
        settings = resolve_capture_settings(args.preset, args.size, args.fps)
        if args.exe is not None:
            launched = _launch_game(args.exe, settings)
            pid = launched.pid
            executable_name = args.exe.name
        else:
            pid = args.pid
            executable_name = _process_executable_name(pid)
        result = record_take(
            pid=pid,
            executable_name=executable_name,
            output_root=args.out or _default_output_root(),
            settings=settings,
            duration_seconds=args.seconds,
            countdown=args.countdown,
            obs_override=args.obs,
        )
        print("Done.", flush=True)
        print(f"Master: {result['master']}", flush=True)
        print(f"Edit:   {result['edit']}", flush=True)
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        if launched is not None and not args.keep_game:
            _stop_process(launched)


def _process_executable_name(pid: int) -> str:
    if os.name != "nt":
        raise RuntimeError("process lookup currently supports Windows only")
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    process_query_limited_information = 0x1000
    kernel32.OpenProcess.argtypes = [
        wintypes.DWORD,
        wintypes.BOOL,
        wintypes.DWORD,
    ]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.QueryFullProcessImageNameW.argtypes = [
        wintypes.HANDLE,
        wintypes.DWORD,
        wintypes.LPWSTR,
        ctypes.POINTER(wintypes.DWORD),
    ]
    kernel32.QueryFullProcessImageNameW.restype = wintypes.BOOL
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    handle = kernel32.OpenProcess(process_query_limited_information, False, pid)
    if not handle:
        raise RuntimeError(f"could not open PID {pid}")
    try:
        buffer = ctypes.create_unicode_buffer(32768)
        size = wintypes.DWORD(len(buffer))
        if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            raise RuntimeError(f"could not resolve executable for PID {pid}")
        return Path(buffer.value).name
    finally:
        kernel32.CloseHandle(handle)


if __name__ == "__main__":
    raise SystemExit(main())
