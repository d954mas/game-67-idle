#!/usr/bin/env python3
"""Record one game window and its process audio for later editing.

This is the deliberately small, human-facing entry point.  The lower-level
capture probes remain useful for diagnostics, but a normal take needs only a
PID or an executable path.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import FIRST_EXCEPTION, ThreadPoolExecutor, wait
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Sequence


RUNTIME_ROOT = Path(__file__).resolve().parent
REPO_ROOT = RUNTIME_ROOT.parents[1]
if str(RUNTIME_ROOT) not in sys.path:
    sys.path.insert(0, str(RUNTIME_ROOT))

from capture.backends.ffmpeg_process_loopback_spike import (  # noqa: E402
    build_mux_command,
    inspect_master,
    run_owned_command,
)
from capture.backends.windows_process_loopback import (  # noqa: E402
    capture_process_audio,
    query_process_creation_time_100ns,
)


@dataclass(frozen=True)
class CaptureSettings:
    width: int
    height: int
    fps: int


@dataclass(frozen=True)
class CapturePaths:
    root: Path
    video: Path
    audio: Path
    master: Path
    edit: Path
    metadata: Path
    preflight: Path

    @classmethod
    def from_root(cls, root: Path) -> "CapturePaths":
        return cls(
            root=root,
            video=root / "video.mkv",
            audio=root / "game.wav",
            master=root / "master.mkv",
            edit=root / "edit.mp4",
            metadata=root / "capture.json",
            preflight=root / "preflight.png",
        )

    def staging(self) -> "CapturePaths":
        return CapturePaths(
            root=self.root,
            video=self.root / ".video.partial.mkv",
            audio=self.root / ".game.partial.wav",
            master=self.root / ".master.partial.mkv",
            edit=self.root / ".edit.partial.mp4",
            metadata=self.root / ".capture.partial.json",
            preflight=self.root / ".preflight.partial.png",
        )


PRESETS = {
    "landscape": CaptureSettings(1920, 1080, 60),
    "social": CaptureSettings(1080, 1920, 60),
    "square": CaptureSettings(1080, 1080, 60),
}


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Record a game window plus its own audio into master.mkv and edit.mp4."
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
        help="take duration; default: 30",
    )
    parser.add_argument(
        "--countdown",
        type=int,
        default=3,
        help="seconds before recording starts; default: 3",
    )
    parser.add_argument(
        "--out",
        type=Path,
        help="take directory; default: tmp/captures/<timestamp>",
    )
    parser.add_argument(
        "--helper",
        type=Path,
        help="override the Windows process-audio helper",
    )
    parser.add_argument(
        "--keep-parts",
        action="store_true",
        help="keep intermediate WAV, video, and preflight frame",
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
        if width <= 0 or height <= 0:
            raise ValueError("size must use positive WIDTHxHEIGHT values")
    resolved_fps = settings.fps if fps is None else fps
    if resolved_fps <= 0:
        raise ValueError("fps must be positive")
    return CaptureSettings(width, height, resolved_fps)


def _canvas_filter(width: int, height: int) -> str:
    return (
        f"scale={width}:{height}:force_original_aspect_ratio=decrease,"
        f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:black"
    )


def _ddagrab_source(
    *,
    x: int,
    y: int,
    source_width: int,
    source_height: int,
    fps: int,
) -> str:
    return (
        "ddagrab=output_idx=0:draw_mouse=false:"
        f"framerate={fps}:video_size={source_width}x{source_height}:"
        f"offset_x={x}:offset_y={y}"
    )


def build_ddagrab_preflight_command(
    ffmpeg: Path,
    *,
    x: int,
    y: int,
    source_width: int,
    source_height: int,
    output: Path,
    width: int,
    height: int,
) -> list[str]:
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-f",
        "lavfi",
        "-i",
        _ddagrab_source(
            x=x,
            y=y,
            source_width=source_width,
            source_height=source_height,
            fps=60,
        ),
        "-frames:v",
        "1",
        "-vf",
        f"hwdownload,format=bgra,{_canvas_filter(width, height)}",
        "-an",
        "-update",
        "1",
        str(output),
    ]


def build_ddagrab_video_command(
    ffmpeg: Path,
    *,
    x: int,
    y: int,
    source_width: int,
    source_height: int,
    output: Path,
    duration_seconds: float,
    fps: int,
    width: int,
    height: int,
) -> list[str]:
    video_filter = (
        f"hwdownload,format=bgra,setpts=PTS-STARTPTS,"
        f"fps={fps},{_canvas_filter(width, height)}"
    )
    return [
        str(ffmpeg),
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-f",
        "lavfi",
        "-i",
        _ddagrab_source(
            x=x,
            y=y,
            source_width=source_width,
            source_height=source_height,
            fps=fps,
        ),
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
        "aac",
        "-b:a",
        "192k",
        "-movflags",
        "+faststart",
        str(output),
    ]


def _default_output_root() -> Path:
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    return REPO_ROOT / "tmp" / "captures" / timestamp


def _default_helper() -> Path:
    return (
        REPO_ROOT
        / "tmp"
        / "capture"
        / "windows-process-loopback-build"
        / "Release"
        / "windows_process_loopback.exe"
    )


def _prepare_outputs(paths: CapturePaths) -> None:
    known_files = (
        paths.video,
        paths.audio,
        paths.master,
        paths.edit,
        paths.metadata,
        paths.preflight,
    )
    existing = [path for path in known_files if path.exists()]
    if existing:
        raise RuntimeError(
            f"take directory already contains output files: {paths.root}; "
            "choose another --out"
        )
    paths.root.mkdir(parents=True, exist_ok=True)
    staging = paths.staging()
    for path in (
        staging.video,
        staging.audio,
        staging.master,
        staging.edit,
        staging.metadata,
        staging.preflight,
    ):
        path.unlink(missing_ok=True)


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
    return [
        str(executable),
        "--window-size",
        f"{width}x{height}",
    ]


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


def _stop_launched_game(process: subprocess.Popen) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def _wait_for_window(pid: int, timeout_seconds: float = 15.0) -> int:
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


def _require_tool(name: str) -> Path:
    resolved = shutil.which(name)
    if not resolved:
        raise RuntimeError(f"{name} is required and must be available in PATH")
    return Path(resolved)


def _ensure_audio_helper(override: Path | None) -> Path:
    helper = (override or _default_helper()).resolve()
    if helper.is_file():
        return helper

    cmake = shutil.which("cmake")
    source = RUNTIME_ROOT / "capture" / "native" / "windows_process_loopback"
    build = REPO_ROOT / "tmp" / "capture" / "windows-process-loopback-build"
    if not cmake or not (source / "CMakeLists.txt").is_file():
        raise RuntimeError(
            f"game-audio helper is missing: {helper}. Install CMake or pass "
            "--helper with a built windows_process_loopback.exe"
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
            raise RuntimeError(f"could not build game-audio helper: {detail}")
    if not helper.is_file():
        raise RuntimeError(f"audio-helper build finished but did not create {helper}")
    return helper


def _run_media(
    command: list[str],
    deadline: float,
    cancel_event: threading.Event | None = None,
    start_barrier: threading.Barrier | None = None,
) -> dict:
    if start_barrier is not None:
        try:
            start_barrier.wait(timeout=5)
        except threading.BrokenBarrierError as exc:
            raise RuntimeError("recording start barrier failed") from exc
    entered_ns = time.perf_counter_ns()
    result = run_owned_command(
        command,
        deadline_monotonic=deadline,
        cancel_event=cancel_event,
    )
    if result["status"] != "completed":
        raise RuntimeError(f"media command {result['status']}")
    if result["returnCode"] != 0:
        detail = (result["stderr"] or result["stdout"]).strip()
        raise RuntimeError(f"media command failed: {detail}")
    return {"enteredNs": entered_ns, **result}


def _capture_audio(
    helper: Path,
    *,
    pid: int,
    creation_time_100ns: int,
    output: Path,
    duration_seconds: float,
    deadline: float,
    cancel_event: threading.Event,
    start_barrier: threading.Barrier,
) -> dict:
    entered_ns: int | None = None

    def owned_runner(command: list[str], **_: object) -> subprocess.CompletedProcess:
        nonlocal entered_ns
        try:
            start_barrier.wait(timeout=5)
        except threading.BrokenBarrierError as exc:
            raise RuntimeError("recording start barrier failed") from exc
        entered_ns = time.perf_counter_ns()
        result = run_owned_command(
            command,
            deadline_monotonic=deadline,
            cancel_event=cancel_event,
        )
        if result["status"] != "completed":
            raise subprocess.TimeoutExpired(command, duration_seconds)
        return subprocess.CompletedProcess(
            command,
            result["returnCode"],
            result["stdout"],
            result["stderr"],
        )

    result = capture_process_audio(
        helper,
        pid=pid,
        expected_creation_time_100ns=creation_time_100ns,
        output=output,
        duration_seconds=duration_seconds,
        runner=owned_runner,
    )
    if entered_ns is None:
        raise RuntimeError("audio helper did not reach the recording start barrier")
    return {"enteredNs": entered_ns, "result": result}


def validate_start_delta(
    audio_entered_ns: int,
    video_entered_ns: int,
    maximum_gap_ms: float = 20.0,
) -> float:
    delta_ms = abs(audio_entered_ns - video_entered_ns) / 1_000_000
    if delta_ms > maximum_gap_ms:
        raise RuntimeError(
            f"audio/video launcher start gap {delta_ms:.3f} ms exceeds "
            f"{maximum_gap_ms:.3f} ms"
        )
    return delta_ms


def _publish_take(
    paths: CapturePaths,
    staging: CapturePaths,
    *,
    keep_parts: bool,
) -> None:
    promotions = [
        (staging.master, paths.master),
        (staging.edit, paths.edit),
    ]
    if keep_parts:
        promotions.extend(
            (
                (staging.video, paths.video),
                (staging.audio, paths.audio),
                (staging.preflight, paths.preflight),
            )
        )
    promotions.append((staging.metadata, paths.metadata))
    published: list[Path] = []
    try:
        for source, target in promotions:
            os.replace(source, target)
            published.append(target)
    except OSError as exc:
        for target in reversed(published):
            try:
                target.unlink(missing_ok=True)
            except OSError:
                pass
        raise RuntimeError(f"could not publish completed take: {exc}") from exc


def _healthy_preflight(command: list[str], path: Path) -> bool:
    try:
        _run_media(command, time.monotonic() + 5)
        return path.is_file() and path.stat().st_size > 0
    except RuntimeError as exc:
        print(f"Capture source rejected: {exc}", file=sys.stderr, flush=True)
        return False


def _select_video_command(
    ffmpeg: Path,
    *,
    rect: object,
    paths: CapturePaths,
    settings: CaptureSettings,
    duration_seconds: float,
) -> tuple[str, list[str]]:
    source_width = rect.right - rect.left
    source_height = rect.bottom - rect.top
    dda_preflight = build_ddagrab_preflight_command(
        ffmpeg,
        x=rect.left,
        y=rect.top,
        source_width=source_width,
        source_height=source_height,
        output=paths.preflight,
        width=settings.width,
        height=settings.height,
    )
    if _healthy_preflight(dda_preflight, paths.preflight):
        return (
            "visible-region",
            build_ddagrab_video_command(
                ffmpeg,
                x=rect.left,
                y=rect.top,
                source_width=source_width,
                source_height=source_height,
                output=paths.video,
                duration_seconds=duration_seconds,
                fps=settings.fps,
                width=settings.width,
                height=settings.height,
            ),
        )
    raise RuntimeError(
        "Windows Desktop Duplication could not capture the game. Run the "
        "command from the unlocked local desktop and keep the game visible"
    )


def record_take(
    *,
    pid: int,
    output_root: Path,
    settings: CaptureSettings,
    duration_seconds: float,
    countdown: int,
    helper_override: Path | None = None,
    keep_parts: bool = False,
) -> dict:
    if os.name != "nt":
        raise RuntimeError("record_game currently supports Windows only")
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
    helper = _ensure_audio_helper(helper_override)
    hwnd = _wait_for_window(pid)
    rect = bring_window_forward(hwnd)
    try:
        source, video_command = _select_video_command(
            ffmpeg,
            rect=rect,
            paths=staging,
            settings=settings,
            duration_seconds=duration_seconds,
        )
        print(
            "Capture: visible game rectangle; keep it unobstructed.",
            flush=True,
        )
        for remaining in range(countdown, 0, -1):
            print(f"Recording in {remaining}...", flush=True)
            time.sleep(1)
        print(f"REC | {duration_seconds:g} seconds", flush=True)

        creation_time = query_process_creation_time_100ns(pid)
        deadline = time.monotonic() + duration_seconds + 25
        cancel_event = threading.Event()
        start_barrier = threading.Barrier(3)
        with ThreadPoolExecutor(max_workers=2) as executor:
            audio_future = executor.submit(
                _capture_audio,
                helper,
                pid=pid,
                creation_time_100ns=creation_time,
                output=staging.audio,
                duration_seconds=duration_seconds,
                deadline=deadline,
                cancel_event=cancel_event,
                start_barrier=start_barrier,
            )
            video_future = executor.submit(
                _run_media,
                video_command,
                deadline,
                cancel_event,
                start_barrier,
            )
            try:
                start_barrier.wait(timeout=5)
                done, pending = wait(
                    (audio_future, video_future),
                    timeout=max(0.1, deadline - time.monotonic()),
                    return_when=FIRST_EXCEPTION,
                )
                if pending:
                    cancel_event.set()
                    wait(pending, timeout=3)
                    for future in done:
                        future.result()
                    raise RuntimeError("recording exceeded its deadline")
                for future in done:
                    future.result()
            except BaseException:
                cancel_event.set()
                wait((audio_future, video_future), timeout=3)
                raise
            finally:
                cancel_event.set()
        audio_timed = audio_future.result()
        video_timed = video_future.result()
        start_delta_ms = validate_start_delta(
            audio_timed["enteredNs"],
            video_timed["enteredNs"],
        )
        audio = audio_timed["result"]

        _run_media(
            build_mux_command(
                ffmpeg,
                video=staging.video,
                audio=staging.audio,
                output=staging.master,
            ),
            time.monotonic() + max(20, duration_seconds),
        )
        master = inspect_master(
            ffprobe,
            staging.master,
            expected_width=settings.width,
            expected_height=settings.height,
            expected_fps=settings.fps,
            expected_duration_seconds=duration_seconds,
            duration_tolerance_seconds=0.75,
        )
        master["path"] = str(paths.master)
        _run_media(
            build_edit_command(ffmpeg, staging.master, staging.edit),
            time.monotonic() + max(20, duration_seconds),
        )
        result = {
            "status": "captured",
            "pid": pid,
            "source": source,
            "durationSeconds": duration_seconds,
            "width": settings.width,
            "height": settings.height,
            "fps": settings.fps,
            "launcherStartDeltaMs": start_delta_ms,
            "syncQualification": "shared-host-start; media-clock offset unmeasured",
            "master": str(paths.master),
            "edit": str(paths.edit),
            "audioFrames": audio["sampleFrames"],
            "inspection": master,
        }
        staging.metadata.write_text(
            json.dumps(result, indent=2) + "\n",
            encoding="utf-8",
        )
        _publish_take(paths, staging, keep_parts=keep_parts)
        if not keep_parts:
            for path in (staging.video, staging.audio, staging.preflight):
                try:
                    path.unlink(missing_ok=True)
                except OSError:
                    pass
        return result
    finally:
        release_topmost(hwnd)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    launched: subprocess.Popen | None = None
    try:
        settings = resolve_capture_settings(args.preset, args.size, args.fps)
        if args.exe is not None:
            launched = _launch_game(args.exe, settings)
            pid = launched.pid
        else:
            pid = args.pid
        result = record_take(
            pid=pid,
            output_root=args.out or _default_output_root(),
            settings=settings,
            duration_seconds=args.seconds,
            countdown=args.countdown,
            helper_override=args.helper,
            keep_parts=args.keep_parts,
        )
        print("Done.", flush=True)
        print(f"Master: {result['master']}", flush=True)
        print(f"Edit:   {result['edit']}", flush=True)
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        if launched is not None and not args.keep_game:
            _stop_launched_game(launched)


if __name__ == "__main__":
    raise SystemExit(main())
