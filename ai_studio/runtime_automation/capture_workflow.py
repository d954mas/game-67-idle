#!/usr/bin/env python3
"""The two public game-video commands: capture live, capture shot <id>."""

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

from capture_scenario import Scenario, expand_schedule, parse_scenario, validate_against_describe
from devapi_client import DevApiError, running_game
from record_game import (
    _extract_health_frame,
    _require_tool,
    record_take,
    resolve_capture_settings,
    resolve_obs_capture_settings,
)

CATALOG_SCHEMA = "ai_studio.game_capture_catalog"
READY_SAFE_AREA_STATUSES = {"official", "measured"}


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
        match = next((shot for shot in self.shots if shot.id == shot_id), None)
        if match is not None:
            return match
        available = ", ".join(shot.id for shot in self.shots)
        raise CaptureWorkflowError(f"unknown shot {shot_id!r}; available: {available}")


def _inside_game(game_root: Path, relative: Any, label: str) -> Path:
    if not isinstance(relative, str) or not relative:
        raise CaptureWorkflowError(f"{label} must be a relative path")
    path = (game_root / relative).resolve()
    if not path.is_relative_to(game_root):
        raise CaptureWorkflowError(f"{label} must stay inside the game")
    return path


def load_catalog(game_root: Path) -> CaptureCatalog:
    game_root = game_root.resolve()
    try:
        data = json.loads((game_root / "capture" / "catalog.json").read_text(encoding="utf-8"))
        if data.get("schema") != CATALOG_SCHEMA or data.get("version") != 1:
            raise CaptureWorkflowError("unsupported capture catalog")
        game = data["game"]
        executable = _inside_game(game_root, data["executable"], "executable")
        live = data["live"]
        safe_area = data["safe_area"]
        if live["preset"] not in {"social", "landscape", "square"}:
            raise CaptureWorkflowError("invalid live preset")
        if safe_area["policy_status"] not in {"incomplete", *READY_SAFE_AREA_STATUSES}:
            raise CaptureWorkflowError("invalid safe-area status")

        shots = []
        seen = set()
        for item in data["shots"]:
            shot_id = item["id"]
            if shot_id in seen:
                raise CaptureWorkflowError(f"duplicate shot id: {shot_id}")
            seen.add(shot_id)
            if item["preset"] not in {"social", "landscape", "square"}:
                raise CaptureWorkflowError(f"invalid preset for shot {shot_id}")
            scenario_path = _inside_game(game_root, item["scenario"], f"shot {shot_id} scenario")
            scenario = parse_scenario(json.loads(scenario_path.read_text(encoding="utf-8")))
            duration = float(item["duration_seconds"])
            if scenario.game != game:
                raise CaptureWorkflowError(f"shot {shot_id} scenario game mismatch")
            if abs(duration - scenario.duration_frames / scenario.output_fps) > 0.001:
                raise CaptureWorkflowError(f"shot {shot_id} duration does not match its scenario")
            shots.append(ApprovedShot(
                id=shot_id,
                purpose=item["purpose"],
                duration_seconds=duration,
                angle=item["angle"],
                preset=item["preset"],
                scenario_path=scenario_path,
                scenario=scenario,
                critical_regions=tuple(item.get("critical_regions", [])),
            ))
        if not shots:
            raise CaptureWorkflowError("capture catalog has no shots")
        if live["scene_shot"] not in seen:
            raise CaptureWorkflowError("live scene_shot must reference a shot")
    except CaptureWorkflowError:
        raise
    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError) as exc:
        raise CaptureWorkflowError(f"invalid capture catalog: {exc}") from exc
    return CaptureCatalog(game_root, game, executable, dict(live), dict(safe_area), tuple(shots))


def evaluate_shot_safe_area(
    policy: Mapping[str, Any],
    critical_regions: list[Mapping[str, Any]] | tuple[Mapping[str, Any], ...],
) -> dict[str, Any]:
    unsafe = policy.get("guide_unsafe_rectangles")
    if not isinstance(unsafe, list):
        raise CaptureWorkflowError("safe-area guide is invalid")
    violations = []
    for region in critical_regions:
        rectangle = region.get("rectangle")
        if not isinstance(rectangle, list) or len(rectangle) != 4:
            raise CaptureWorkflowError("critical region is invalid")
        if any(
            rectangle[0] < blocked[2]
            and rectangle[2] > blocked[0]
            and rectangle[1] < blocked[3]
            and rectangle[3] > blocked[1]
            for blocked in unsafe
        ):
            violations.append(region.get("id", "unnamed"))
    geometry = "fail" if violations else "pass"
    policy_status = policy.get("policy_status")
    status = "fail" if violations else (
        "pass" if policy_status in READY_SAFE_AREA_STATUSES else "guidance_only"
    )
    return {
        "policy": policy.get("id"),
        "policyStatus": policy_status,
        "geometryStatus": geometry,
        "status": status,
        "masterEligible": status == "pass",
        "criticalRegions": {"status": geometry, "violations": violations},
    }


