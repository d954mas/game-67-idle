#!/usr/bin/env python3
"""One-command live and deterministic gameplay capture for a Studio game."""

from __future__ import annotations

import argparse
from contextlib import redirect_stdout
from datetime import datetime
import json
import math
from pathlib import Path
import sys
import time
from typing import Callable, Sequence
import uuid

try:
    from .devapi_client import running_game
    from .record_game import PRESETS, record_take, resolve_capture_settings
except ImportError:
    from devapi_client import running_game  # type: ignore[no-redef]
    from record_game import PRESETS, record_take, resolve_capture_settings  # type: ignore[no-redef]


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Launch a Studio game and record one validated gameplay MP4."
    )
    parser.add_argument("game", type=Path, nargs="?", default=Path.cwd())
    parser.add_argument("mode", nargs="?", choices=("live", "shot"), default="live")
    parser.add_argument("shot_id", nargs="?", help="catalog shot id")
    parser.add_argument("--seconds", type=float)
    parser.add_argument("--countdown", type=int, default=0)
    parser.add_argument("--preset", choices=sorted(PRESETS))
    parser.add_argument("--size", help="output size, for example 1920x1080")
    parser.add_argument("--fps", type=int, help="output FPS")
    parser.add_argument("--out", type=Path, help="take directory")
    return parser.parse_args(argv)


def resolve_game_executable(
    game_root: Path,
    relative_path: str = "build/devapi-debug/bin/game.exe",
) -> Path:
    executable = game_root.resolve() / relative_path
    if not executable.is_file():
        raise RuntimeError(
            "game executable is missing; build the debug game first: "
            f"{executable}"
        )
    return executable.resolve()


def resolve_studio_root(game_root: Path) -> Path:
    for candidate in (game_root.resolve(), *game_root.resolve().parents):
        if (candidate / "ai_studio" / "runtime_automation" / "capture_game.py").is_file():
            return candidate
    raise RuntimeError(f"Studio root not found above game directory: {game_root}")


def _positive_seconds(value: object, label: str) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool) or value <= 0:
        raise ValueError(f"{label} must be a positive number")
    return float(value)


