#!/usr/bin/env python3
"""Native audiovisual capture of live play and catalog scenarios for a Studio game."""

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
    from .native_capture_preflight import (
        canonical_native_executable,
        native_capture_target,
        prepare_native_capture,
        validate_native_capture_final,
    )
    from .record_game import PRESETS, resolve_capture_settings
    from .native_record_media import validate_native_media
except ImportError:
    from devapi_client import running_game  # type: ignore[no-redef]
    from native_capture_preflight import (  # type: ignore[no-redef]
        canonical_native_executable,
        native_capture_target,
        prepare_native_capture,
        validate_native_capture_final,
    )
    from record_game import PRESETS, resolve_capture_settings  # type: ignore[no-redef]
    from native_record_media import validate_native_media


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
    parser.add_argument("--executable", type=Path,
                        help="override executable; relative paths use the selected game root")
    return parser.parse_args(argv)


def resolve_game_executable(
    game_root: Path,
    relative_path: str | None = None,
    *,
    override: Path | None = None,
) -> Path:
    requested = override if override is not None else (
        Path(relative_path) if relative_path is not None else canonical_native_executable(game_root)
    )
    _build_dir, executable, _target = native_capture_target(game_root, requested)
    return executable


def append_executable_identity(result: dict, native_build: dict[str, object]) -> dict:
    edit = Path(result["edit"])
    metadata_path = edit.parent / "capture.json"
    result["effectiveExecutable"] = native_build["executable"]
    result["nativeBuild"] = native_build
    if not metadata_path.is_file():
        return result
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    metadata["effectiveExecutable"] = native_build["executable"]
    metadata["nativeBuild"] = native_build
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return result


def resolve_studio_root(game_root: Path) -> Path:
    for candidate in (game_root.resolve(), *game_root.resolve().parents):
        if (candidate / "ai_studio" / "runtime_automation" / "capture_game.py").is_file():
            return candidate
    raise RuntimeError(f"Studio root not found above game directory: {game_root}")


def _positive_seconds(value: object, label: str) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value) or value <= 0:
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
    resolve_capture_settings(preset, size, fps, backend=values.get("backend", "window"))
    if "ticks_per_frame" in values:
        _positive_int(values["ticks_per_frame"], f"{label} ticks_per_frame")
    for key in ("expect_motion", "expect_audio"):
        if key in values and not isinstance(values[key], bool):
            raise ValueError(f"{label} {key} must be a boolean")
    if "audio_unlock" in values:
        _validate_actions([values["audio_unlock"]], f"{label} audio_unlock", timed=False)
    if "max_freeze_seconds" in values:
        freeze = values["max_freeze_seconds"]
        if not isinstance(freeze, (int, float)) or isinstance(freeze, bool) or not math.isfinite(freeze) or freeze < 0:
            raise ValueError(f"{label} max_freeze_seconds must be finite and non-negative")