def _apply_event(game: Any, scenario: Scenario, event: Mapping[str, Any]) -> None:
    if "set" in event:
        setting = event["set"]
        game.result("game.capture_scene.set_parameter", {
            "scene": scenario.scene_id,
            "parameter": setting["parameter"],
            "value": setting["value"],
        })
    else:
        game.result("game.capture_scene.trigger_action", {
            "scene": scenario.scene_id,
            "action": event["action"]["id"],
            "arguments": {},
        })


def _ready_status(game: Any, scenario: Scenario, phase: str) -> dict[str, Any]:
    status = game.result("game.capture_scene.status")
    if status.get("ready") is not True or status.get("tick") != scenario.warmup_ticks:
        raise CaptureWorkflowError(f"capture scene failed {phase}: {status}")
    return status


def prepare_scenario(game: Any, scenario: Scenario) -> dict[str, Any]:
    listing = game.result("game.capture_scene.list")
    scene_ids = [item.get("id") for item in listing.get("scenes", []) if isinstance(item, Mapping)]
    if listing.get("apiVersion") != 1 or listing.get("gameId") != scenario.game:
        raise CaptureWorkflowError("game capture-scene identity mismatch")
    if scenario.scene_id not in scene_ids:
        raise CaptureWorkflowError(f"scenario scene is not registered: {scenario.scene_id}")
    describe = game.result("game.capture_scene.describe", {"scene": scenario.scene_id})
    validate_against_describe(scenario, describe)
    game.result("time.set_mode", {"mode": "manual"})
    game.result("game.capture_scene.load", {"scene": scenario.scene_id, "seed": scenario.seed})
    if scenario.warmup_ticks:
        game.result("time.step", {"count": scenario.warmup_ticks})
    status = _ready_status(game, scenario, "warmup")
    game.result("time.set_mode", {"mode": "run"})
    return status


def prepare_live(game: Any, scenario: Scenario) -> dict[str, Any]:
    status = prepare_scenario(game, scenario)
    game.result("time.set_mode", {"mode": "manual"})
    for event in expand_schedule(scenario).get(0, []):
        if "set" in event:
            _apply_event(game, scenario, event)
    game.result("time.set_mode", {"mode": "run"})
    return status


def reset_scenario_for_recording(game: Any, scenario: Scenario) -> dict[str, Any]:
    game.result("time.set_mode", {"mode": "manual"})
    game.result("game.capture_scene.reset", {"scene": scenario.scene_id, "seed": scenario.seed})
    if scenario.warmup_ticks:
        game.result("time.step", {"count": scenario.warmup_ticks})
    return _ready_status(game, scenario, "REC reset")


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
            _apply_event(game, scenario, event)
        game.result("time.step", {"count": scenario.ticks_per_frame})
        remaining = started + (frame + 1) / scenario.output_fps - monotonic()
        if remaining > 0:
            sleep(remaining)
    return {
        "status": "completed",
        "frames": scenario.duration_frames,
        "finalStatus": game.result("game.capture_scene.status"),
    }