def _positive_int(value: object, label: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        raise ValueError(f"{label} must be a positive integer")
    return value


def _output_frame_count(seconds: object, fps: int, label: str) -> int:
    duration = _positive_seconds(seconds, f"{label} seconds")
    frame_count = duration * fps
    rounded = round(frame_count)
    if not math.isclose(frame_count, rounded, abs_tol=1e-9):
        raise ValueError(f"{label} seconds * fps must be a whole number of output frames")
    return int(rounded)


def _validate_capture_fields(values: dict, label: str) -> None:
    preset = values.get("preset", "landscape")
    if not isinstance(preset, str) or preset not in PRESETS:
        raise ValueError(f"{label} preset must be one of {', '.join(sorted(PRESETS))}")
    size = values.get("size")
    if size is not None and not isinstance(size, str):
        raise ValueError(f"{label} size must be WIDTHxHEIGHT")
    fps = values.get("fps")
    if fps is not None and (not isinstance(fps, int) or isinstance(fps, bool)):
        raise ValueError(f"{label} fps must be an integer")
    resolve_capture_settings(preset, size, fps)
    if "ticks_per_frame" in values:
        _positive_int(values["ticks_per_frame"], f"{label} ticks_per_frame")


def _capture_options(defaults: dict, shot: dict | None, args: argparse.Namespace | None = None) -> tuple:
    values = dict(defaults)
    if shot is not None:
        for key in ("preset", "size", "fps", "ticks_per_frame"):
            if key in shot:
                values[key] = shot[key]
    if args is not None:
        for key in ("preset", "size", "fps"):
            value = getattr(args, key)
            if value is not None:
                values[key] = value
    _validate_capture_fields(values, "capture")
    settings = resolve_capture_settings(
        values.get("preset", "landscape"), values.get("size"), values.get("fps")
    )
    return settings, _positive_int(values.get("ticks_per_frame", 2), "capture ticks_per_frame")


def _validate_actions(actions: list, label: str, *, timed: bool) -> None:
    for index, action in enumerate(actions):
        action_label = f"{label} {index}"
        if not isinstance(action, dict):
            raise ValueError(f"{action_label} must be an object")
        method = action.get("method")
        if not isinstance(method, str) or not method:
            raise ValueError(f"{action_label} method must be a non-empty string")
        params = action.get("params")
        if params is not None and not isinstance(params, dict):
            raise ValueError(f"{action_label} params must be an object")
        if timed:
            frame = action.get("frame")
            if not isinstance(frame, int) or isinstance(frame, bool) or frame < 0:
                raise ValueError(f"{action_label} frame must be a non-negative integer")


def validate_catalog(catalog: object) -> dict:
    if not isinstance(catalog, dict):
        raise ValueError("capture catalog must be an object")
    if catalog.get("version") != 1:
        raise ValueError("capture catalog version must be 1")
    for key in ("defaults", "live", "shots"):
        if not isinstance(catalog.get(key, {}), dict):
            raise ValueError(f"catalog {key} must be an object")
    executable = catalog.get("executable")
    if executable is not None and (not isinstance(executable, str) or not executable):
        raise ValueError("catalog executable must be a non-empty string")

    defaults = catalog.get("defaults", {})
    _validate_capture_fields(defaults, "catalog defaults")
    live = catalog.get("live", {})
    if "seconds" in live:
        _positive_seconds(live["seconds"], "catalog live seconds")

    shots = catalog.get("shots", {})
    for shot_id, shot in shots.items():
        if not isinstance(shot_id, str) or not shot_id:
            raise ValueError("catalog shot ids must be non-empty strings")
        if not isinstance(shot, dict):
            raise ValueError(f"catalog shot {shot_id} must be an object")
        _positive_seconds(shot.get("seconds"), f"catalog shot {shot_id} seconds")
        if "warmup_ticks" in shot and (
            not isinstance(shot["warmup_ticks"], int)
            or isinstance(shot["warmup_ticks"], bool)
            or shot["warmup_ticks"] < 0
        ):
            raise ValueError(f"catalog shot {shot_id} warmup_ticks must be a non-negative integer")
        if "max_freeze_seconds" in shot:
            value = shot["max_freeze_seconds"]
            if not isinstance(value, (int, float)) or isinstance(value, bool) or value < 0:
                raise ValueError(f"catalog shot {shot_id} max_freeze_seconds must be non-negative")
        for key in ("setup", "events"):
            if key in shot and not isinstance(shot[key], list):
                raise ValueError(f"catalog shot {shot_id} {key} must be a list")
        _validate_actions(shot.get("setup", []), f"catalog shot {shot_id} setup", timed=False)
        _validate_actions(shot.get("events", []), f"catalog shot {shot_id} event", timed=True)
        settings, _ = _capture_options(defaults, shot)
        total_frames = _output_frame_count(shot["seconds"], settings.fps, f"catalog shot {shot_id}")
        for index, event in enumerate(shot.get("events", [])):
            if event["frame"] >= total_frames:
                raise ValueError(
                    f"catalog shot {shot_id} event {index} frame is outside output frames 0..{total_frames - 1}"
                )
    return catalog


def load_catalog(game_root: Path) -> dict:
    path = game_root.resolve() / "capture" / "catalog.json"
    try:
        catalog = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise RuntimeError(f"capture catalog is missing: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid capture catalog JSON: {exc.msg}") from exc
    return validate_catalog(catalog)


def available_shot_ids(catalog: dict) -> str:
    shot_ids = sorted(catalog["shots"])
    return ", ".join(shot_ids) if shot_ids else "(none)"


def resolve_shot(catalog: dict, shot_id: str | None) -> dict:
    if not shot_id:
        raise ValueError(f"shot id is required; available shots: {available_shot_ids(catalog)}")
    try:
        return catalog["shots"][shot_id]
    except KeyError as exc:
        raise ValueError(
            f"unknown shot: {shot_id}; available shots: {available_shot_ids(catalog)}"
        ) from exc


def prepare_shot(
    game,
    shot: dict,
    *,
    output_fps: int = 30,
    ticks_per_frame: int = 2,
) -> None:
    game.result("time.set_fps", {"fps": output_fps * ticks_per_frame})
    game.result("time.set_scale", {"scale": 1.0})
    game.result("time.set_mode", {"mode": "manual"})
    game.result("input.set_player_enabled", {"enabled": False})
    try:
        for action in shot.get("setup", []):
            game.result(action["method"], action.get("params") or {})
        warmup_ticks = int(shot.get("warmup_ticks", 0))
        if warmup_ticks:
            game.result("time.step", {"count": warmup_ticks})
    except BaseException:
        cleanup_shot(game)
        raise


def cleanup_shot(game) -> None:
    try:
        game.result("input.set_player_enabled", {"enabled": True})
    finally:
        game.result("time.set_mode", {"mode": "run"})


def play_timeline(
    game,
    shot: dict,
    *,
    output_fps: int,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict:
    total_frames = _output_frame_count(shot["seconds"], output_fps, "shot")
    events_by_frame: dict[int, list[dict]] = {}
    for event in shot.get("events", []):
        frame = int(event["frame"])
        if frame >= total_frames:
            raise ValueError(f"shot event frame is outside output frames 0..{total_frames - 1}")
        events_by_frame.setdefault(frame, []).append(event)

    # Manual ticks serialize one TCP round-trip each and cannot meet wall-clock video pacing.
    game.result("time.set_mode", {"mode": "run"})
    started = monotonic()
    event_count = 0
    for frame in range(total_frames):
        delay = started + frame / output_fps - monotonic()
        if delay > 0:
            sleep(delay)
        for event in events_by_frame.get(frame, []):
            game.result(event["method"], event.get("params") or {})
            event_count += 1
    duration = float(shot["seconds"])
    delay = started + duration - monotonic()
    if delay > 0:
        sleep(delay)
    elapsed = monotonic() - started
    if elapsed > duration + 1 / output_fps:
        raise RuntimeError(
            f"timeline exceeded output duration: {elapsed:.3f}s > {duration + 1 / output_fps:.3f}s"
        )
    return {"frames": total_frames, "events": event_count}

def default_output_root(game_root: Path) -> Path:
    stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    take_id = uuid.uuid4().hex[:8]
    return game_root.resolve() / "tmp" / "captures" / f"{stamp}-{take_id}"


def run(args: argparse.Namespace) -> dict:
    game_root = args.game.resolve()
    catalog = load_catalog(game_root)
    shot = resolve_shot(catalog, args.shot_id) if args.mode == "shot" else None
    settings, ticks_per_frame = _capture_options(catalog.get("defaults", {}), shot, args)
    if shot is not None:
        duration_seconds = _positive_seconds(shot["seconds"], "shot seconds")
        _output_frame_count(duration_seconds, settings.fps, "shot")
    else:
        live = catalog.get("live", {})
        duration_seconds = _positive_seconds(
            args.seconds if args.seconds is not None else live.get("seconds", 15),
            "live seconds",
        )
    executable = resolve_game_executable(
        game_root,
        catalog.get("executable", "build/devapi-debug/bin/game.exe"),
    )
    output_root = args.out.resolve() if args.out else default_output_root(game_root)

    with running_game(
        exe=str(executable),
        cwd=str(game_root),
        fresh_state=shot is not None,
        autosave_enabled=shot is None,
        window_size=f"{settings.width}x{settings.height}",
        extra_args=["--no-vsync"],
    ) as game:
        cleaned = False

        def finish_shot() -> None:
            nonlocal cleaned
            if shot is not None and not cleaned:
                cleanup_shot(game)
                cleaned = True

        recording_prepare = None
        recording_driver = None
        if shot is not None:
            def recording_prepare() -> None:
                nonlocal cleaned
                try:
                    prepare_shot(
                        game, shot, output_fps=settings.fps, ticks_per_frame=ticks_per_frame
                    )
                except BaseException:
                    cleaned = True
                    raise

            def recording_driver() -> dict:
                return play_timeline(
                    game,
                    shot,
                    output_fps=settings.fps,
                )

        try:
            kwargs = {
                "pid": game.process_id,
                "executable_name": executable.name,
                "output_root": output_root,
                "settings": settings,
                "duration_seconds": duration_seconds,
                "countdown": args.countdown,
                "recording_prepare": recording_prepare,
                "recording_driver": recording_driver,
                "hide_game_window": shot is not None,
            }
            if shot is not None:
                kwargs["max_freeze_seconds"] = float(shot.get("max_freeze_seconds", 2.0))
            with redirect_stdout(sys.stderr):
                return record_take(**kwargs)
        finally:
            finish_shot()


def main(argv: Sequence[str] | None = None) -> int:
    try:
        result = run(parse_args(argv))
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(result["edit"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())