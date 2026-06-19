#!/usr/bin/env python3
"""Native debug performance gate for Backrooms Liminal."""

from __future__ import annotations

import argparse
import json
import math
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.devapi.devapi_client import running_game  # noqa: E402


DEFAULT_JSON = ROOT / "gamedesign" / "projects" / "backrooms-liminal" / "reviews" / "perf_gate_latest.json"
DEFAULT_MD = ROOT / "gamedesign" / "projects" / "backrooms-liminal" / "reviews" / "perf_gate_latest.md"


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = min(len(ordered) - 1, max(0, round((len(ordered) - 1) * p)))
    return ordered[idx]


def timed_ms(fn) -> tuple[float, Any]:
    start = time.perf_counter()
    result = fn()
    return (time.perf_counter() - start) * 1000.0, result


def pass_fail(value: float, budget: float) -> str:
    return "PASS" if value <= budget else "FAIL"


def write_report_md(path: Path, report: dict[str, Any]) -> None:
    checks = report["checks"]
    frame = report["frame_wait_ms_per_frame"]
    stats = report["perf_stats"]
    gfx = stats.get("gfx", {})
    portal = stats.get("portal", {})
    lines = [
        "# Backrooms Liminal Perf Gate",
        "",
        f"- Verdict: **{report['verdict']}**",
        f"- Build: native-debug, {report['window_size']}",
        f"- Samples: {frame['samples']} chunks, {report['frames_per_sample']} frames/chunk",
        f"- Median frame wait: {frame['median']:.2f} ms (budget {checks['median_ms']['budget']:.2f}, {checks['median_ms']['status']})",
        f"- P95 frame wait: {frame['p95']:.2f} ms (budget {checks['p95_ms']['budget']:.2f}, {checks['p95_ms']['status']})",
        f"- Max frame wait: {frame['max']:.2f} ms (budget {checks['max_ms']['budget']:.2f}, {checks['max_ms']['status']})",
        f"- Draw calls: {gfx.get('draw_calls', 0)} (budget {checks['draw_calls']['budget']}, {checks['draw_calls']['status']})",
        f"- Frame vertices: {gfx.get('vertices', 0)} (budget {checks['frame_vertices']['budget']}, {checks['frame_vertices']['status']})",
        f"- Portal overlay vertices: {portal.get('overlay_vertices', 0)} (budget {checks['portal_overlay_vertices']['budget']}, {checks['portal_overlay_vertices']['status']})",
        f"- Mouse-look: yaw {report['mouse_look']['yaw_before']:.4f} -> {report['mouse_look']['yaw_after']:.4f} ({checks['mouse_look']['status']})",
        "",
        "Notes:",
        "- This gate measures native gameplay frame pacing through DevAPI frame waits.",
        "- It intentionally does not include framebuffer/video capture readback cost.",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--port", type=int, default=9145)
    parser.add_argument("--window-size", default="1280x720")
    parser.add_argument("--samples", type=int, default=80)
    parser.add_argument("--frames-per-sample", type=int, default=5)
    parser.add_argument("--budget-median-ms", type=float, default=22.0)
    parser.add_argument("--budget-p95-ms", type=float, default=25.0)
    parser.add_argument("--budget-max-ms", type=float, default=35.0)
    parser.add_argument("--budget-draw-calls", type=int, default=8)
    parser.add_argument("--budget-frame-vertices", type=int, default=3200)
    parser.add_argument("--budget-portal-overlay-vertices", type=int, default=1450)
    parser.add_argument("--json", type=Path, default=DEFAULT_JSON)
    parser.add_argument("--md", type=Path, default=DEFAULT_MD)
    args = parser.parse_args()

    if not args.skip_build:
        subprocess.run(["cmake", "--build", "--preset", "native-debug"], cwd=ROOT, check=True)

    with running_game(port=args.port, fresh_state=True, window_size=args.window_size) as game:
        game.wait_frames(30)
        game.result("game.action.set_pose", {"x": -3.35, "z": 10.8, "yaw": -math.pi / 2.0})
        game.wait_frames(4)
        game.result("game.action.place_mark")
        game.result("game.action.set_pose", {"x": 1.25, "z": 10.8, "yaw": math.pi / 2.0})
        game.wait_frames(30)

        samples = []
        for _ in range(args.samples):
            ms, _ = timed_ms(lambda: game.result("frame.wait", {"frames": args.frames_per_sample}))
            samples.append(ms / float(args.frames_per_sample))

        perf_stats = game.result("game.perf.stats")

        before = game.result("game.action.set_pose", {"x": 0.0, "z": 6.2, "yaw": 0.0})
        game.result("input.move", {"x": 640.0, "y": 360.0})
        game.wait_frames(1)
        game.result("input.move", {"x": 820.0, "y": 360.0})
        game.wait_frames(3)
        after = game.result("game.state")

    frame = {
        "samples": len(samples),
        "median": statistics.median(samples),
        "p95": percentile(samples, 0.95),
        "max": max(samples),
    }
    gfx = perf_stats.get("gfx", {})
    portal = perf_stats.get("portal", {})
    mouse_delta = abs(float(after["yaw"]) - float(before["yaw"]))
    checks = {
        "median_ms": {"value": frame["median"], "budget": args.budget_median_ms, "status": pass_fail(frame["median"], args.budget_median_ms)},
        "p95_ms": {"value": frame["p95"], "budget": args.budget_p95_ms, "status": pass_fail(frame["p95"], args.budget_p95_ms)},
        "max_ms": {"value": frame["max"], "budget": args.budget_max_ms, "status": pass_fail(frame["max"], args.budget_max_ms)},
        "draw_calls": {"value": gfx.get("draw_calls", 0), "budget": args.budget_draw_calls, "status": pass_fail(float(gfx.get("draw_calls", 0)), float(args.budget_draw_calls))},
        "frame_vertices": {"value": gfx.get("vertices", 0), "budget": args.budget_frame_vertices, "status": pass_fail(float(gfx.get("vertices", 0)), float(args.budget_frame_vertices))},
        "portal_overlay_vertices": {
            "value": portal.get("overlay_vertices", 0),
            "budget": args.budget_portal_overlay_vertices,
            "status": pass_fail(float(portal.get("overlay_vertices", 0)), float(args.budget_portal_overlay_vertices)),
        },
        "mouse_look": {"value": mouse_delta, "budget": ">=0.05 yaw delta", "status": "PASS" if mouse_delta >= 0.05 else "FAIL"},
    }
    verdict = "PASS" if all(item["status"] == "PASS" for item in checks.values()) else "FAIL"
    report = {
        "verdict": verdict,
        "window_size": args.window_size,
        "frames_per_sample": args.frames_per_sample,
        "frame_wait_ms_per_frame": frame,
        "perf_stats": perf_stats,
        "mouse_look": {
            "yaw_before": float(before["yaw"]),
            "yaw_after": float(after["yaw"]),
            "yaw_delta": mouse_delta,
        },
        "checks": checks,
    }

    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    write_report_md(args.md, report)
    print(json.dumps(report, indent=2))
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
