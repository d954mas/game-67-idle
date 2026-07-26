#!/usr/bin/env python3
"""Strict, game-agnostic deterministic capture-scene runner."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import subprocess
import sys
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from devapi_client import DevApiError, connect_existing, running_game, write_engine_capture_payload_png
from native_window import resize_and_park_client

SCHEMA = "ai_studio.capture_scenario"
VERSION = 1
API_VERSION = 1
TOP_KEYS = {"schema", "version", "api_version", "game", "scene", "viewport", "clock", "events", "ramps", "evidence"}


class CaptureScenarioError(ValueError):
    pass


def _object(value: Any, name: str, keys: set[str]) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise CaptureScenarioError(f"{name} must be an object")
    unknown = sorted(set(value).difference(keys))
    if unknown:
        raise CaptureScenarioError(f"{name} has unknown key: {unknown[0]}")
    return value


def _integer(value: Any, name: str, minimum: int = 0, maximum: int = 10_000_000) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not minimum <= value <= maximum:
        raise CaptureScenarioError(f"{name} must be an integer in {minimum}..{maximum}")
    return value


def _number(value: Any, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(float(value)):
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
        return (json.dumps(self.raw, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def parse_scenario(document: Any) -> Scenario:
    root = _object(document, "manifest", TOP_KEYS)
    if set(root) != TOP_KEYS:
        missing = sorted(TOP_KEYS.difference(root))
        raise CaptureScenarioError(f"manifest missing key: {missing[0]}")
    if root["schema"] != SCHEMA or root["version"] != VERSION or root["api_version"] != API_VERSION:
        raise CaptureScenarioError("unsupported capture scenario identity")
    game = _text(root["game"], "game")

    scene = _object(root["scene"], "scene", {"id", "contract_version", "seed"})
    if set(scene) != {"id", "contract_version", "seed"}:
        raise CaptureScenarioError("scene requires id, contract_version, and seed")
    scene_id = _text(scene["id"], "scene.id")
    contract_version = _integer(scene["contract_version"], "scene.contract_version", 1, 1000)
    seed = _integer(scene["seed"], "scene.seed", 0, 4_294_967_295)

    viewport_keys = {"orientation", "output_width", "output_height", "min_framebuffer_width", "min_framebuffer_height", "prefer_supersample"}
    viewport = _object(root["viewport"], "viewport", viewport_keys)
    if set(viewport) != viewport_keys:
        raise CaptureScenarioError("viewport is incomplete")
    if viewport["orientation"] != "vertical" or viewport["prefer_supersample"] is not True:
        raise CaptureScenarioError("v1 requires vertical output with prefer_supersample=true")
    output_width = _integer(viewport["output_width"], "viewport.output_width", 64, 8192)
    output_height = _integer(viewport["output_height"], "viewport.output_height", 64, 8192)
    minimum_width = _integer(viewport["min_framebuffer_width"], "viewport.min_framebuffer_width", output_width, 8192)
    minimum_height = _integer(viewport["min_framebuffer_height"], "viewport.min_framebuffer_height", output_height, 8192)
    if output_height <= output_width or minimum_height <= minimum_width:
        raise CaptureScenarioError("viewport must be portrait")

    clock_keys = {"fixed_tick_hz", "output_fps", "ticks_per_output_frame", "warmup_ticks", "duration_frames"}
    clock = _object(root["clock"], "clock", clock_keys)
    if set(clock) != clock_keys:
        raise CaptureScenarioError("clock is incomplete")
    fixed_tick_hz = _integer(clock["fixed_tick_hz"], "clock.fixed_tick_hz", 1, 1000)
    output_fps = _integer(clock["output_fps"], "clock.output_fps", 1, 240)
    ticks_per_frame = _integer(clock["ticks_per_output_frame"], "clock.ticks_per_output_frame", 1, 1000)
    warmup_ticks = _integer(clock["warmup_ticks"], "clock.warmup_ticks", 0, 100_000)
    duration_frames = _integer(clock["duration_frames"], "clock.duration_frames", 1, 100_000)
    if fixed_tick_hz != output_fps * ticks_per_frame:
        raise CaptureScenarioError("fixed_tick_hz must equal output_fps * ticks_per_output_frame")

    if not isinstance(root["events"], list) or not isinstance(root["ramps"], list):
        raise CaptureScenarioError("events and ramps must be arrays")
    events: list[dict[str, Any]] = []
    for index, event_value in enumerate(root["events"]):
        event = _object(event_value, f"events[{index}]", {"frame", "set", "action"})
        if set(event) not in ({"frame", "set"}, {"frame", "action"}):
            raise CaptureScenarioError(f"events[{index}] must contain exactly one set or action")
        frame = _integer(event["frame"], f"events[{index}].frame", 0, duration_frames - 1)
        if "set" in event:
            setting = _object(event["set"], f"events[{index}].set", {"parameter", "value"})
            if set(setting) != {"parameter", "value"}:
                raise CaptureScenarioError(f"events[{index}].set is incomplete")
            _text(setting["parameter"], f"events[{index}].set.parameter")
            if isinstance(setting["value"], float) and not math.isfinite(setting["value"]):
                raise CaptureScenarioError(f"events[{index}].set.value must be finite")
        else:
            action = _object(event["action"], f"events[{index}].action", {"id", "arguments"})
            if set(action) != {"id", "arguments"} or action["arguments"] != {}:
                raise CaptureScenarioError(f"events[{index}].action requires empty arguments")
            _text(action["id"], f"events[{index}].action.id")
        events.append({**event, "frame": frame})

    ramps: list[dict[str, Any]] = []
    for index, ramp_value in enumerate(root["ramps"]):
        keys = {"start_frame", "end_frame", "parameter", "from", "to", "curve"}
        ramp = _object(ramp_value, f"ramps[{index}]", keys)
        if set(ramp) != keys:
            raise CaptureScenarioError(f"ramps[{index}] is incomplete")
        start = _integer(ramp["start_frame"], f"ramps[{index}].start_frame", 0, duration_frames - 1)
        end = _integer(ramp["end_frame"], f"ramps[{index}].end_frame", start + 1, duration_frames - 1)
        parameter = _text(ramp["parameter"], f"ramps[{index}].parameter")
        start_value = _number(ramp["from"], f"ramps[{index}].from")
        end_value = _number(ramp["to"], f"ramps[{index}].to")
        if ramp["curve"] not in ("linear", "smoothstep"):
            raise CaptureScenarioError(f"ramps[{index}].curve must be linear or smoothstep")
        ramps.append({"start_frame": start, "end_frame": end, "parameter": parameter,
                      "from": start_value, "to": end_value, "curve": ramp["curve"]})

    evidence = _object(root["evidence"], "evidence", {"boundary_radius_frames", "uniform_contact_sheet_samples"})
    if set(evidence) != {"boundary_radius_frames", "uniform_contact_sheet_samples"}:
        raise CaptureScenarioError("evidence is incomplete")
    boundary_radius = _integer(evidence["boundary_radius_frames"], "evidence.boundary_radius_frames", 0, 30)
    contact_samples = _integer(evidence["uniform_contact_sheet_samples"], "evidence.uniform_contact_sheet_samples", 1, 64)

    normalized = json.loads(json.dumps(root, sort_keys=True, separators=(",", ":")))
    scenario = Scenario(normalized, game, scene_id, contract_version, seed, output_width, output_height,
                        minimum_width, minimum_height, fixed_tick_hz, output_fps, ticks_per_frame,
                        warmup_ticks, duration_frames, tuple(events), tuple(ramps), boundary_radius,
                        contact_samples)
    expand_schedule(scenario)
    return scenario


def expand_schedule(scenario: Scenario) -> dict[int, list[dict[str, Any]]]:
    schedule: dict[int, list[dict[str, Any]]] = {}
    writes: set[tuple[int, str]] = set()
    for event in scenario.events:
        frame = event["frame"]
        if "set" in event:
            key = (frame, event["set"]["parameter"])
            if key in writes:
                raise CaptureScenarioError(f"duplicate parameter write at frame {frame}: {key[1]}")
            writes.add(key)
        schedule.setdefault(frame, []).append(event)
    for ramp in scenario.ramps:
        span = ramp["end_frame"] - ramp["start_frame"]
        for frame in range(ramp["start_frame"], ramp["end_frame"] + 1):
            key = (frame, ramp["parameter"])
            if key in writes:
                raise CaptureScenarioError(f"duplicate parameter write at frame {frame}: {key[1]}")
            writes.add(key)
            t = (frame - ramp["start_frame"]) / span
            if ramp["curve"] == "smoothstep":
                t = t * t * (3.0 - 2.0 * t)
            value = ramp["from"] + (ramp["to"] - ramp["from"]) * t
            schedule.setdefault(frame, []).append(
                {"frame": frame, "set": {"parameter": ramp["parameter"], "value": value}}
            )
    return schedule


def validate_against_describe(scenario: Scenario, describe: dict[str, Any]) -> None:
    if describe.get("apiVersion") != API_VERSION or describe.get("gameId") != scenario.game:
        raise CaptureScenarioError("game/API identity mismatch")
    scene = describe.get("scene")
    if not isinstance(scene, dict) or scene.get("id") != scenario.scene_id or scene.get("contractVersion") != scenario.contract_version:
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
            desc = parameters.get(setting["parameter"])
            if desc is None:
                raise CaptureScenarioError(f"unknown parameter: {setting['parameter']}")
            value = setting["value"]
            if desc.get("type") == "float":
                number = _number(value, setting["parameter"])
                if not float(desc["minimum"]) <= number <= float(desc["maximum"]):
                    raise CaptureScenarioError(f"parameter out of range: {setting['parameter']}")
            elif desc.get("type") == "enum":
                if value not in desc.get("values", []):
                    raise CaptureScenarioError(f"invalid enum value: {setting['parameter']}")
            else:
                raise CaptureScenarioError(f"unsupported parameter type: {setting['parameter']}")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")


def _game_root(executable: Path) -> Path:
    for candidate in executable.parents:
        if (candidate / "CMakeLists.txt").is_file() and (candidate / "src").is_dir():
            return candidate
    return executable.parent


def _encode_video(frame_dir: Path, output: Path, scenario: Scenario, source_width: int, source_height: int) -> None:
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        raise CaptureScenarioError("ffmpeg is required")
    if source_width < scenario.output_width or source_height < scenario.output_height:
        raise CaptureScenarioError("framebuffer_too_small")
    source_ratio = source_width / source_height
    output_ratio = scenario.output_width / scenario.output_height
    if abs(source_ratio - output_ratio) > 0.005:
        raise CaptureScenarioError("framebuffer_aspect_mismatch")
    args = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-framerate", str(scenario.output_fps),
            "-i", str(frame_dir / "f_%06d.png")]
    if (source_width, source_height) != (scenario.output_width, scenario.output_height):
        args += ["-vf", f"scale={scenario.output_width}:{scenario.output_height}:flags=lanczos"]
    args += ["-frames:v", str(scenario.duration_frames), "-c:v", "libx264", "-pix_fmt", "yuv420p",
             "-movflags", "+faststart", str(output)]
    subprocess.run(args, check=True, shell=False)


def _contact_sheet(frame_dir: Path, output: Path, scenario: Scenario) -> None:
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        raise CaptureScenarioError("ffmpeg is required")
    count = min(scenario.contact_samples, scenario.duration_frames)
    indices = sorted({round(i * (scenario.duration_frames - 1) / max(count - 1, 1)) for i in range(count)})
    cols = 4
    rows = math.ceil(len(indices) / cols)
    selector = "+".join(f"eq(n\\,{index})" for index in indices)
    cell_w = max(180, scenario.output_width // cols)
    cell_h = round(cell_w * scenario.output_height / scenario.output_width)
    filter_graph = f"select='{selector}',scale={cell_w}:{cell_h},tile={cols}x{rows}:padding=4:margin=4"
    subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-framerate", str(scenario.output_fps),
                    "-i", str(frame_dir / "f_%06d.png"), "-vf", filter_graph, "-frames:v", "1", str(output)],
                   check=True, shell=False)


def run_scenario(scenario: Scenario, executable: Path, output_root: Path,
                 visible_contract: Path | None = None) -> Path:
    executable = executable.resolve(strict=True)
    output_root = output_root.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    game_root = _game_root(executable)
    # Launch comfortably on-screen first. Win32 then performs the oversized
    # off-screen client resize; launching oversized lets GLFW apply a monitor
    # clamp that can persist into the first resize on some DPI configurations.
    window_size = f"{max(320, scenario.minimum_width // 2)}x{max(480, scenario.minimum_height // 2)}"
    with running_game(exe=str(executable), cwd=str(game_root), window_size=window_size) as game:
        methods = game.endpoint_methods()
        required = {f"game.capture_scene.{name}" for name in ("list", "describe", "load", "reset", "set_parameter", "trigger_action", "status")}
        if not required.issubset(methods):
            raise CaptureScenarioError("capture-scene endpoint family is incomplete")
        listing = game.result("game.capture_scene.list")
        if listing.get("apiVersion") != API_VERSION or listing.get("gameId") != scenario.game:
            raise CaptureScenarioError("game/API identity mismatch")
        ids = [item.get("id") for item in listing.get("scenes", [])]
        if ids != sorted(ids) or len(ids) != len(set(ids)) or scenario.scene_id not in ids:
            raise CaptureScenarioError("invalid capture scene catalog")
        describe = game.result("game.capture_scene.describe", {"scene": scenario.scene_id})
        validate_against_describe(scenario, describe)
        resize_and_park_client(game.process_id, scenario.minimum_width, scenario.minimum_height)
        game.wait_frames(4)
        game.result("time.set_mode", {"mode": "manual"})

        probe, _ = game.capture_frame_and_step(1)
        if not isinstance(probe, dict):
            raise CaptureScenarioError("invalid framebuffer payload")
        width = _integer(probe.get("width"), "capture.width", 1, 16384)
        height = _integer(probe.get("height"), "capture.height", 1, 16384)
        # Windows DPI can turn a 1080x1920 logical client into a 1622x2883
        # framebuffer. That is valid supersampling but needlessly inflates each
        # base64 response. Calibrate once while no target scene is active and
        # keep a small downscale margin instead of risking either upscale or a
        # transport timeout on visually denser scenes.
        scale = min(width / scenario.minimum_width, height / scenario.minimum_height)
        if scale > 1.15 and game.process_id:
            calibrated_width = math.ceil(scenario.minimum_width / scale * 1.04)
            calibrated_height = math.ceil(scenario.minimum_height / scale * 1.04)
            resize_and_park_client(game.process_id, calibrated_width, calibrated_height)
            game.result("time.step", {"count": 4})
            probe, _ = game.capture_frame_and_step(1)
            if not isinstance(probe, dict):
                raise CaptureScenarioError("invalid calibrated framebuffer payload")
            width = _integer(probe.get("width"), "capture.width", 1, 16384)
            height = _integer(probe.get("height"), "capture.height", 1, 16384)
        if width < scenario.minimum_width or height < scenario.minimum_height:
            raise CaptureScenarioError(f"framebuffer_too_small: {width}x{height}")
        if height <= width or abs(width / height - scenario.output_width / scenario.output_height) > 0.005:
            raise CaptureScenarioError(f"framebuffer_aspect_mismatch: {width}x{height}")

        run_id = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()) + "-" + uuid.uuid4().hex[:8]
        staging = output_root / f".{run_id}.staging"
        final = output_root / run_id
        if staging.exists() or final.exists():
            raise CaptureScenarioError("unique run directory collision")
        staging.mkdir()
        frames = staging / "frames"
        frames.mkdir()
        write_json(staging / "manifest.json", scenario.raw)
        write_json(staging / "describe.json", describe)
        executable_record = {"path": str(executable), "sha256": sha256_file(executable)}
        write_json(staging / "executable.json", executable_record)
        if visible_contract is not None:
            shutil.copy2(visible_contract, staging / "visible_frame_contract.v1.json")
        else:
            write_json(staging / "visible_frame_contract.v1.json", {"apiVersion": 1, "captureBeforeStep": True,
                       "gameId": scenario.game, "probeAction": "marker.toggle", "probeScene": "contract.visible_frame_probe",
                       "scheduledEventFrame": 1, "visibleFrameOffset": 0, "visiblePngFrame": 1})

        game.result("game.capture_scene.load", {"scene": scenario.scene_id, "seed": scenario.seed})
        if scenario.warmup_ticks:
            game.result("time.step", {"count": scenario.warmup_ticks})
        initial = game.result("game.capture_scene.status")
        if initial.get("tick") != scenario.warmup_ticks or initial.get("ready") is not True:
            raise CaptureScenarioError(f"capture scene failed warmup contract: {initial}")

        schedule = expand_schedule(scenario)
        frame_records: list[dict[str, Any]] = []
        action_frames: list[int] = []
        previous_status = initial
        try:
            for frame in range(scenario.duration_frames):
                before = previous_status
                for event in schedule.get(frame, []):
                    if "set" in event:
                        setting = event["set"]
                        set_result = game.result("game.capture_scene.set_parameter", {"scene": scenario.scene_id,
                                                 "parameter": setting["parameter"], "value": setting["value"]})
                        before = set_result["status"]
                    else:
                        action = event["action"]
                        before = game.result("game.capture_scene.trigger_action", {"scene": scenario.scene_id,
                                             "action": action["id"], "arguments": {}})
                        action_frames.append(frame)
                capture, step = game.capture_frame_and_step(scenario.ticks_per_frame)
                path = frames / f"f_{frame:06d}.png"
                write_engine_capture_payload_png(capture, str(path))
                after = game.result("game.capture_scene.status")
                frame_records.append({"frame": frame, "before": before, "after": after, "step": step})
                previous_status = after
        except (ConnectionError, OSError) as exc:
            exit_code = game.process.poll() if game.process is not None else None
            replacement = connect_existing(port=game.port, timeout=1.0)
            process_alive = replacement is not None
            if replacement is not None:
                replacement.close()
            raise CaptureScenarioError(
                f"DevAPI transport disconnected at output frame {len(frame_records)}; "
                f"processAlive={process_alive}; exitCode={exit_code}"
            ) from exc

    _encode_video(frames, staging / "video.mp4", scenario, width, height)
    _contact_sheet(frames, staging / "contact_sheet.jpg", scenario)
    visible = json.loads((staging / "visible_frame_contract.v1.json").read_text(encoding="utf-8"))
    offset = _integer(visible.get("visibleFrameOffset"), "visibleFrameOffset", 0, 1)
    boundary_indices: set[int] = {0, scenario.duration_frames - 1}
    for frame in action_frames:
        visible_frame = frame + offset
        for delta in range(-scenario.boundary_radius, scenario.boundary_radius + 1):
            boundary_indices.add(min(max(visible_frame + delta, 0), scenario.duration_frames - 1))
    boundary_dir = staging / "boundaries"
    boundary_dir.mkdir()
    for index in sorted(boundary_indices):
        shutil.copy2(frames / f"f_{index:06d}.png", boundary_dir / f"f_{index:06d}.png")
    diagnostics = {"schema": "ai_studio.capture_diagnostics", "version": 1, "framebuffer": {"width": width, "height": height},
                   "initialStatus": initial, "frames": frame_records, "targetLoadCount": 1,
                   "warmupTicks": scenario.warmup_ticks, "durationFrames": scenario.duration_frames}
    write_json(staging / "diagnostics.json", diagnostics)

    inventory_paths = [path for path in staging.rglob("*") if path.is_file() and path.name not in ("handoff.json", "provenance.json")]
    inventory = [{"path": path.relative_to(staging).as_posix(), "sha256": sha256_file(path)} for path in sorted(inventory_paths)]
    provenance = {"schema": "ai_studio.capture_provenance", "version": 1, "artifacts": inventory}
    write_json(staging / "provenance.json", provenance)
    provenance_hash = sha256_file(staging / "provenance.json")
    manifest_hash = sha256_file(staging / "manifest.json")
    describe_hash = sha256_file(staging / "describe.json")
    video_hash = sha256_file(staging / "video.mp4")
    handoff = {"apiVersion": API_VERSION, "candidateBundleId": run_id, "captureApiVersion": API_VERSION,
               "contactSheetPath": "contact_sheet.jpg", "describeSha256": describe_hash,
               "diagnosticsPath": "diagnostics.json", "executableSha256": executable_record["sha256"],
               "gameId": scenario.game, "manifestSha256": manifest_hash, "provenancePath": "provenance.json",
               "provenanceSha256": provenance_hash, "sceneContractVersion": scenario.contract_version,
               "sceneId": scenario.scene_id, "schema": "ai_studio.capture_handoff", "status": "ready", "version": 1,
               "video": {"durationFrames": scenario.duration_frames, "fps": scenario.output_fps,
                         "height": scenario.output_height, "path": "video.mp4", "sha256": video_hash,
                         "width": scenario.output_width}}
    write_json(staging / "handoff.json", handoff)
    for attempt in range(6):
        try:
            staging.rename(final)
            break
        except PermissionError:
            if attempt == 5:
                raise
            time.sleep(0.2 * (attempt + 1))
    return final


def load_manifest(path: Path) -> Scenario:
    return parse_scenario(json.loads(path.read_text(encoding="utf-8")))


def compare_runs(a: Path, b: Path) -> bool:
    first = json.loads((a / "handoff.json").read_text(encoding="utf-8"))
    second = json.loads((b / "handoff.json").read_text(encoding="utf-8"))
    fields = ("apiVersion", "captureApiVersion", "describeSha256", "executableSha256", "gameId",
              "manifestSha256", "sceneContractVersion", "sceneId")
    return all(first.get(field) == second.get(field) for field in fields)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("list")
    describe_parser = sub.add_parser("describe")
    describe_parser.add_argument("scene_id")
    validate_parser = sub.add_parser("validate")
    validate_parser.add_argument("manifest", type=Path)
    run_parser = sub.add_parser("run")
    run_parser.add_argument("manifest", type=Path)
    run_parser.add_argument("--out", required=True, type=Path)
    run_parser.add_argument("--visible-contract", type=Path)
    compare_parser = sub.add_parser("compare")
    compare_parser.add_argument("run_a", type=Path)
    compare_parser.add_argument("run_b", type=Path)
    args = parser.parse_args(argv)

    if args.command == "validate":
        scenario = load_manifest(args.manifest)
        print(scenario.normalized_bytes().decode("utf-8"), end="")
        return 0
    if args.command == "compare":
        matched = compare_runs(args.run_a, args.run_b)
        print(json.dumps({"equal": matched}))
        return 0 if matched else 1
    if args.exe is None:
        parser.error("--exe is required")
    executable = args.exe.resolve(strict=True)
    if args.command in ("list", "describe"):
        with running_game(exe=str(executable), cwd=str(_game_root(executable))) as game:
            value = game.result("game.capture_scene.list" if args.command == "list" else "game.capture_scene.describe",
                                {} if args.command == "list" else {"scene": args.scene_id})
        print(json.dumps(value, ensure_ascii=False, sort_keys=True))
        return 0
    scenario = load_manifest(args.manifest)
    output = run_scenario(scenario, executable, args.out, args.visible_contract)
    print(output)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (CaptureScenarioError, DevApiError, subprocess.CalledProcessError) as exc:
        print(json.dumps({"schema": "ai_studio.capture_error", "version": 1,
                          "code": type(exc).__name__, "message": str(exc)}, ensure_ascii=False), file=sys.stderr)
        raise SystemExit(2)