def _capture_options(defaults: dict, shot: dict | None, args: argparse.Namespace | None = None) -> tuple:
    values = dict(defaults)
    if shot is not None:
        for key in ("preset", "size", "fps", "ticks_per_frame", "backend", "expect_motion", "expect_audio", "audio_unlock", "max_freeze_seconds"):
            if key in shot:
                values[key] = shot[key]
    if args is not None:
        for key in ("preset", "size", "fps"):
            value = getattr(args, key)
            if value is not None:
                values[key] = value
    _validate_capture_fields(values, "capture")
    settings = resolve_capture_settings(
        values.get("preset", "landscape"), values.get("size"), values.get("fps"),
        backend=values.get("backend", "window"),
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
    launch = catalog.get("launch", {})
    if not isinstance(launch, dict) or set(launch) - {"fresh_state", "autosave_enabled", "args"}:
        raise ValueError("catalog launch must contain only fresh_state, autosave_enabled, and args")
    for key in ("fresh_state", "autosave_enabled"):
        if key in launch and not isinstance(launch[key], bool):
            raise ValueError(f"catalog launch {key} must be a boolean")
    if "args" in launch and (not isinstance(launch["args"], list)
                             or any(not isinstance(arg, str) or not arg for arg in launch["args"])):
        raise ValueError("catalog launch args must be a list of non-empty strings")

    defaults = catalog.get("defaults", {})
    _validate_capture_fields(defaults, "catalog defaults")
    live = catalog.get("live", {})
    _validate_capture_fields(live, "catalog live")
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
        if "no_vsync" in shot and not isinstance(shot["no_vsync"], bool):
            raise ValueError(f"catalog shot {shot_id} no_vsync must be a boolean")
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
    sleep: Callable[[float], None] = time.sleep,
) -> None:
    game.result("time.set_fps", {"fps": output_fps * ticks_per_frame})
    game.result("time.set_scale", {"scale": 1.0})
    try:
        game.result("time.set_mode", {"mode": "run"})
        game.result("input.set_player_enabled", {"enabled": False})
        for action in shot.get("setup", []):
            if action["method"] == "time.step":
                count = _positive_int(
                    (action.get("params") or {}).get("count"),
                    "shot setup time.step count",
                )
                sleep(count / (output_fps * ticks_per_frame))
                continue
            if action["method"] == "time.set_mode":
                mode = (action.get("params") or {}).get("mode")
                if mode == "manual":
                    raise ValueError(
                        "shot setup time.set_mode manual is unsupported by realtime capture; "
                        "replace it with time.step or a host-paced wait"
                    )
            game.result(action["method"], action.get("params") or {})
        warmup_ticks = int(shot.get("warmup_ticks", 0))
        if warmup_ticks:
            sleep(warmup_ticks / (output_fps * ticks_per_frame))
    except BaseException as setup_error:
        try:
            cleanup_shot(game)
        except BaseException as cleanup_error:
            setup_error.add_note(f"shot cleanup failed: {cleanup_error!r}")
        raise


def cleanup_shot(game) -> None:
    input_error: BaseException | None = None
    try:
        game.result("input.set_player_enabled", {"enabled": True})
    except BaseException as exc:
        input_error = exc
    try:
        game.result("time.set_mode", {"mode": "run"})
    except BaseException as time_error:
        if input_error is None:
            raise
        input_error.add_note(f"shot cleanup failed: {time_error!r}")
    if input_error is not None:
        raise input_error


def _dispatch_timeline_event(game, event: dict, *, capture_width: int, capture_height: int) -> None:
    method = event["method"]
    params = event.get("params") or {}
    if method == "game.toy.shoot_target":
        state = game.result("game.toy.state")
        targets = state.get("targets") or []
        if targets and state.get("phase") == 0:
            target = max(targets, key=lambda item: item.get("y", 0.0))
            aim_y = max(target["y"] - target.get("height", 0.0) * 0.5 + 0.35, min(target["y"], 1.0))
            game.result("game.toy.shoot", {"x": target["x"], "y": aim_y, "z": target.get("z", 0.0)})
        return
    if method == "game.toy.special_target":
        shot_name = params["shot"]
        game.result("input.set_player_enabled", {"enabled": True})
        game.result("ui.click", {"id": f"toy/shot/{shot_name}", "button": "left"})
        state = game.result("game.toy.state")
        targets = state.get("targets") or []
        if targets and state.get("phase") == 0:
            tree = game.result("ui.tree")
            target = max(targets, key=lambda item: item.get("y", 0.0))
            game.result("ui.click", {"id": {"x": target["screenX"] * tree["width"] / capture_width,
                                             "y": target["screenY"] * tree["height"] / capture_height},
                                      "button": "left"})
        game.result("input.set_player_enabled", {"enabled": False})
        return
    game.result(method, params)


