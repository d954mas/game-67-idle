#!/usr/bin/env python3
"""Small agent-facing live/shot workflow over the existing OBS recorder."""

from __future__ import annotations

import json
import os
import shutil
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Mapping

from capture_safe_area import UnsafeMask, evaluate_critical_regions
from capture_scenario import Scenario, expand_schedule, parse_scenario


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
