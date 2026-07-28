#!/usr/bin/env python3
"""Parse game-owned deterministic capture scenarios."""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from typing import Any

SCHEMA = "ai_studio.capture_scenario"
VERSION = 1
API_VERSION = 1
TOP_KEYS = {
    "schema", "version", "api_version", "game", "scene", "viewport",
    "clock", "events", "ramps", "evidence",
}


class CaptureScenarioError(ValueError):
    pass


def _object(value: Any, name: str, keys: set[str]) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise CaptureScenarioError(f"{name} must be an object")
    unknown = set(value) - keys
    if unknown:
        raise CaptureScenarioError(f"{name} has unknown key: {sorted(unknown)[0]}")
    return value


def _exact(value: Any, name: str, keys: set[str]) -> dict[str, Any]:
    result = _object(value, name, keys)
    missing = keys - set(result)
    if missing:
        raise CaptureScenarioError(f"{name} missing key: {sorted(missing)[0]}")
    return result


def _integer(value: Any, name: str, minimum: int, maximum: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise CaptureScenarioError(f"{name} must be an integer in {minimum}..{maximum}")
    return value


def _number(value: Any, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise CaptureScenarioError(f"{name} must be a finite number")
    return float(value)


def _text(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value or len(value) > 128:
        raise CaptureScenarioError(f"{name} must be a non-empty string up to 128 characters")
    return value


@dataclass(frozen=True)
class Scenario:
    raw: dict[str, Any]
    game: str
    scene_id: str
    contract_version: int
    seed: int
    output_width: int
    output_height: int
    minimum_width: int
    minimum_height: int
    fixed_tick_hz: int
    output_fps: int
    ticks_per_frame: int
    warmup_ticks: int
    duration_frames: int
    events: tuple[dict[str, Any], ...]
    ramps: tuple[dict[str, Any], ...]
    boundary_radius: int
    contact_samples: int

    def normalized_bytes(self) -> bytes:
        text = json.dumps(self.raw, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        return f"{text}\n".encode()


def parse_scenario(document: Any) -> Scenario:
    root = _exact(document, "manifest", TOP_KEYS)
    if (root["schema"], root["version"], root["api_version"]) != (SCHEMA, VERSION, API_VERSION):
        raise CaptureScenarioError("unsupported capture scenario identity")

    scene = _exact(root["scene"], "scene", {"id", "contract_version", "seed"})
    viewport = _exact(root["viewport"], "viewport", {
        "orientation", "output_width", "output_height", "min_framebuffer_width",
        "min_framebuffer_height", "prefer_supersample",
    })
    clock = _exact(root["clock"], "clock", {
        "fixed_tick_hz", "output_fps", "ticks_per_output_frame",
        "warmup_ticks", "duration_frames",
    })
    evidence = _exact(
        root["evidence"], "evidence",
        {"boundary_radius_frames", "uniform_contact_sheet_samples"},
    )

    output_width = _integer(viewport["output_width"], "viewport.output_width", 64, 8192)
    output_height = _integer(viewport["output_height"], "viewport.output_height", 64, 8192)
    minimum_width = _integer(viewport["min_framebuffer_width"], "viewport.min_framebuffer_width", output_width, 8192)
    minimum_height = _integer(viewport["min_framebuffer_height"], "viewport.min_framebuffer_height", output_height, 8192)
    if viewport["orientation"] != "vertical" or viewport["prefer_supersample"] is not True:
        raise CaptureScenarioError("v1 requires vertical output with prefer_supersample=true")
    if output_height <= output_width or minimum_height <= minimum_width:
        raise CaptureScenarioError("viewport must be portrait")

    fixed_tick_hz = _integer(clock["fixed_tick_hz"], "clock.fixed_tick_hz", 1, 1000)
    output_fps = _integer(clock["output_fps"], "clock.output_fps", 1, 240)
    ticks_per_frame = _integer(clock["ticks_per_output_frame"], "clock.ticks_per_output_frame", 1, 1000)
    if fixed_tick_hz != output_fps * ticks_per_frame:
        raise CaptureScenarioError("fixed_tick_hz must equal output_fps * ticks_per_output_frame")
    warmup_ticks = _integer(clock["warmup_ticks"], "clock.warmup_ticks", 0, 100_000)
    duration_frames = _integer(clock["duration_frames"], "clock.duration_frames", 1, 100_000)

    if not isinstance(root["events"], list) or not isinstance(root["ramps"], list):
        raise CaptureScenarioError("events and ramps must be arrays")
    events: list[dict[str, Any]] = []
    for index, raw_event in enumerate(root["events"]):
        event = _object(raw_event, f"events[{index}]", {"frame", "set", "action"})
        if set(event) not in ({"frame", "set"}, {"frame", "action"}):
            raise CaptureScenarioError(f"events[{index}] must contain exactly one set or action")
        frame = _integer(event["frame"], f"events[{index}].frame", 0, duration_frames - 1)
        if "set" in event:
            setting = _exact(event["set"], f"events[{index}].set", {"parameter", "value"})
            _text(setting["parameter"], f"events[{index}].set.parameter")
            if isinstance(setting["value"], float) and not math.isfinite(setting["value"]):
                raise CaptureScenarioError(f"events[{index}].set.value must be finite")
        else:
            action = _exact(event["action"], f"events[{index}].action", {"id", "arguments"})
            _text(action["id"], f"events[{index}].action.id")
            if action["arguments"] != {}:
                raise CaptureScenarioError(f"events[{index}].action requires empty arguments")
        events.append({**event, "frame": frame})

    ramps: list[dict[str, Any]] = []
    ramp_keys = {"start_frame", "end_frame", "parameter", "from", "to", "curve"}
    for index, raw_ramp in enumerate(root["ramps"]):
        ramp = _exact(raw_ramp, f"ramps[{index}]", ramp_keys)
        start = _integer(ramp["start_frame"], f"ramps[{index}].start_frame", 0, duration_frames - 1)
        end = _integer(ramp["end_frame"], f"ramps[{index}].end_frame", start + 1, duration_frames - 1)
        curve = ramp["curve"]
        if curve not in ("linear", "smoothstep"):
            raise CaptureScenarioError(f"ramps[{index}].curve must be linear or smoothstep")
        ramps.append({
            "start_frame": start,
            "end_frame": end,
            "parameter": _text(ramp["parameter"], f"ramps[{index}].parameter"),
            "from": _number(ramp["from"], f"ramps[{index}].from"),
            "to": _number(ramp["to"], f"ramps[{index}].to"),
            "curve": curve,
        })

    scenario = Scenario(
        json.loads(json.dumps(root)), _text(root["game"], "game"),
        _text(scene["id"], "scene.id"),
        _integer(scene["contract_version"], "scene.contract_version", 1, 1000),
        _integer(scene["seed"], "scene.seed", 0, 4_294_967_295),
        output_width, output_height, minimum_width, minimum_height,
        fixed_tick_hz, output_fps, ticks_per_frame, warmup_ticks, duration_frames,
        tuple(events), tuple(ramps),
        _integer(evidence["boundary_radius_frames"], "evidence.boundary_radius_frames", 0, 30),
        _integer(evidence["uniform_contact_sheet_samples"], "evidence.uniform_contact_sheet_samples", 1, 64),
    )
    expand_schedule(scenario)
    return scenario


def expand_schedule(scenario: Scenario) -> dict[int, list[dict[str, Any]]]:
    schedule: dict[int, list[dict[str, Any]]] = {}
    writes: set[tuple[int, str]] = set()

    def add(frame: int, event: dict[str, Any]) -> None:
        if "set" in event:
            key = (frame, event["set"]["parameter"])
            if key in writes:
                raise CaptureScenarioError(f"duplicate parameter write at frame {frame}: {key[1]}")
            writes.add(key)
        schedule.setdefault(frame, []).append(event)

    for event in scenario.events:
        add(event["frame"], event)
    for ramp in scenario.ramps:
        span = ramp["end_frame"] - ramp["start_frame"]
        for frame in range(ramp["start_frame"], ramp["end_frame"] + 1):
            t = (frame - ramp["start_frame"]) / span
            if ramp["curve"] == "smoothstep":
                t = t * t * (3 - 2 * t)
            value = ramp["from"] + (ramp["to"] - ramp["from"]) * t
            add(frame, {"frame": frame, "set": {"parameter": ramp["parameter"], "value": value}})
    return schedule


def validate_against_describe(scenario: Scenario, describe: dict[str, Any]) -> None:
    if describe.get("apiVersion") != API_VERSION or describe.get("gameId") != scenario.game:
        raise CaptureScenarioError("game/API identity mismatch")
    scene = describe.get("scene")
    if not isinstance(scene, dict) or (
        scene.get("id"), scene.get("contractVersion")
    ) != (scenario.scene_id, scenario.contract_version):
        raise CaptureScenarioError("scene contract mismatch")
    parameters = {item.get("id"): item for item in scene.get("parameters", []) if isinstance(item, dict)}
    actions = {item.get("id") for item in scene.get("actions", []) if isinstance(item, dict)}
    for entries in expand_schedule(scenario).values():
        for event in entries:
            if "action" in event:
                if event["action"]["id"] not in actions:
                    raise CaptureScenarioError(f"unknown action: {event['action']['id']}")
                continue
            setting = event["set"]
            description = parameters.get(setting["parameter"])
            if description is None:
                raise CaptureScenarioError(f"unknown parameter: {setting['parameter']}")
            value = setting["value"]
            if description.get("type") == "float":
                value = _number(value, setting["parameter"])
                if not float(description["minimum"]) <= value <= float(description["maximum"]):
                    raise CaptureScenarioError(f"parameter out of range: {setting['parameter']}")
            elif description.get("type") == "enum":
                if value not in description.get("values", []):
                    raise CaptureScenarioError(f"invalid enum value: {setting['parameter']}")
            else:
                raise CaptureScenarioError(f"unsupported parameter type: {setting['parameter']}")
