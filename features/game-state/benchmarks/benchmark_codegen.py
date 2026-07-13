#!/usr/bin/env python3
"""Measure game-state codegen locally without creating a cross-machine CI gate."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

BENCHMARK_DIR = Path(__file__).resolve().parent
SCRIPT_DIR = BENCHMARK_DIR.parent / "scripts"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from state_codegen.model import build_model
from state_codegen.naming import provenance_label
from state_codegen.output import write_bundle
from state_codegen.schema import load_schema

ROOT = BENCHMARK_DIR.parents[2]
DEFAULT_FIXTURE = BENCHMARK_DIR / "fixtures" / "multi_fragment.schema.json"
DEFAULT_BASELINE = BENCHMARK_DIR / "baseline.json"
WARMUPS = 3
WARM_RUNS = 25
COLD_RUNS = 3
ADVISORY_REGRESSION = 0.15


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]


def materialize_fixture(fixture_path: Path, root: Path) -> list[Path]:
    fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
    if fixture.get("fixture_version") != 1 or not isinstance(fixture.get("fragments"), list):
        raise SystemExit("benchmark fixture must contain fixture_version=1 and fragments[]")
    state_dir = root / "state"
    state_dir.mkdir(parents=True, exist_ok=True)
    paths = []
    for schema in fixture["fragments"]:
        fragment = schema.get("fragment")
        if not isinstance(fragment, str) or not fragment:
            raise SystemExit("each benchmark fragment requires a non-empty fragment id")
        path = state_dir / f"{fragment}.schema.json"
        path.write_text(json.dumps(schema, indent=2) + "\n", encoding="utf-8")
        paths.append(path)
    return paths


def generate_fixture(schema_paths: list[Path], output_root: Path, provenance_root: Path) -> None:
    for schema_path in schema_paths:
        schema = load_schema(schema_path)
        model = build_model(schema, provenance_label(schema_path, provenance_root))
        write_bundle(model, output_root / schema["fragment"])


def one_process_run(fixture_path: Path) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        schemas = materialize_fixture(fixture_path, root)
        generate_fixture(schemas, root / "generated", root)


def elapsed_ms(callable_) -> float:
    start = time.perf_counter()
    callable_()
    return (time.perf_counter() - start) * 1000.0


def cold_run_ms(fixture_path: Path) -> float:
    command = [sys.executable, str(Path(__file__).resolve()), "--single-run", "--fixture", str(fixture_path)]
    start = time.perf_counter()
    result = subprocess.run(command, cwd=ROOT, check=False, capture_output=True, text=True)
    elapsed = (time.perf_counter() - start) * 1000.0
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout or "cold benchmark subprocess failed")
    return elapsed


def benchmark(fixture_path: Path) -> dict[str, object]:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        schemas = materialize_fixture(fixture_path, root)
        run = lambda: generate_fixture(schemas, root / "generated", root)
        for _ in range(WARMUPS):
            run()
        warm = [elapsed_ms(run) for _ in range(WARM_RUNS)]
    cold = [cold_run_ms(fixture_path) for _ in range(COLD_RUNS)]
    return {
        "fixture": fixture_path.relative_to(ROOT).as_posix(),
        "warmups": WARMUPS,
        "warm_runs": WARM_RUNS,
        "cold_runs": COLD_RUNS,
        "warm_median_ms": statistics.median(warm),
        "warm_p90_ms": percentile(warm, 0.90),
        "cold_ms": cold,
    }


def baseline_contract(fixture_path: Path) -> dict[str, object]:
    return {
        "fixture": fixture_path.relative_to(ROOT).as_posix(),
        "fixture_sha256": hashlib.sha256(fixture_path.read_bytes()).hexdigest(),
        "python": platform.python_version(),
        "platform": platform.platform(),
        "warmups": WARMUPS,
        "warm_runs": WARM_RUNS,
        "cold_runs": COLD_RUNS,
    }


def load_baseline(path: Path, expected: dict[str, object]) -> tuple[float | None, str]:
    if not path.exists():
        return None, "missing"
    baseline = json.loads(path.read_text(encoding="utf-8"))
    if any(baseline.get(key) != value for key, value in expected.items()):
        return None, "incompatible"
    value = baseline.get("warm_median_ms")
    if not isinstance(value, (int, float)):
        return None, "invalid"
    return float(value), "compatible"


def write_baseline(path: Path, fixture_path: Path, result: dict[str, object]) -> None:
    baseline = {
        **baseline_contract(fixture_path),
        "warm_median_ms": round(float(result["warm_median_ms"]), 3),
        "advisory_threshold_percent": 15,
    }
    path.write_text(json.dumps(baseline, indent=2) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT_FIXTURE)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--update-baseline", action="store_true")
    parser.add_argument("--single-run", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    fixture_path = args.fixture.resolve()
    if args.single_run:
        one_process_run(fixture_path)
        return 0

    result = benchmark(fixture_path)
    if args.update_baseline:
        write_baseline(args.baseline, fixture_path, result)
    baseline, baseline_status = load_baseline(args.baseline, baseline_contract(fixture_path))
    result["baseline_median_ms"] = baseline
    result["baseline_status"] = baseline_status
    if baseline is None:
        result["advisory"] = "local baseline missing or incompatible; run with --update-baseline"
    else:
        regression = float(result["warm_median_ms"]) / baseline - 1.0
        result["regression_percent"] = regression * 100.0
        result["advisory"] = "investigate locally" if regression > ADVISORY_REGRESSION else "none"
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
