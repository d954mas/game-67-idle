#!/usr/bin/env python3
"""Fan out many image generations concurrently, with skip-if-exists.

Generation is API-bound (gpt-image-2, 30-60s/image) and dominates a whole-game
art run, but each call is independent. gen_batch reads a jobs file and runs
generate_image.py for up to --concurrency jobs at once, so 100 serial assets
(~75 min) finish in ~20 min. Unchanged assets are skipped automatically via
generate_image.py's hash sidecar, so re-runs only pay for changed/failed assets.

Keep the pool small (default 3) to respect Codex rate limits.

Jobs file: a JSON array (or {"jobs": [...]}) of objects:
  {"prompt": "...", "out": "tmp/x.png", "size"?, "quality"?, "format"?,
   "model"?, "background"?, "input_image"? [paths], "name"? }

Usage:
  py -3.12 gen_batch.py --jobs jobs.json [--concurrency 3] [--force] [--dry-run]
                        [--generator <path to generate_image.py>]
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_GENERATOR = str(HERE / "generate_image.py")


def build_cmd(job: dict, generator: str, force: bool) -> list[str]:
    cmd = [sys.executable, generator, "--prompt", job["prompt"], "--out", job["out"]]
    for key in ("size", "quality", "format", "model", "background"):
        if job.get(key):
            cmd += [f"--{key}", str(job[key])]
    for img in job.get("input_image", []) or []:
        cmd += ["--input-image", str(img)]
    if force:
        cmd.append("--force")
    return cmd


def run_job(job: dict, generator: str, force: bool, dry_run: bool) -> tuple[str, str, str]:
    name = str(job.get("name") or job.get("out") or "?")
    if not job.get("prompt") or not job.get("out"):
        return name, "FAIL", "job needs prompt + out"
    cmd = build_cmd(job, generator, force)
    if dry_run:
        return name, "dry-run", " ".join(cmd)
    result = subprocess.run(cmd, capture_output=True, text=True)
    tail = (result.stdout + result.stderr).strip().splitlines()
    detail = tail[-1] if tail else ""
    if "SKIP" in (result.stdout or ""):
        return name, "skip", detail
    return name, ("ok" if result.returncode == 0 else "FAIL"), detail


def main() -> int:
    ap = argparse.ArgumentParser(description="Concurrent batch image generation with skip-if-exists.")
    ap.add_argument("--jobs", required=True, help="JSON array (or {jobs:[...]}) of generate_image jobs")
    ap.add_argument("--concurrency", type=int, default=3, help="max generations in flight (rate-limit safe)")
    ap.add_argument("--force", action="store_true", help="regenerate every job even if unchanged")
    ap.add_argument("--dry-run", action="store_true", help="print the planned commands, generate nothing")
    ap.add_argument("--generator", default=DEFAULT_GENERATOR)
    a = ap.parse_args()

    data = json.load(open(a.jobs, encoding="utf-8"))
    jobs = data.get("jobs", []) if isinstance(data, dict) else data
    if not jobs:
        print("gen_batch: no jobs")
        return 0

    mode = "dry-run" if a.dry_run else f"run x{a.concurrency}"
    print(f"gen_batch: {len(jobs)} jobs, {mode}")
    results: list[tuple[str, str]] = []
    with ThreadPoolExecutor(max_workers=max(1, a.concurrency)) as pool:
        futures = [pool.submit(run_job, job, a.generator, a.force, a.dry_run) for job in jobs]
        for future in as_completed(futures):
            name, status, detail = future.result()
            results.append((name, status))
            print(f"  [{status}] {name}: {detail}")

    counts: dict[str, int] = {}
    for _, status in results:
        counts[status] = counts.get(status, 0) + 1
    print(f"done: {counts}")
    return 1 if counts.get("FAIL") else 0


if __name__ == "__main__":
    raise SystemExit(main())
