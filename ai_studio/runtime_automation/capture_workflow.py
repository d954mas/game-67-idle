#!/usr/bin/env python3
"""Small agent-facing live/shot workflow over the existing OBS recorder."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Mapping

from capture_safe_area import UnsafeMask, evaluate_critical_regions
from capture_scenario import (
    Scenario,
    expand_schedule,
    parse_scenario,
    validate_against_describe,
)
from devapi_client import DevApiError, running_game
from record_game import (
    _extract_health_frame,
    _require_tool,
    record_take,
    resolve_capture_settings,
    resolve_obs_capture_settings,
)


CATALOG_SCHEMA = "ai_studio.game_capture_catalog"
CATALOG_VERSION = 1
READY_SAFE_AREA_STATUSES = frozenset({"official", "measured"})


class CaptureWorkflowError(ValueError):
    pass


@dataclass(frozen=True)
class ApprovedShot:
    id: str
    purpose: str
    duration_seconds: float
    angle: str
    preset: str
    scenario_path: Path
    scenario: Scenario
    critical_regions: tuple[dict[str, Any], ...]


@dataclass(frozen=True)
class CaptureCatalog:
    game_root: Path
    game: str
    executable: Path
    live: dict[str, Any]
    safe_area: dict[str, Any]
    shots: tuple[ApprovedShot, ...]

    def shot(self, shot_id: str) -> ApprovedShot:
        for shot in self.shots:
            if shot.id == shot_id:
                return shot
        available = ", ".join(shot.id for shot in self.shots)
        raise CaptureWorkflowError(
            f"unknown approved shot {shot_id!r}; available: {available}"
        )


def _object(value: Any, name: str, keys: set[str]) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise CaptureWorkflowError(f"{name} must be an object")
    unknown = sorted(set(value) - keys)
    if unknown:
        raise CaptureWorkflowError(f"{name} has unknown key: {unknown[0]}")
    return value


def _text(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise CaptureWorkflowError(f"{name} must be a non-empty string")
    return value


def _duration(value: Any, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value <= 0:
        raise CaptureWorkflowError(f"{name} must be a positive number")
    return float(value)


def _inside_game(game_root: Path, relative: Any, name: str) -> Path:
    text = _text(relative, name)
    path = (game_root / text).resolve()
    if not path.is_relative_to(game_root):
        raise CaptureWorkflowError(f"{name} must stay inside the game")
    return path


def load_catalog(game_root: Path) -> CaptureCatalog:
    game_root = game_root.resolve()
    path = game_root / "capture" / "catalog.json"
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CaptureWorkflowError(f"cannot load capture catalog: {exc}") from exc
    root = _object(
        document,
        "catalog",
        {
            "schema",
            "version",
            "game",
            "executable",
            "live",
            "safe_area",
            "shots",
        },
    )
    if root.get("schema") != CATALOG_SCHEMA or root.get("version") != CATALOG_VERSION:
        raise CaptureWorkflowError("unsupported capture catalog identity")
    game = _text(root.get("game"), "catalog.game")
    executable = _inside_game(game_root, root.get("executable"), "catalog.executable")
    live = _object(
        root.get("live"),
        "catalog.live",
        {"purpose", "duration_seconds", "angle", "preset"},
    )
    _text(live.get("purpose"), "catalog.live.purpose")
    _duration(live.get("duration_seconds"), "catalog.live.duration_seconds")
    _text(live.get("angle"), "catalog.live.angle")
    if live.get("preset") not in {"social", "landscape", "square"}:
        raise CaptureWorkflowError("catalog.live.preset is invalid")
    safe_area = _object(
        root.get("safe_area"),
        "catalog.safe_area",
        {
            "id",
            "policy_status",
            "normalized_dimensions",
            "guide_unsafe_rectangles",
        },
    )
    _text(safe_area.get("id"), "catalog.safe_area.id")
    if safe_area.get("policy_status") not in {
        "incomplete",
        "official",
        "measured",
    }:
        raise CaptureWorkflowError("catalog.safe_area.policy_status is invalid")
    if not isinstance(root.get("shots"), list) or not root["shots"]:
        raise CaptureWorkflowError("catalog.shots must be a non-empty array")

    shots: list[ApprovedShot] = []
    ids: set[str] = set()
    shot_keys = {
        "id",
        "purpose",
        "duration_seconds",
        "angle",
        "preset",
        "scenario",
        "critical_regions",
    }
    for index, value in enumerate(root["shots"]):
        item = _object(value, f"catalog.shots[{index}]", shot_keys)
        shot_id = _text(item.get("id"), f"catalog.shots[{index}].id")
        if shot_id in ids:
            raise CaptureWorkflowError(f"duplicate approved shot id: {shot_id}")
        ids.add(shot_id)
        preset = item.get("preset")
        if preset not in {"social", "landscape", "square"}:
            raise CaptureWorkflowError(f"shot {shot_id} has invalid preset")
        scenario_path = _inside_game(
            game_root, item.get("scenario"), f"shot {shot_id} scenario"
        )
        try:
            scenario = parse_scenario(
                json.loads(scenario_path.read_text(encoding="utf-8"))
            )
        except (OSError, json.JSONDecodeError, ValueError) as exc:
            raise CaptureWorkflowError(
                f"cannot load scenario for shot {shot_id}: {exc}"
            ) from exc
        if scenario.game != game:
            raise CaptureWorkflowError(f"shot {shot_id} scenario game mismatch")
        declared_duration = _duration(
            item.get("duration_seconds"), f"shot {shot_id} duration_seconds"
        )
        actual_duration = scenario.duration_frames / scenario.output_fps
        if abs(declared_duration - actual_duration) > 0.001:
            raise CaptureWorkflowError(
                f"shot {shot_id} duration does not match its scenario"
            )
        regions = item.get("critical_regions")
        if not isinstance(regions, list):
            raise CaptureWorkflowError(
                f"shot {shot_id} critical_regions must be an array"
            )
        shots.append(
            ApprovedShot(
                id=shot_id,
                purpose=_text(item.get("purpose"), f"shot {shot_id} purpose"),
                duration_seconds=declared_duration,
                angle=_text(item.get("angle"), f"shot {shot_id} angle"),
                preset=preset,
                scenario_path=scenario_path,
                scenario=scenario,
                critical_regions=tuple(dict(region) for region in regions),
            )
        )
    return CaptureCatalog(
        game_root=game_root,
        game=game,
        executable=executable,
        live=dict(live),
        safe_area=dict(safe_area),
        shots=tuple(shots),
    )


def evaluate_shot_safe_area(
    policy: Mapping[str, Any],
    critical_regions: list[Mapping[str, Any]] | tuple[Mapping[str, Any], ...],
    scenario: Scenario,
) -> dict[str, Any]:
    dimensions = policy.get("normalized_dimensions")
    if not isinstance(dimensions, Mapping):
        raise CaptureWorkflowError("safe-area dimensions are missing")
    width = dimensions.get("width")
    height = dimensions.get("height")
    rectangles = policy.get("guide_unsafe_rectangles")
    if (
        not isinstance(width, int)
        or not isinstance(height, int)
        or not isinstance(rectangles, list)
    ):
        raise CaptureWorkflowError("safe-area guide is invalid")
    mask = UnsafeMask.from_normalized_rectangles(width, height, rectangles)
    timed_regions = [
        {
            "id": _text(region.get("id"), "critical region id"),
            "start_tick": 0,
            "end_tick_exclusive": scenario.duration_frames,
            "rectangle": region.get("rectangle"),
        }
        for region in critical_regions
    ]
    geometry = evaluate_critical_regions(
        mask,
        timed_regions,
        measured_ticks=range(scenario.duration_frames),
    )
    policy_status = policy.get("policy_status")
    if geometry["status"] == "fail":
        status = "fail"
    elif (
        policy_status in READY_SAFE_AREA_STATUSES
        and geometry["status"] == "pass"
    ):
        status = "pass"
    else:
        status = "guidance_only"
    return {
        "policy": policy.get("id"),
        "policyStatus": policy_status,
        "geometryStatus": geometry["status"],
        "status": status,
        "masterEligible": status == "pass",
        "criticalRegions": geometry,
    }


def play_scenario_realtime(
    game: Any,
    scenario: Scenario,
    *,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    schedule = expand_schedule(scenario)
    started = monotonic()
    for frame in range(scenario.duration_frames):
        for event in schedule.get(frame, []):
            if "set" in event:
                setting = event["set"]
                game.result(
                    "game.capture_scene.set_parameter",
                    {
                        "scene": scenario.scene_id,
                        "parameter": setting["parameter"],
                        "value": setting["value"],
                    },
                )
            else:
                game.result(
                    "game.capture_scene.trigger_action",
                    {
                        "scene": scenario.scene_id,
                        "action": event["action"]["id"],
                        "arguments": {},
                    },
                )
        game.result("time.step", {"count": scenario.ticks_per_frame})
        remaining = started + ((frame + 1) / scenario.output_fps) - monotonic()
        if remaining > 0:
            sleep(remaining)
    final = game.result("game.capture_scene.status")
    return {
        "status": "completed",
        "frames": scenario.duration_frames,
        "ticksPerFrame": scenario.ticks_per_frame,
        "finalStatus": final,
    }


def prepare_scenario(game: Any, scenario: Scenario) -> dict[str, Any]:
    required = {
        f"game.capture_scene.{name}"
        for name in (
            "list",
            "describe",
            "load",
            "set_parameter",
            "trigger_action",
            "status",
        )
    }
    methods = game.endpoint_methods()
    missing = sorted(required - methods)
    if missing:
        raise CaptureWorkflowError(
            f"game capture-scene endpoint is missing: {missing[0]}"
        )
    for method in sorted(required):
        game.result("command.describe", {"method": method})
    listing = game.result("game.capture_scene.list")
    if (
        listing.get("apiVersion") != 1
        or listing.get("gameId") != scenario.game
    ):
        raise CaptureWorkflowError("game capture-scene identity mismatch")
    scene_ids = [
        item.get("id")
        for item in listing.get("scenes", [])
        if isinstance(item, Mapping)
    ]
    if scenario.scene_id not in scene_ids:
        raise CaptureWorkflowError(
            f"scenario scene is not registered: {scenario.scene_id}"
        )
    describe = game.result(
        "game.capture_scene.describe", {"scene": scenario.scene_id}
    )
    validate_against_describe(scenario, describe)
    game.result("time.set_mode", {"mode": "manual"})
    game.result(
        "game.capture_scene.load",
        {"scene": scenario.scene_id, "seed": scenario.seed},
    )
    if scenario.warmup_ticks:
        game.result("time.step", {"count": scenario.warmup_ticks})
    status = game.result("game.capture_scene.status")
    if status.get("ready") is not True or status.get("tick") != scenario.warmup_ticks:
        raise CaptureWorkflowError(
            f"capture scene failed warmup contract: {status}"
        )
    game.result("time.set_mode", {"mode": "run"})
    return status


def reset_scenario_for_recording(
    game: Any, scenario: Scenario
) -> dict[str, Any]:
    game.result("time.set_mode", {"mode": "manual"})
    game.result(
        "game.capture_scene.reset",
        {"scene": scenario.scene_id, "seed": scenario.seed},
    )
    if scenario.warmup_ticks:
        game.result("time.step", {"count": scenario.warmup_ticks})
    status = game.result("game.capture_scene.status")
    if status.get("ready") is not True or status.get("tick") != scenario.warmup_ticks:
        raise CaptureWorkflowError(
            f"capture scene failed REC reset contract: {status}"
        )
    return status


def publish_take(
    recorder_root: Path,
    take_root: Path,
    *,
    representative_frame: Path,
    workflow: Mapping[str, Any],
) -> dict[str, Any]:
    recorder_root = recorder_root.resolve()
    take_root = take_root.resolve()
    draft = take_root / "draft"
    master = take_root / "master"
    if draft.exists() or master.exists():
        raise CaptureWorkflowError(f"take output already exists: {take_root}")
    sources = {
        "recording.mkv": recorder_root / "master.mkv",
        "edit.mp4": recorder_root / "edit.mp4",
        "capture.json": recorder_root / "capture.json",
        "representative-frame.png": representative_frame.resolve(),
    }
    missing = [name for name, path in sources.items() if not path.is_file()]
    if missing:
        raise CaptureWorkflowError(
            f"recorder output is incomplete: {', '.join(missing)}"
        )
    recorder_metadata = json.loads(
        sources["capture.json"].read_text(encoding="utf-8")
    )
    eligible = (
        workflow.get("mode") == "shot"
        and workflow.get("scenarioStatus") == "completed"
        and isinstance(workflow.get("safeArea"), Mapping)
        and workflow["safeArea"].get("masterEligible") is True
        and workflow["safeArea"].get("status") == "pass"
    )
    classification = "master" if eligible else "draft"
    manifest = {
        **recorder_metadata,
        "workflow": dict(workflow),
        "classification": classification,
        "artifacts": {
            "draft": "draft",
            "master": "master" if eligible else None,
        },
    }
    draft.mkdir(parents=True)
    for name in ("recording.mkv", "edit.mp4", "representative-frame.png"):
        os.replace(sources[name], draft / name)
    (draft / "capture.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    sources["capture.json"].unlink(missing_ok=True)
    if eligible:
        shutil.copytree(draft, master)
    return {
        "classification": classification,
        "draft": str(draft),
        "master": str(master) if eligible else None,
        "manifest": manifest,
    }


def _take_root(game_root: Path, label: str) -> Path:
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    return (
        game_root
        / "tmp"
        / "captures"
        / label
        / f"{stamp}-{uuid.uuid4().hex[:8]}"
    )


def run_capture(
    game_root: Path,
    command: str,
    shot_id: str | None = None,
    *,
    live_driver: Callable[[Any], None] | None = None,
    countdown: int = 3,
) -> dict[str, Any]:
    catalog = load_catalog(game_root)
    if not catalog.executable.is_file():
        raise CaptureWorkflowError(
            f"build the DevAPI capture executable first: {catalog.executable}"
        )
    if command == "shot":
        if shot_id is None:
            raise CaptureWorkflowError("shot id is required")
        shot = catalog.shot(shot_id)
        preset = shot.preset
        duration = shot.duration_seconds
        fps = shot.scenario.output_fps
        label = shot.id
    elif command == "live":
        shot = None
        preset = catalog.live["preset"]
        duration = float(catalog.live["duration_seconds"])
        fps = None
        label = "live"
    else:
        raise CaptureWorkflowError(f"unsupported capture command: {command}")

    settings = resolve_capture_settings(preset, None, fps)
    window = resolve_obs_capture_settings(settings)
    take_root = _take_root(catalog.game_root, label)
    recorder_root = take_root / ".recorder"
    with running_game(
        exe=str(catalog.executable),
        cwd=str(catalog.game_root),
        fresh_state=shot is not None,
        autosave_enabled=shot is None,
        window_size=f"{window.width}x{window.height}",
    ) as game:
        if shot is not None:
            prepare_scenario(game, shot.scenario)
            def driver() -> None:
                reset_scenario_for_recording(game, shot.scenario)
                play_scenario_realtime(game, shot.scenario)
        else:
            driver = (lambda: live_driver(game)) if live_driver is not None else None
        recorder_result = record_take(
            pid=game.process_id,
            executable_name=catalog.executable.name,
            output_root=recorder_root,
            settings=settings,
            duration_seconds=duration,
            countdown=countdown,
            recording_driver=driver,
        )

    representative_frame = recorder_root / "representative-frame.png"
    _extract_health_frame(
        _require_tool("ffmpeg"),
        recorder_root / "master.mkv",
        representative_frame,
        min(duration / 2, 2.0),
    )
    if shot is None:
        safe_area = {
            "policy": catalog.safe_area.get("id"),
            "policyStatus": catalog.safe_area.get("policy_status"),
            "geometryStatus": "not_measured",
            "status": "guidance_only",
            "masterEligible": False,
        }
        scenario_status = "not_applicable"
        workflow_shot = None
    else:
        safe_area = evaluate_shot_safe_area(
            catalog.safe_area,
            shot.critical_regions,
            shot.scenario,
        )
        scenario_status = "completed"
        workflow_shot = {
            "id": shot.id,
            "purpose": shot.purpose,
            "durationSeconds": shot.duration_seconds,
            "angle": shot.angle,
            "scenario": str(shot.scenario_path.relative_to(catalog.game_root)),
            "scene": shot.scenario.scene_id,
            "seed": shot.scenario.seed,
        }
    published = publish_take(
        recorder_root,
        take_root,
        representative_frame=representative_frame,
        workflow={
            "schema": "ai_studio.capture_workflow_result",
            "version": 1,
            "game": catalog.game,
            "mode": command,
            "shot": workflow_shot,
            "scenarioStatus": scenario_status,
            "safeArea": safe_area,
            "recorderStatus": recorder_result.get("status"),
        },
    )
    try:
        recorder_root.rmdir()
    except OSError:
        pass
    return published


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="capture",
        description="Record live play or one approved deterministic game shot.",
    )
    parser.add_argument("--game-root", type=Path, required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("live", help="record normal play")
    shot = commands.add_parser("shot", help="record an approved deterministic shot")
    shot.add_argument("shot_id")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        result = run_capture(
            args.game_root,
            args.command,
            getattr(args, "shot_id", None),
        )
    except (CaptureWorkflowError, DevApiError, OSError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(f"Capture: {result['classification']}")
    print(f"Draft:   {result['draft']}")
    if result["master"]:
        print(f"Master:  {result['master']}")
    else:
        safe = result["manifest"]["workflow"]["safeArea"]
        print(
            "Master:  not promoted "
            f"(safe area: {safe['status']}, policy: {safe['policyStatus']})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