def publish_take(
    recorder_root: Path,
    take_root: Path,
    *,
    representative_frame: Path,
    workflow: Mapping[str, Any],
) -> dict[str, Any]:
    recorder_root, take_root = recorder_root.resolve(), take_root.resolve()
    draft, master = take_root / "draft", take_root / "master"
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
        raise CaptureWorkflowError(f"recorder output is incomplete: {', '.join(missing)}")

    safe_area = workflow.get("safeArea")
    eligible = (
        workflow.get("mode") == "shot"
        and workflow.get("scenarioStatus") == "completed"
        and isinstance(safe_area, Mapping)
        and safe_area.get("status") == "pass"
    )
    classification = "master" if eligible else "draft"
    manifest = {
        **json.loads(sources["capture.json"].read_text(encoding="utf-8")),
        "workflow": dict(workflow),
        "classification": classification,
    }
    draft.mkdir(parents=True)
    for name in ("recording.mkv", "edit.mp4", "representative-frame.png"):
        os.replace(sources[name], draft / name)
    (draft / "capture.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    sources["capture.json"].unlink()
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
    return game_root / "tmp" / "captures" / label / f"{stamp}-{uuid.uuid4().hex[:8]}"


def record_with_transient_retry(
    output_root: Path,
    record: Callable[[Path], dict[str, Any]],
) -> tuple[Path, dict[str, Any]]:
    for attempt in (1, 2):
        root = output_root if attempt == 1 else output_root.with_name(f"{output_root.name}-retry-2")
        try:
            return root, record(root)
        except RuntimeError as exc:
            transient = (
                "incomplete ffprobe metadata" in str(exc)
                or "OBS window source stayed unhealthy" in str(exc)
            )
            if attempt == 1 and transient:
                print("OBS take was transiently unhealthy; retrying once...", flush=True)
                continue
            raise
    raise CaptureWorkflowError("capture retry did not run")


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
        raise CaptureWorkflowError(f"build the DevAPI game first: {catalog.executable}")
    if command == "shot":
        if shot_id is None:
            raise CaptureWorkflowError("shot id is required")
        shot = catalog.shot(shot_id)
        preset, duration, fps, label = (
            shot.preset, shot.duration_seconds, shot.scenario.output_fps, shot.id
        )
    elif command == "live":
        shot = None
        live_shot = catalog.shot(catalog.live["scene_shot"])
        preset, duration, fps, label = (
            catalog.live["preset"], float(catalog.live["duration_seconds"]),
            live_shot.scenario.output_fps, "live",
        )
    else:
        raise CaptureWorkflowError(f"unsupported capture command: {command}")

    settings = resolve_capture_settings(preset, None, fps)
    window = resolve_obs_capture_settings(settings)
    take_root = _take_root(catalog.game_root, label)
    recorder_root = take_root / ".recorder"
    with running_game(
        exe=str(catalog.executable),
        cwd=str(catalog.game_root),
        fresh_state=True,
        autosave_enabled=False,
        window_size=f"{window.width}x{window.height}",
    ) as game:
        if shot is None:
            prepare_live(game, live_shot.scenario)
            driver = (lambda: live_driver(game)) if live_driver else None
        else:
            prepare_scenario(game, shot.scenario)

            def driver() -> None:
                reset_scenario_for_recording(game, shot.scenario)
                play_scenario_realtime(game, shot.scenario)

        recorder_root, recorder_result = record_with_transient_retry(
            recorder_root,
            lambda root: record_take(
                pid=game.process_id,
                executable_name=catalog.executable.name,
                output_root=root,
                settings=settings,
                duration_seconds=duration,
                countdown=countdown,
                recording_driver=driver,
            ),
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
        scenario_status, shot_result = "not_applicable", None
    else:
        safe_area = evaluate_shot_safe_area(catalog.safe_area, shot.critical_regions)
        scenario_status = "completed"
        shot_result = {
            "id": shot.id,
            "purpose": shot.purpose,
            "durationSeconds": shot.duration_seconds,
            "angle": shot.angle,
            "scenario": str(shot.scenario_path.relative_to(catalog.game_root)),
            "scene": shot.scenario.scene_id,
            "seed": shot.scenario.seed,
        }
    result = publish_take(
        recorder_root,
        take_root,
        representative_frame=representative_frame,
        workflow={
            "game": catalog.game,
            "mode": command,
            "shot": shot_result,
            "scenarioStatus": scenario_status,
            "safeArea": safe_area,
            "recorderStatus": recorder_result.get("status"),
        },
    )
    try:
        recorder_root.rmdir()
    except OSError:
        pass
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="capture")
    parser.add_argument("--game-root", type=Path, required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("live")
    shot = commands.add_parser("shot")
    shot.add_argument("shot_id")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        result = run_capture(args.game_root, args.command, getattr(args, "shot_id", None))
    except (CaptureWorkflowError, DevApiError, OSError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(f"Capture: {result['classification']}")
    print(f"Draft:   {result['draft']}")
    if result["master"]:
        print(f"Master:  {result['master']}")
    else:
        safe = result["manifest"]["workflow"]["safeArea"]
        print(f"Master:  not promoted (safe area: {safe['status']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
