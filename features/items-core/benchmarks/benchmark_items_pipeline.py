#!/usr/bin/env python3
"""Measure the finished Items authoring/build/runtime pipeline."""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
CLI = ROOT / "features" / "items-core" / "scripts" / "items_cli.py"
DEFAULT_PROJECT = ROOT / "templates" / "template"
DEFAULT_EDIT_PROJECT = ROOT / "features" / "items-core" / "tests" / "fixtures" / "items_cli"
BENCHMARK_SOURCES = {
    "benchmark": Path(__file__).resolve(),
    "cli": CLI,
    "evaluator": ROOT / "features" / "items-core" / "scripts" / "items_lua_sandbox.py",
    "runtime_bind": ROOT / "features" / "items-core" / "benchmarks" / "items_runtime_bind_benchmark.c",
}
LOGICAL_READS = {
    "source": 4,
    "preview": 12,
    "apply": 23,
    "build": 5,
    "validate": 5,
    "inspect": 3,
    "conflict": 4,
    "undo": 23,
}


def _descendants(parent_by_pid: dict[int, int], root_pid: int) -> set[int]:
    selected = {root_pid}
    changed = True
    while changed:
        changed = False
        for pid, parent in parent_by_pid.items():
            if pid not in selected and parent in selected:
                selected.add(pid)
                changed = True
    return selected


def _linux_process_tree_rss(root_pid: int) -> int:
    parent_by_pid: dict[int, int] = {}
    rss_by_pid: dict[int, int] = {}
    page_size = os.sysconf("SC_PAGE_SIZE")
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text(encoding="ascii")
            fields = stat.rsplit(") ", 1)[1].split()
            parent_by_pid[int(entry.name)] = int(fields[1])
            rss_pages = int((entry / "statm").read_text(encoding="ascii").split()[1])
            rss_by_pid[int(entry.name)] = rss_pages * page_size
        except (FileNotFoundError, PermissionError, ProcessLookupError, ValueError, IndexError):
            continue
    return sum(rss_by_pid.get(pid, 0) for pid in _descendants(parent_by_pid, root_pid))


def _windows_process_tree_rss(root_pid: int) -> int:
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)

    class ProcessEntry32(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
            ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260),
        ]

    class ProcessMemoryCounters(ctypes.Structure):
        _fields_ = [
            ("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
            ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
            ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
            ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
            ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t),
        ]

    kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry32)]
    kernel32.Process32FirstW.restype = wintypes.BOOL
    kernel32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry32)]
    kernel32.Process32NextW.restype = wintypes.BOOL
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    psapi.GetProcessMemoryInfo.argtypes = [
        wintypes.HANDLE, ctypes.POINTER(ProcessMemoryCounters), wintypes.DWORD,
    ]
    psapi.GetProcessMemoryInfo.restype = wintypes.BOOL

    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
    if snapshot == wintypes.HANDLE(-1).value:
        return 0
    parent_by_pid: dict[int, int] = {}
    try:
        entry = ProcessEntry32()
        entry.dwSize = ctypes.sizeof(entry)
        present = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while present:
            parent_by_pid[int(entry.th32ProcessID)] = int(entry.th32ParentProcessID)
            present = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)

    total = 0
    for pid in _descendants(parent_by_pid, root_pid):
        process = kernel32.OpenProcess(0x0400 | 0x0010, False, pid)
        if not process:
            continue
        try:
            counters = ProcessMemoryCounters()
            counters.cb = ctypes.sizeof(counters)
            if psapi.GetProcessMemoryInfo(process, ctypes.byref(counters), counters.cb):
                total += int(counters.WorkingSetSize)
        finally:
            kernel32.CloseHandle(process)
    return total


def _process_tree_rss(root_pid: int) -> int:
    if os.name == "nt":
        return _windows_process_tree_rss(root_pid)
    if Path("/proc").is_dir():
        return _linux_process_tree_rss(root_pid)
    return 0


def measure_command(
    command: list[str], *, cwd: Path = ROOT, timeout: float = 120.0,
) -> dict[str, Any]:
    """Run one command and sample aggregate RSS for it and its descendants."""
    started = time.perf_counter_ns()
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + timeout
    peak_rss = 0
    while process.poll() is None:
        peak_rss = max(peak_rss, _process_tree_rss(process.pid))
        if time.monotonic() >= deadline:
            process.kill()
            process.communicate()
            raise subprocess.TimeoutExpired(command, timeout)
        time.sleep(0.005)
    stdout, stderr = process.communicate()
    peak_rss = max(peak_rss, _process_tree_rss(process.pid))
    return {
        "exit_code": process.returncode,
        "wall_ms": round((time.perf_counter_ns() - started) / 1_000_000, 3),
        "peak_process_tree_rss_bytes": peak_rss,
        "stdout_bytes": len(stdout),
        "stderr_bytes": len(stderr),
        "stdout": stdout,
        "stderr": stderr,
    }