def _autoplay_tick(game, bot: dict, state: dict, controller: dict, *, capture_width: int, capture_height: int) -> bool:
    levels = bot["levels"]
    tick_frames = int(bot.get("tick_frames", 18))
    phase = state.get("phase")
    if phase not in (0, 1):
        retry_level = int(state.get("level") or levels[controller["level_index"]])
        game.result("game.toy.load", {"level": retry_level})
        controller["special_used"] = False
        controller["victory_frames"] = 0
        controller["last_level"] = retry_level
        return True
    if phase == 1:
        controller["victory_frames"] += tick_frames
        if controller["victory_frames"] < int(bot.get("victory_hold_frames", 30)):
            return False
        controller["level_index"] = (controller["level_index"] + 1) % len(levels)
        game.result("game.toy.load", {"level": levels[controller["level_index"]]})
        controller["victory_frames"] = 0
        controller["special_used"] = False
        return True
    if phase != 0:
        return False
    if state.get("balls", 0) <= 0:
        retry_level = int(state.get("level") or levels[controller["level_index"]])
        game.result("game.toy.load", {"level": retry_level})
        controller["special_used"] = False
        controller["victory_frames"] = 0
        controller["last_level"] = retry_level
        return True
    if not state.get("targets"):
        return False
    if controller.get("last_level") != state.get("level"):
        controller["last_level"] = state.get("level")
        controller["special_used"] = False
        controller["victory_frames"] = 0
    specials = bot.get("special_by_level", {})
    special_name = specials.get(str(state.get("level")))
    if special_name and not controller.get("special_used"):
        _dispatch_timeline_event(game, {"method": "game.toy.special_target", "params": {"shot": special_name}}, capture_width=capture_width, capture_height=capture_height)
        controller["special_used"] = True
        return True
    target = max(state["targets"], key=lambda item: sum(max(0.0, 1.6 - abs(item["x"] - other["x"])) for other in state["targets"] if other["y"] >= item["y"] - 0.3) / (1.0 + item["y"] * 0.35))
    aim_y = max(target["y"] - target.get("height", 0.0) * 0.5 + 0.35, min(target["y"], 1.0))
    result = game.result("game.toy.shoot", {"x": target["x"], "y": aim_y, "z": target.get("z", 0.0)})
    return bool(result.get("fired"))


def play_timeline(
    game,
    shot: dict,
    *,
    output_fps: int,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict:
    shot_width, shot_height = (int(part) for part in shot.get("size", "1920x1080").split("x", 1))
    total_frames = _output_frame_count(shot["seconds"], output_fps, "shot")
    autoplay = shot.get("autoplay")
    bot_controller = {"level_index": 0, "victory_frames": 0, "special_used": False, "last_level": None}
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
            _dispatch_timeline_event(game, event, capture_width=shot_width, capture_height=shot_height)
            event_count += 1
        if autoplay and frame % int(autoplay.get("tick_frames", 18)) == 0:
            state = game.result("game.toy.state")
            if _autoplay_tick(game, autoplay, state, bot_controller, capture_width=shot_width, capture_height=shot_height):
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


def unlock_audio(
    game,
    *,
    action: dict | None = None,
    preserve_disabled: bool = False,
) -> None:
    """Unlock runtime audio through an ordinary input before recording starts."""
    action = action or {"method": "input.key", "params": {"key": "W", "hold": 2}}
    if preserve_disabled:
        game.result("input.set_player_enabled", {"enabled": False})
        game.result(action["method"], action.get("params") or {})
        game.result("frame.wait", {"frames": 3})
    else:
        with game.player_gated():
            game.result(action["method"], action.get("params") or {})
            game.result("frame.wait", {"frames": 3})


def validate_native_record_receipt(
    receipt: object,
    *,
    expected_frames: int | None = None,
    frame_tolerance: int = 1,
    fps: int = 60,
) -> dict:
    """Validate the engine audiovisual recorder's cadence and audio receipt."""
    if not isinstance(receipt, dict):
        raise RuntimeError("game.record.stop returned a non-object receipt")
    required = ("frames", "audio_frames", "max_drift_ms", "tail_delta_ms", "valid")
    missing = [key for key in required if key not in receipt]
    if missing:
        raise RuntimeError("game.record.stop receipt is missing: " + ", ".join(missing))
    frames = receipt["frames"]
    audio_frames = receipt["audio_frames"]
    if not isinstance(frames, int) or isinstance(frames, bool) or frames <= 0:
        raise RuntimeError("game.record.stop receipt has an invalid video frame count")
    if not isinstance(audio_frames, int) or isinstance(audio_frames, bool) or audio_frames <= 0:
        raise RuntimeError("game.record.stop receipt has no audio frames")
    for key in ("max_drift_ms", "tail_delta_ms"):
        value = receipt[key]
        if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value):
            raise RuntimeError(f"game.record.stop receipt has an invalid {key}")
    if receipt["max_drift_ms"] < 0:
        raise RuntimeError("game.record.stop receipt has a negative max drift")
    if receipt["max_drift_ms"] > 100 or abs(receipt["tail_delta_ms"]) > 100 + 1000 / fps:
        raise RuntimeError("game.record.stop reports excessive audiovisual clock drift")
    tail = (audio_frames / 48000 - frames / fps) * 1000
    if abs(tail - receipt["tail_delta_ms"]) > .01:
        raise RuntimeError("game.record.stop tail difference disagrees with sample/frame counts")
    if receipt["valid"] is not True:
        raise RuntimeError("game.record.stop marked the audiovisual take invalid")
    if expected_frames is not None and abs(frames - expected_frames) > frame_tolerance:
        raise RuntimeError(
            f"game.record.stop returned {frames} video frames; expected {expected_frames}"
        )
    return receipt


