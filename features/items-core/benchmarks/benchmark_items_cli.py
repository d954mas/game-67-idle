#!/usr/bin/env python3
"""Bounded T0366 agent edit-loop benchmark."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "items_cli.py"
READS = {"source": 4, "level-set": 12, "validate": 5, "inspect": 3, "conflict": 4}


def _run(project: Path, *arguments: str, read_kind: str) -> tuple[dict, subprocess.CompletedProcess[str]]:
    started = time.perf_counter_ns()
    process = subprocess.run(
        [sys.executable, str(SCRIPT), "--project-root", str(project), *arguments],
        text=False, capture_output=True, timeout=30,
    )
    elapsed = (time.perf_counter_ns() - started) / 1_000_000
    return {
        "operation": arguments[0],
        "wall_ms": round(elapsed, 3),
        "logical_project_file_reads": READS[read_kind],
        "stdout_bytes": len(process.stdout),
        "stderr_bytes": len(process.stderr),
        "exit_code": process.returncode,
    }, process


def benchmark(project: Path) -> dict:
    commands: list[dict] = []
    measured, source = _run(
        project, "source", "--item", "game.levelled_sword", read_kind="source",
    )
    commands.append(measured)
    if source.returncode != 0:
        raise RuntimeError(source.stderr.decode("utf-8", errors="replace"))
    source_hash = json.loads(source.stdout)["result"]["source_hash"]

    measured, preview = _run(
        project, "level-set", "--item", "game.levelled_sword", "--level", "2",
        "--field", "attack", "--value", "17", "--expected-source-hash", source_hash,
        read_kind="level-set",
    )
    commands.append(measured)
    if preview.returncode != 0:
        raise RuntimeError(preview.stderr.decode("utf-8", errors="replace"))

    measured, validate = _run(
        project, "validate", "--affected", "game.levelled_sword", read_kind="validate",
    )
    commands.append(measured)
    if validate.returncode != 0:
        raise RuntimeError(validate.stderr.decode("utf-8", errors="replace"))

    measured, inspect = _run(
        project, "inspect", "--item", "game.levelled_sword",
        "--level-from", "2", "--level-to", "2", read_kind="inspect",
    )
    commands.append(measured)
    if inspect.returncode != 0:
        raise RuntimeError(inspect.stderr.decode("utf-8", errors="replace"))

    stale_hash = "sha256:" + ("0" * 64)
    measured, conflict = _run(
        project, "level-set", "--item", "game.levelled_sword", "--level", "2",
        "--field", "attack", "--value", "17", "--expected-source-hash", stale_hash,
        read_kind="conflict",
    )
    commands.append(measured)
    conflict_payload = json.loads(conflict.stderr)
    diagnostic = conflict_payload.get("error", {})
    quality = {
        "structured_json": conflict_payload.get("schema") == "items.cli.error.v1",
        "stable_code": diagnostic.get("code") == "edit.conflict",
        "actionable_message": "hash" in str(diagnostic.get("message", "")).lower(),
        "has_expected_actual": all(key in diagnostic for key in ("expected", "actual")),
    }
    if conflict.returncode == 0 or not all(quality.values()):
        raise RuntimeError("conflict diagnostic did not satisfy the benchmark contract")

    return {
        "schema": "items.cli.benchmark.v1",
        "scenario": "source-preview-validate-affected-inspect-conflict",
        "command_count": len(commands),
        "commands": commands,
        "totals": {
            "wall_ms": round(sum(command["wall_ms"] for command in commands), 3),
            "logical_project_file_reads": sum(
                command["logical_project_file_reads"] for command in commands
            ),
            "stdout_bytes": sum(command["stdout_bytes"] for command in commands),
            "stderr_bytes": sum(command["stderr_bytes"] for command in commands),
        },
        "diagnostic_quality": quality,
        "method": {
            "latency": "subprocess wall clock; advisory machine sample",
            "file_reads": (
                "deterministic application-level project reads implied by the manifest, "
                "temporary validation copy, receipt and state inputs; excludes OS/library reads"
            ),
            "context_bytes": "exact captured UTF-8 stdout/stderr byte counts",
            "source_sha256": "sha256:" + hashlib.sha256(SCRIPT.read_bytes()).hexdigest(),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    result = benchmark(Path(args.project_root).resolve())
    output = Path(args.out).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