def conflict_quality(stderr: bytes) -> dict[str, bool]:
    try:
        payload = json.loads(stderr)
    except (json.JSONDecodeError, UnicodeDecodeError):
        payload = {}
    error = payload.get("error", {}) if isinstance(payload, dict) else {}
    return {
        "structured_json": payload.get("schema") == "items.cli.error.v1" if isinstance(payload, dict) else False,
        "stable_code": error.get("code") == "edit.conflict" if isinstance(error, dict) else False,
        "actionable_message": "hash" in str(error.get("message", "")).lower() if isinstance(error, dict) else False,
        "has_expected_actual": isinstance(error, dict) and all(key in error for key in ("expected", "actual")),
    }


def ratify_backend(
    snapshot: dict[str, Any], build: dict[str, Any], noop: dict[str, Any],
    runtime: dict[str, Any], *, lupa_version: str,
) -> dict[str, str]:
    evaluator = snapshot.get("evaluator", {})
    changed = noop.get("result", {}).get("changed", {})
    runtime_result = runtime.get("result", {})
    proven = (
        evaluator.get("module") == "lupa.lua54"
        and evaluator.get("version") == "5.4"
        and evaluator.get("package") in {None, "lupa@2.8"}
        and lupa_version == "2.8"
        and build.get("exit_code") == 0 and build.get("result", {}).get("ok") is True
        and noop.get("exit_code") == 0 and noop.get("result", {}).get("ok") is True
        and changed == {"snapshot": False, "blob": False, "header": False}
        and runtime.get("exit_code") == 0
        and runtime_result.get("bind_samples", 0) > 0
        and runtime_result.get("steady_owned_bytes", 0) > 0
    )
    return {
        "status": "ratified" if proven else "unresolved",
        "backend": "lupa.lua54",
        "lua": "5.4",
        "package": f"lupa@{lupa_version}",
        "runtime_format": "compact-blob-v2",
    }


def _payload(measured: dict[str, Any], *, allow_stderr: bool = False) -> dict[str, Any]:
    encoded = measured["stderr"] if allow_stderr else measured["stdout"]
    try:
        return json.loads(encoded)
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise RuntimeError(f"command returned invalid JSON: {error}") from error


def _public_measurement(measured: dict[str, Any], operation: str) -> dict[str, Any]:
    return {
        "operation": operation,
        **{key: measured[key] for key in (
            "exit_code", "wall_ms", "peak_process_tree_rss_bytes",
            "stdout_bytes", "stderr_bytes",
        )},
    }


def _cli(project: Path, *arguments: str) -> dict[str, Any]:
    return measure_command([
        sys.executable, str(CLI), "--project-root", str(project), *arguments,
    ])


def _require_success(measured: dict[str, Any], operation: str) -> dict[str, Any]:
    if measured["exit_code"] != 0:
        message = measured["stderr"].decode("utf-8", errors="replace")
        raise RuntimeError(f"{operation} failed: {message}")
    return _payload(measured)


def _edit_args(patch: dict[str, Any], *, apply: bool = False) -> list[str]:
    operation = patch["operation"]
    arguments = [
        operation, "--item", patch["item"], "--field", patch["field"],
        "--value", str(patch["value"]),
        "--expected-source-hash", patch["expected_source_hash"],
    ]
    if operation == "curve-set":
        arguments.extend(["--parameter", patch["parameter"]])
    else:
        arguments.extend(["--level", str(patch["level"])])
    if apply:
        arguments.append("--apply")
    return arguments


def _runtime_executable(build_dir: Path) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    candidates = [
        build_dir / "benchmarks" / f"benchmark_items_runtime_bind{suffix}",
        build_dir / "benchmarks" / "Release" / f"benchmark_items_runtime_bind{suffix}",
    ]
    executable = next((path for path in candidates if path.is_file()), None)
    if executable is None:
        raise RuntimeError("runtime bind benchmark executable was not produced")
    return executable


