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
from ctypes import wintypes
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Sequence


RUNTIME_ROOT = Path(__file__).resolve().parent
REPO_ROOT = RUNTIME_ROOT.parents[1]
if str(RUNTIME_ROOT) not in sys.path:
    sys.path.insert(0, str(RUNTIME_ROOT))

from capture.backends.ffmpeg_process_loopback_spike import (  # noqa: E402
    inspect_master,
)
from capture.backends.windows_process_loopback import (  # noqa: E402
    capture_process_audio,
    query_process_creation_time_100ns,
)
from pixel_health import assert_pixel_health  # noqa: E402


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
OBS_SOURCE_PREROLL_SECONDS = 2.0


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
    max_pixels = 720 * 1280
    scale = min(
        1.0,
        1280 / max(settings.width, settings.height),
        math.sqrt(max_pixels / (settings.width * settings.height)),
    )
    width = max(2, round(settings.width * scale / 2) * 2)
    height = max(2, round(settings.height * scale / 2) * 2)
    return CaptureSettings(width, height, settings.fps)


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
        "-ss",
        f"{start_seconds:.3f}",
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
        f"fps={fps},scale={width}:{height}:flags=lanczos",
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


def build_obs_launch_command(obs_executable: Path) -> list[str]:
    return [
        str(obs_executable),
        "--portable",
        "--multi",
        "--disable-updater",
        "--disable-shutdown-check",
        "--minimize-to-tray",
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
        "method": 1,
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


def _window_identity(hwnd: int, executable_name: str) -> tuple[str, str, str]:
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
    user32.GetClassNameW.argtypes = [
        wintypes.HWND,
        wintypes.LPWSTR,
        ctypes.c_int,
    ]
    user32.GetClassNameW.restype = ctypes.c_int

    title_buffer = ctypes.create_unicode_buffer(
        max(512, user32.GetWindowTextLengthW(hwnd) + 1)
    )
    class_buffer = ctypes.create_unicode_buffer(256)
    user32.GetWindowTextW(hwnd, title_buffer, len(title_buffer))
    if not user32.GetClassNameW(hwnd, class_buffer, len(class_buffer)):
        raise RuntimeError("could not resolve the game window class")
    title = title_buffer.value
    if not title:
        raise RuntimeError("the game window has no title")
    return title, class_buffer.value, executable_name


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
    if helper.is_file():
        return helper
    cmake = shutil.which("cmake")
    source = RUNTIME_ROOT / "capture" / "native" / "windows_process_loopback"
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
FirstRun=false

[Video]
Renderer=Direct3D 11

[BasicWindow]
PreviewEnabled=false
PreviewProgramMode=false
SnappingEnabled=false
SysTrayEnabled=true
SysTrayWhenStarted=true
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


def _stop_obs(process: subprocess.Popen, timeout_seconds: float = 20.0) -> int:
    if process.poll() is not None:
        return int(process.returncode)
    if _request_obs_close(process.pid) == 0:
        _stop_process(process)
        return int(process.returncode)
    try:
        return process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        _stop_process(process)
        return int(process.returncode)


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


def _extract_health_frame(
    ffmpeg: Path,
    master: Path,
    output: Path,
    seek_seconds: float,
) -> dict:
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
            str(master),
            "-frames:v",
            "1",
            str(output),
        ],
        timeout_seconds=20,
    )
    health = assert_pixel_health(str(output))
    return {
        "uniqueColors": health.unique_colors,
        "uniqueBuckets": health.unique_buckets,
        "lumaRange": round(health.luma_range, 3),
        "lumaStdev": round(health.luma_stdev, 3),
    }


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


def record_take(
    *,
    pid: int,
    executable_name: str,
    output_root: Path,
    settings: CaptureSettings,
    duration_seconds: float,
    countdown: int,
    obs_override: Path | None = None,
) -> dict:
    if os.name != "nt":
        raise RuntimeError("OBS game recording currently supports Windows only")
    if pid <= 0:
        raise ValueError("pid must be positive")
    if duration_seconds <= 0:
        raise ValueError("seconds must be positive")
    if countdown < 0:
        raise ValueError("countdown cannot be negative")

    from capture_window import bring_window_forward, release_topmost

    paths = CapturePaths.from_root(output_root.resolve())
    _prepare_outputs(paths)
    staging = paths.staging()
    ffmpeg = _require_tool("ffmpeg")
    ffprobe = _require_tool("ffprobe")
    obs = _resolve_obs_executable(obs_override)
    audio_helper = _ensure_audio_helper()
    obs_settings = resolve_obs_capture_settings(settings)
    hwnd = _wait_for_stable_window(pid)
    title, class_name, executable_name = _window_identity(hwnd, executable_name)
    descriptor = window_descriptor(
        title=title,
        class_name=class_name,
        executable_name=executable_name,
    )
    session_root = _portable_session_root()
    obs_process: subprocess.Popen | None = None
    recording_directory = paths.root / ".obs-recording"
    health_frame = paths.root / ".obs-health.png"
    audio_path = paths.root / ".game.partial.wav"
    audio_path.unlink(missing_ok=True)
    recording_directory.mkdir(exist_ok=False)
    bring_window_forward(hwnd)
    try:
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
            recorded = _wait_for_recording_file(
                recording_directory,
                obs_process,
            )
            recording_detected_at = time.monotonic()
            current_hwnd = _wait_for_stable_window(
                pid,
                stable_seconds=0.5,
                timeout_seconds=5.0,
            )
            rejection: str | None = None
            if current_hwnd != hwnd:
                hwnd = current_hwnd
                rejection = "game window changed during OBS startup"
            else:
                bring_window_forward(hwnd)
                print(
                    "Checking live OBS pixels before REC...",
                    flush=True,
                )
                time.sleep(OBS_SOURCE_PREROLL_SECONDS)
                try:
                    preflight_health = _extract_health_frame(
                        ffmpeg,
                        recorded,
                        health_frame,
                        seek_seconds=1.0,
                    )
                except RuntimeError as exc:
                    rejection = str(exc)
            if rejection is None:
                break
            _stop_obs(obs_process)
            obs_process = None
            health_frame.unlink(missing_ok=True)
            for obsolete in recording_directory.glob("*.mkv"):
                obsolete.unlink(missing_ok=True)
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
        content_start_offset = time.monotonic() - recording_detected_at
        print(
            f"REC | {duration_seconds:g} seconds",
            flush=True,
        )
        audio_capture = capture_process_audio(
            audio_helper,
            pid=pid,
            expected_creation_time_100ns=query_process_creation_time_100ns(pid),
            output=audio_path,
            duration_seconds=duration_seconds,
        )
        obs_exit_code = _stop_obs(obs_process)
        obs_process = None
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
        pixel_health = _extract_health_frame(
            ffmpeg,
            staging.edit,
            health_frame,
            min(duration_seconds / 2, 2.0),
        )
        audio_levels = _audio_levels(ffmpeg, staging.edit)
        result = {
            "status": "captured",
            "backend": "obs-window-capture-bitblt",
            "pid": pid,
            "durationSeconds": duration_seconds,
            "sourceTrimSeconds": round(content_start_offset, 3),
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
            "audio": audio_levels,
            "audioCapture": {
                "source": "windows-process-loopback",
                "sampleFrames": audio_capture["sampleFrames"],
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
                _stop_obs(obs_process)
            except (OSError, RuntimeError):
                _stop_process(obs_process)
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