def record_native_take(
    game,
    *,
    output_root: Path,
    fps: int,
    width: int,
    height: int,
    duration_seconds: float,
    recording_prepare: Callable[[], None] | None = None,
    recording_driver: Callable[[], dict] | None = None,
    audio_unlock_action: dict | None = None,
    expect_motion: bool = False,
    expect_audio: bool = False,
    max_freeze_seconds: float | None = None,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict:
    """Record realtime game frames through the engine's audiovisual DevAPI."""
    require_new_take(output_root)
    output_root.mkdir(parents=True, exist_ok=True)
    edit = (output_root / "edit.mp4").resolve()
    expected_frames = _output_frame_count(duration_seconds, fps, "capture")
    game.result("time.set_mode", {"mode": "run"})
    unlock_audio(
        game,
        action=audio_unlock_action,
        preserve_disabled=recording_prepare is not None,
    )
    if recording_prepare is not None:
        recording_prepare()
    view = game.result("view")
    if view.get("fb_width", 0) < 2 or view.get("fb_height", 0) < 2:
        raise RuntimeError("game framebuffer is empty; restore the game window before recording")
    if (view["fb_width"], view["fb_height"]) != (width, height):
        raise RuntimeError(f"game framebuffer {view['fb_width']}x{view['fb_height']} differs from requested {width}x{height}")
    game.result("game.record.start", {"name": str(edit), "fps": fps})
    driver_result: dict = {}
    driver_error: BaseException | None = None
    receipt: dict | None = None
    try:
        if recording_driver is not None:
            driver_result = recording_driver()
        else:
            started = monotonic()
            sleep(max(0.0, duration_seconds - (monotonic() - started)))
    except BaseException as exc:
        driver_error = exc
    finally:
        try:
            receipt = game.result("game.record.stop")
        except BaseException as stop_error:
            if driver_error is not None:
                driver_error.add_note(f"recorder stop failed: {stop_error!r}")
                raise driver_error from stop_error
            raise
    validated_receipt = validate_native_record_receipt(receipt, fps=fps)
    actual_duration_seconds = validated_receipt["frames"] / fps
    duration_margin = 0.1 + 1 / fps
    if abs(actual_duration_seconds - duration_seconds) > duration_margin:
        raise RuntimeError(
            f"game.record.stop duration {actual_duration_seconds:.3f}s is outside "
            f"requested {duration_seconds:.3f}s +/- {duration_margin:.3f}s"
        )
    if driver_error is not None:
        raise driver_error
    media = validate_native_media(edit, width=width, height=height, fps=fps,
                                  receipt=validated_receipt, expect_motion=expect_motion, expect_audio=expect_audio,
                                  max_freeze_seconds=max_freeze_seconds)
    metadata = {
        "schema": "ai_studio.runtime_automation.native_record.v1",
        "backend": "engine",
        "status": "draft",
        "edit": str(edit),
        "fps": fps,
        "durationSeconds": duration_seconds,
        "actualDurationSeconds": actual_duration_seconds,
        "expectedFrames": expected_frames,
        "receipt": validated_receipt,
        "driver": driver_result,
        "media": media,
    }
    (output_root / "capture.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )
    return {
        "edit": edit,
        "receipt": validated_receipt,
        "driver": driver_result,
        "durationSeconds": duration_seconds,
        "actualDurationSeconds": actual_duration_seconds,
        "media": media,
    }

def default_output_root(game_root: Path) -> Path:
    stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    take_id = uuid.uuid4().hex[:8]
    return game_root.resolve() / "tmp" / "captures" / f"{stamp}-{take_id}"


def require_new_take(root: Path) -> None:
    if any((root / name).exists() for name in ("edit.mp4", "capture.json")):
        raise RuntimeError("output already contains a recording; choose a new --out directory")


def resolve_output_root(game_root: Path, requested: Path | None) -> Path:
    if requested is None:
        return default_output_root(game_root)
    return (requested if requested.is_absolute() else game_root / requested).resolve()


def run(args: argparse.Namespace) -> dict:
    if args.countdown < 0:
        raise ValueError("countdown cannot be negative")
    game_root = args.game.resolve()
    catalog = load_catalog(game_root)
    shot = resolve_shot(catalog, args.shot_id) if args.mode == "shot" else None
    if shot is not None and args.seconds is not None:
        raise ValueError("catalog owns shot duration; remove --seconds")
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
    output_root = resolve_output_root(game_root, args.out)
    require_new_take(output_root)
    requested_executable = resolve_game_executable(
        game_root,
        catalog.get("executable", str(canonical_native_executable(game_root).relative_to(game_root))),
        override=args.executable,
    )
    native_build = prepare_native_capture(
        game_root,
        output_root,
        requested_executable=requested_executable,
    )
    executable = Path(str(native_build["executable"]["path"]))
    launch = catalog.get("launch", {})
    storage_root = output_root / ".game_storage"
    storage_root.mkdir(parents=True, exist_ok=True)

    with running_game(
        exe=str(executable),
        cwd=str(game_root),
        fresh_state=launch.get("fresh_state", shot is not None and not bool(shot.get("preserve_state", False))),
        autosave_enabled=launch.get("autosave_enabled", shot is None),
        window_size=f"{settings.width}x{settings.height}",
        extra_args=[
            *([] if shot is not None and shot.get("no_vsync") is False else ["--no-vsync"]),
            *launch.get("args", []),
        ],
        env_overrides={"GAME_STORAGE_ROOT": str(storage_root)},
    ) as game:
        game.result("endpoints")
        methods = {"game.record.start", "game.record.stop", "view", "input.key", "input.set_player_enabled",
                   "frame.wait", "time.set_mode", "time.set_fps", "time.set_scale"}
        policy = {**catalog.get("defaults", {}), **(shot if shot is not None else catalog.get("live", {}))}
        unlock = policy.get("audio_unlock")
        if unlock:
            methods.add(unlock["method"])
        if shot is not None:
            methods.update(action["method"] for action in shot.get("setup", []) + shot.get("events", []))
        for method in sorted(methods):
            game.result("command.describe", {"method": method})
        if args.countdown > 0:
            time.sleep(args.countdown)
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

        recording_error: BaseException | None = None
        try:
            with redirect_stdout(sys.stderr):
                result = record_native_take(
                    game=game,
                    output_root=output_root,
                    fps=settings.fps,
                    width=settings.width,
                    height=settings.height,
                    duration_seconds=duration_seconds,
                    recording_prepare=recording_prepare,
                    recording_driver=recording_driver,
                    audio_unlock_action=unlock,
                    expect_motion=policy.get("expect_motion", False),
                    expect_audio=policy.get("expect_audio", False),
                    max_freeze_seconds=policy.get("max_freeze_seconds"),
                )
            validate_native_capture_final(native_build)
            return append_executable_identity(result, native_build)
        except BaseException as exc:
            recording_error = exc
            raise
        finally:
            try:
                finish_shot()
            except BaseException as cleanup_error:
                if recording_error is None:
                    raise
                recording_error.add_note(f"shot cleanup failed: {cleanup_error!r}")


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