def _agent_totals(commands: list[dict[str, Any]]) -> dict[str, int | float]:
    return {
        "tool_count": len(commands),
        "logical_project_file_reads": sum(LOGICAL_READS[command["operation"]] for command in commands),
        "wall_ms": round(sum(command["wall_ms"] for command in commands), 3),
        "stdout_context_bytes": sum(command["stdout_bytes"] for command in commands),
        "stderr_context_bytes": sum(command["stderr_bytes"] for command in commands),
        "peak_process_tree_rss_bytes": max(command["peak_process_tree_rss_bytes"] for command in commands),
    }


def benchmark(project: Path, edit_project: Path, build_dir: Path, *, warm_runs: int = 3) -> dict[str, Any]:
    project = project.resolve()
    edit_project = edit_project.resolve()
    build_dir = build_dir.resolve()
    if warm_runs < 2 or warm_runs > 9:
        raise ValueError("warm_runs must be between 2 and 9")
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file() or "CMAKE_BUILD_TYPE:STRING=Release" not in cache.read_text(encoding="utf-8"):
        raise RuntimeError("build_dir must be a configured single-config Release build")

    cold_validate = _cli(project, "validate")
    _require_success(cold_validate, "cold validate")
    warm_validate = [_cli(project, "validate") for _ in range(warm_runs)]
    for measured in warm_validate:
        _require_success(measured, "warm validate")

    with tempfile.TemporaryDirectory(prefix="items-pipeline-", dir=build_dir) as temporary:
        temporary_root = Path(temporary)
        output_dir = temporary_root / "production-output"
        cold_build = _cli(project, "build", "--out-dir", str(output_dir))
        cold_build_payload = _require_success(cold_build, "cold build")
        noop_build = _cli(project, "build", "--out-dir", str(output_dir))
        noop_build_payload = _require_success(noop_build, "no-op build")
        snapshot = json.loads((output_dir / "items.snapshot.json").read_text(encoding="utf-8"))

        edit_copy = temporary_root / "editable-project"
        shutil.copytree(edit_project, edit_copy)
        edit_output = temporary_root / "edit-output"
        agent: list[dict[str, Any]] = []

        source = _cli(edit_copy, "source", "--item", "game.levelled_sword")
        source_payload = _require_success(source, "source")
        agent.append(_public_measurement(source, "source"))
        source_hash = source_payload["result"]["source_hash"]
        edit = {
            "schema": "items.cli.patch.v1", "operation": "level-set",
            "item": "game.levelled_sword", "field": "attack", "level": 2,
            "value": 17, "expected_source_hash": source_hash,
        }

        preview = _cli(edit_copy, *_edit_args(edit))
        _require_success(preview, "preview")
        agent.append(_public_measurement(preview, "preview"))
        applied = _cli(edit_copy, *_edit_args(edit, apply=True))
        applied_payload = _require_success(applied, "apply")
        if not applied_payload["result"]["applied"]:
            raise RuntimeError("one-edit flow did not change the temporary Lua source")
        agent.append(_public_measurement(applied, "apply"))

        edit_build = _cli(edit_copy, "build", "--out-dir", str(edit_output))
        _require_success(edit_build, "one-edit build")
        agent.append(_public_measurement(edit_build, "build"))
        validate = _cli(edit_copy, "validate", "--affected", "game.levelled_sword")
        _require_success(validate, "affected validate")
        agent.append(_public_measurement(validate, "validate"))
        inspect = _cli(
            edit_copy, "inspect", "--item", "game.levelled_sword",
            "--level-from", "2", "--level-to", "2",
        )
        _require_success(inspect, "inspect")
        agent.append(_public_measurement(inspect, "inspect"))

        conflict_edit = {**edit, "expected_source_hash": "sha256:" + "0" * 64}
        conflict = _cli(edit_copy, *_edit_args(conflict_edit))
        if conflict["exit_code"] == 0:
            raise RuntimeError("stale edit unexpectedly succeeded")
        diagnostic_quality = conflict_quality(conflict["stderr"])
        if not all(diagnostic_quality.values()):
            raise RuntimeError("conflict diagnostic is not structured and actionable")
        agent.append(_public_measurement(conflict, "conflict"))

        inverse = applied_payload["result"]["inverse_patch"]
        undo = _cli(edit_copy, *_edit_args(inverse, apply=True))
        undo_payload = _require_success(undo, "undo")
        if not undo_payload["result"]["applied"]:
            raise RuntimeError("returned inverse patch did not restore the temporary source")
        agent.append(_public_measurement(undo, "undo"))

        native_prepare = measure_command([
            "cmake", "--build", str(build_dir), "--target", "benchmark_items_runtime_bind",
            "--config", "Release",
        ])
        if native_prepare["exit_code"] != 0:
            message = native_prepare["stderr"].decode("utf-8", errors="replace")
            raise RuntimeError(f"runtime benchmark build failed: {message}")
        runtime = measure_command([str(_runtime_executable(build_dir)), str(output_dir / "items.catalog")])
        runtime_payload = _require_success(runtime, "runtime bind")

        build_evidence = {"exit_code": cold_build["exit_code"], "result": cold_build_payload["result"]}
        noop_evidence = {"exit_code": noop_build["exit_code"], "result": noop_build_payload["result"]}
        runtime_evidence = {"exit_code": runtime["exit_code"], "result": runtime_payload}
        backend = ratify_backend(
            snapshot, build_evidence, noop_evidence, runtime_evidence,
            lupa_version=importlib.metadata.version("lupa"),
        )

        warm_wall = [measurement["wall_ms"] for measurement in warm_validate]
        flows = {
            "cold_evaluate_validate": _public_measurement(cold_validate, "validate"),
            "warm_evaluate_validate": {
                "runs": warm_runs,
                "median_wall_ms": round(statistics.median(warm_wall), 3),
                "samples_wall_ms": warm_wall,
                "max_peak_process_tree_rss_bytes": max(
                    measurement["peak_process_tree_rss_bytes"] for measurement in warm_validate
                ),
            },
            "cold_build": {
                **_public_measurement(cold_build, "build"),
                "changed": cold_build_payload["result"]["changed"],
            },
            "noop_build": {
                **_public_measurement(noop_build, "build"),
                "changed": noop_build_payload["result"]["changed"],
            },
            "one_edit_apply": next(command for command in agent if command["operation"] == "apply"),
            "one_edit_build": next(command for command in agent if command["operation"] == "build"),
            "runtime_bind": {
                **_public_measurement(runtime, "runtime-bind"),
                **runtime_payload,
            },
        }
        bottleneck_candidates = {
            "cold evaluate/validate": flows["cold_evaluate_validate"]["wall_ms"],
            "warm evaluate/validate": flows["warm_evaluate_validate"]["median_wall_ms"],
            "cold build": flows["cold_build"]["wall_ms"],
            "no-op build": flows["noop_build"]["wall_ms"],
            "one-edit apply": flows["one_edit_apply"]["wall_ms"],
            "one-edit build": flows["one_edit_build"]["wall_ms"],
            "runtime bind process": flows["runtime_bind"]["wall_ms"],
        }
        blob_bytes = (output_dir / "items.catalog").stat().st_size

    try:
        project_label = str(project.relative_to(ROOT))
    except ValueError:
        project_label = str(project)
    return {
        "schema": "items.pipeline.benchmark.v1",
        "platform": {
            "system": platform.system(), "release": platform.release(),
            "machine": platform.machine(), "python": platform.python_version(),
        },
        "fixture": {
            "production_project": project_label,
            "items": len(snapshot["items"]), "fields": len(snapshot["fields"]),
            "levels": sum(len(item.get("levels", {}).get("rows", [])) for item in snapshot["items"]),
            "blob_bytes": blob_bytes,
        },
        "flows": flows,
        "agent_scenario": {
            "name": "source-preview-apply-build-validate-inspect-conflict-undo",
            "commands": agent,
            "totals": _agent_totals(agent),
            "diagnostic_quality": diagnostic_quality,
        },
        "bottlenecks": [
            {"flow": name, "wall_ms": wall_ms}
            for name, wall_ms in sorted(bottleneck_candidates.items(), key=lambda entry: entry[1], reverse=True)
        ],
        "decision": backend,
        "budgets": {
            "status": "advisory", "enforced": False,
            "note": "No performance threshold becomes a gate without explicit acceptance.",
        },
        "method": {
            "cold": "first fresh evaluator process and empty build output; OS caches are not flushed",
            "warm": f"median of {warm_runs} later fresh evaluator processes with ordinary OS caches",
            "memory": "5 ms samples of aggregate resident bytes for the command process and descendants",
            "context": "exact captured stdout/stderr byte counts",
            "reads": "deterministic application-level project reads; excludes interpreter and OS reads",
            "source_sha256": {
                name: "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
                for name, path in BENCHMARK_SOURCES.items()
            },
        },
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--edit-project", type=Path, default=DEFAULT_EDIT_PROJECT)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--warm-runs", type=int, default=3)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        report = benchmark(args.project_root, args.edit_project, args.build_dir, warm_runs=args.warm_runs)
        output = args.out.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps({"schema": report["schema"], "out": str(output), "decision": report["decision"]}, sort_keys=True))
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(json.dumps({"error": str(error)}), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
