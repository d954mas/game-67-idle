#!/usr/bin/env python3
"""Measure equivalent generated-C and compact-blob Items runtime candidates."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Any
import zlib


ROOT = Path(__file__).resolve().parents[3]
SCRIPTS = ROOT / "features" / "items-core" / "scripts"
ARRAY_FIXTURE = ROOT / "features" / "items-core" / "tests" / "fixtures" / "items_runtime_benchmark_arrays.json"
BLOB_FIXTURE = ROOT / "features" / "items-core" / "tests" / "fixtures" / "items_runtime_snapshot_v1.json"
TARGETS = ("benchmark_items_c_arrays", "benchmark_items_runtime_blob")


def _run(command: list[str], cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command, cwd=cwd, text=True, capture_output=True, encoding="utf-8", timeout=120,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout or f"command failed: {command[0]}")
    return result


def _elapsed_ms(command: list[str]) -> tuple[float, subprocess.CompletedProcess[str]]:
    start = time.perf_counter_ns()
    result = _run(command)
    return (time.perf_counter_ns() - start) / 1_000_000.0, result


def _executable(build_dir: Path, target: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    return build_dir / "benchmarks" / f"{target}{suffix}"


def _clean_candidate_outputs(build_dir: Path, target: str) -> None:
    object_dir = (build_dir / "CMakeFiles" / f"{target}.dir").resolve()
    resolved_build = build_dir.resolve()
    if resolved_build not in object_dir.parents:
        raise RuntimeError("candidate object directory escaped build root")
    if object_dir.exists():
        shutil.rmtree(object_dir)
    executable = _executable(build_dir, target)
    if executable.exists():
        executable.unlink()


def _build_metrics(build_dir: Path, target: str) -> tuple[dict[str, Any], Path]:
    _clean_candidate_outputs(build_dir, target)
    command = ["cmake", "--build", str(build_dir), "--target", target, "--config", "Release"]
    compile_link_ms, _ = _elapsed_ms(command)
    executable = _executable(build_dir, target)
    if not executable.is_file():
        raise RuntimeError(f"benchmark target did not produce {executable}")
    executable.unlink()
    link_ms, _ = _elapsed_ms(command)
    noop_ms, _ = _elapsed_ms(command)
    runtime = json.loads(_run([str(executable)]).stdout)
    return ({
        "compile_link_ms": round(compile_link_ms, 3),
        "link_ms": round(link_ms, 3),
        "noop_build_ms": round(noop_ms, 3),
        "executable_bytes": executable.stat().st_size,
        "runtime": runtime,
    }, executable)


def _section_sizes(executable: Path) -> dict[str, int]:
    tool = shutil.which("llvm-size")
    if tool is None:
        return {}
    result = _run([tool, "-A", str(executable)])
    sections: dict[str, int] = {}
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith(".") and parts[1].isdigit():
            sections[parts[0]] = int(parts[1])
    return sections


def _cmake_cache(build_dir: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in (build_dir / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line or ":" not in line:
            continue
        key_type, value = line.split("=", 1)
        key, _ = key_type.split(":", 1)
        values[key] = value
    return values


def _snapshot_hash(snapshot: dict[str, Any]) -> str:
    payload = {
        key: snapshot[key]
        for key in ("schema", "fields", "items", "requirements")
    }
    encoded = json.dumps(
        payload, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
    ).encode("utf-8")
    return "sha256:" + hashlib.sha256(encoded).hexdigest()


def _proof_transition(value: Any) -> Any:
    if value is None:
        return None
    if value.get("kind") == "free":
        return "free"
    return sorted((entry["item_ref"], entry["count"]) for entry in value["cost"])


def _blob_transition(value: Any) -> Any:
    if value is None:
        return None
    if value.get("__studio_kind") == "free":
        return "free"
    if value.get("__studio_kind") == "cost":
        return [(value["item"]["id"], value["count"])]
    raise ValueError("unknown normalized blob transition")


def fixture_signatures() -> tuple[dict[str, Any], dict[str, Any]]:
    arrays = json.loads(ARRAY_FIXTURE.read_text(encoding="utf-8"))
    blob = json.loads(BLOB_FIXTURE.read_text(encoding="utf-8"))
    arrays_signature = {
        "fields": [
            (field["field_id"], field["member"], field["min"], field["max"])
            for field in arrays["fields"]
        ],
        "items": [
            {
                "id": item["def_id"], "kind": item["kind"], "stack": item["stack"],
                "acquire": _proof_transition(item.get("acquire")),
                "levels": [
                    (row["attack"], _proof_transition(row.get("cost_to_reach")))
                    for row in item["levels"]
                ],
            }
            for item in arrays["items"]
        ],
    }
    blob_signature = {
        "fields": [
            (field["id"], field["member"], field["min"], field["max"])
            for field in blob["fields"]
        ],
        "items": [
            {
                "id": item["id"], "kind": item["kind"], "stack": item["stack"],
                "acquire": _blob_transition(item.get("acquire", {}).get("cost")),
                "levels": [
                    (row["attack"], _blob_transition(row.get("cost_to_reach")))
                    for row in item.get("levels", {}).get("rows", [])
                ],
            }
            for item in blob["items"]
        ],
    }
    return arrays_signature, blob_signature


def _generation_metrics(build_dir: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="items-benchmark-", dir=build_dir) as temporary:
        root = Path(temporary)
        arrays_out = root / "arrays"
        arrays_command = [
            sys.executable, str(SCRIPTS / "generate_items_api_proof.py"),
            "--snapshot", str(ARRAY_FIXTURE), "--out-dir", str(arrays_out),
        ]
        arrays_cold_ms, _ = _elapsed_ms(arrays_command)
        arrays_noop_ms, _ = _elapsed_ms(arrays_command)
        arrays_before = {
            path.name: path.read_bytes()
            for path in arrays_out.iterdir() if path.is_file()
        }

        arrays_changed = json.loads(ARRAY_FIXTURE.read_text(encoding="utf-8"))
        arrays_changed["items"][1]["levels"][0]["attack"] += 1
        arrays_changed_path = root / "arrays-changed.json"
        arrays_changed_path.write_text(json.dumps(arrays_changed), encoding="utf-8")
        arrays_value_ms, _ = _elapsed_ms([
            sys.executable, str(SCRIPTS / "generate_items_api_proof.py"),
            "--snapshot", str(arrays_changed_path), "--out-dir", str(arrays_out),
        ])
        arrays_after = {
            path.name: path.read_bytes()
            for path in arrays_out.iterdir() if path.is_file()
        }
        arrays_changed_files = sorted(
            name for name in arrays_before if arrays_before[name] != arrays_after[name]
        )

        blob_out = root / "items.catalog"
        header_out = root / "items_catalog_abi.gen.h"
        blob_command = [
            sys.executable, str(SCRIPTS / "items_runtime_package.py"), "build",
            "--snapshot", str(BLOB_FIXTURE), "--out", str(blob_out),
            "--header-out", str(header_out),
        ]
        blob_cold_ms, _ = _elapsed_ms(blob_command)
        blob_noop_ms, _ = _elapsed_ms(blob_command)
        header_before = header_out.read_bytes()
        blob_changed = json.loads(BLOB_FIXTURE.read_text(encoding="utf-8"))
        blob_changed["items"][1]["levels"]["rows"][0]["attack"] += 1
        blob_changed["content_hash"] = _snapshot_hash(blob_changed)
        blob_changed_path = root / "blob-changed.json"
        blob_changed_path.write_text(json.dumps(blob_changed), encoding="utf-8")
        blob_value_ms, blob_value = _elapsed_ms([
            sys.executable, str(SCRIPTS / "items_runtime_package.py"), "build",
            "--snapshot", str(blob_changed_path), "--out", str(blob_out),
            "--header-out", str(header_out),
        ])

        arrays_source = arrays_after["items_game.gen.c"]
        blob_bytes = blob_out.read_bytes()
        return {
            "c_arrays": {
                "cold_ms": round(arrays_cold_ms, 3),
                "noop_ms": round(arrays_noop_ms, 3),
                "value_edit_ms": round(arrays_value_ms, 3),
                "value_edit_changed_files": arrays_changed_files,
                "raw_data_source_bytes": len(arrays_source),
                "zlib_data_source_bytes": len(zlib.compress(arrays_source, level=9)),
                "relink_required_on_value_edit": "items_game.gen.c" in arrays_changed_files,
            },
            "blob": {
                "cold_ms": round(blob_cold_ms, 3),
                "noop_ms": round(blob_noop_ms, 3),
                "value_edit_ms": round(blob_value_ms, 3),
                "value_edit_changed": json.loads(blob_value.stdout)["changed"],
                "header_stable_on_value_edit": header_before == header_out.read_bytes(),
                "raw_blob_bytes": len(blob_bytes),
                "zlib_blob_bytes": len(zlib.compress(blob_bytes, level=9)),
                "relink_required_on_value_edit": header_before != header_out.read_bytes(),
            },
        }


def benchmark(build_dir: Path) -> dict[str, Any]:
    build_dir = build_dir.resolve()
    if not (build_dir / "CMakeCache.txt").is_file():
        raise RuntimeError("build directory must be configured by CMake")
    cache = _cmake_cache(build_dir)
    if cache.get("CMAKE_BUILD_TYPE") != "Release":
        raise RuntimeError("benchmark requires a single-config CMake Release build")
    arrays_signature, blob_signature = fixture_signatures()
    if arrays_signature != blob_signature:
        raise RuntimeError("benchmark candidates do not describe equivalent catalog content")
    generation = _generation_metrics(build_dir)
    candidates: dict[str, Any] = {}
    for target in TARGETS:
        metrics, executable = _build_metrics(build_dir, target)
        candidate = "c_arrays" if target.endswith("c_arrays") else "blob"
        candidates[candidate] = metrics
        sections = _section_sizes(executable)
        metrics["image_sections"] = sections
        metrics["selected_code_data_section_bytes"] = sum(
            sections.get(name, 0) for name in (".text", ".rdata", ".data", ".bss")
        ) if sections else None
    arrays_runtime = candidates["c_arrays"]["runtime"]
    blob_runtime = candidates["blob"]["runtime"]
    for key in ("access_runs", "checksum"):
        if arrays_runtime.get(key) != blob_runtime.get(key):
            raise RuntimeError(f"benchmark runtime parity mismatch: {key}")
    blob_is_default = (
        generation["blob"]["header_stable_on_value_edit"]
        and not generation["blob"]["relink_required_on_value_edit"]
        and generation["blob"]["raw_blob_bytes"] < generation["c_arrays"]["raw_data_source_bytes"]
    )
    return {
        "schema": "items.runtime.benchmark.v1",
        "platform": {
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "build": {
            "type": cache.get("CMAKE_BUILD_TYPE"),
            "generator": cache.get("CMAKE_GENERATOR"),
            "c_compiler": cache.get("CMAKE_C_COMPILER"),
        },
        "fixture": {"items": 2, "fields": 1, "levels": 3, "costs": 2},
        "generation": generation,
        "candidates": candidates,
        "lookup_strategy": "linear stable-hash definition lookup, then checked dense level/value spans",
        "pack_rebuild": {
            "current_pack_blob_requires_repack": True,
            "separate_pack_blob_requires_repack": True,
            "separate_pack_game_pack_requires_rebuild": False,
            "blob_requires_relink": False,
            "c_arrays_require_relink": True,
        },
        "decision": {
            "default": "blob" if blob_is_default else "unresolved",
            "reason": "value-only edits keep the ABI header stable and avoid relinking" if blob_is_default else "measurements did not ratify the provisional default",
        },
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        report = benchmark(args.build_dir)
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(
            json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(json.dumps({
            "schema": report["schema"], "out": str(args.out),
            "decision": report["decision"],
        }, ensure_ascii=False, sort_keys=True))
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(json.dumps({"error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
