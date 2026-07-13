#!/usr/bin/env python3
"""Trustworthy reference-template source-edit to DevAPI proof loop."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from devapi_client import (
    DEFAULT_DEVAPI_PORT,
    ROOT,
    DevApiError,
    connect_existing,
    ensure_output_dir,
    pick_free_port,
    running_game,
    write_engine_capture_payload_png,
)


ROOT_PATH = Path(ROOT)
TEMPLATE_ROOT = ROOT_PATH / "templates" / "template"
BUILD_DIR = TEMPLATE_ROOT / "build" / "devapi-debug"
CACHE_PATH = BUILD_DIR / "CMakeCache.txt"
NINJA_LOG = BUILD_DIR / ".ninja_log"
EXE_NAME = "game.exe" if os.name == "nt" else "game"
NATIVE_DEVAPI_EXE = BUILD_DIR / "bin" / EXE_NAME
PACK_PATH = BUILD_DIR / "bin" / "assets" / "game.ntpack"
PROOF_SOURCE = TEMPLATE_ROOT / "src" / "iteration_proof_devapi.c"
STATE_SCHEMA = TEMPLATE_ROOT / "state" / "game_state.schema.json"
PROOF_METHOD = "game.iteration.proof"
SHOT = "tmp/captures/iterate.png"
RESULT_SCHEMA = "ai_studio.runtime_automation.iterate.v2"
ERROR_TAIL_LIMIT = 4096
INVENTORY_LIMIT = 256
SCOPED_GUARD_PATHS = (
    "templates/template",
    "features",
    "ai_studio/runtime_automation",
    "ai_studio/studio.mjs",
)
PHASE_NAMES = (
    "validation",
    "configure",
    "metadata",
    "codegen",
    "compile",
    "link",
    "launchToDevapiReady",
    "semanticProof",
    "captureConsistency",
    "total",
)


class IterationError(RuntimeError):
    pass


class ProcessError(IterationError):
    def __init__(self, phase: str, returncode: int, tail: str, duration_ms: float | None = None, output_bytes: int | None = None):
        super().__init__(f"{phase} failed ({returncode})")
        self.phase = phase
        self.returncode = returncode
        self.tail = tail[-ERROR_TAIL_LIMIT:]
        self.duration_ms = duration_ms
        self.output_bytes = output_bytes if output_bytes is not None else len(tail.encode("utf-8"))


@dataclass(frozen=True)
class ToolResult:
    duration_ms: float
    output_bytes: int
    stdout: str = ""
    stderr: str = ""


def canonical_configure_command(platform_name: str | None = None, *, fresh: bool = False) -> list[str]:
    platform_name = platform_name or sys.platform
    command = ["cmake"]
    if fresh:
        command.append("--fresh")
    command.extend([
        "-S", "templates/template",
        "-B", "templates/template/build/devapi-debug",
        "-G", "Ninja",
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_CXX_COMPILER=clang++",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DGAME_DEVAPI_ENABLED=ON",
    ])
    if platform_name.startswith("linux"):
        command.append("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined")
    return command


def canonical_build_command() -> list[str]:
    return ["cmake", "--build", "templates/template/build/devapi-debug", "--target", "game"]


def _cache_values(cache_path: Path) -> dict[str, str]:
    if not cache_path.is_file():
        return {}
    values: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(("//", "#")) or "=" not in line or ":" not in line:
            continue
        key_type, value = line.split("=", 1)
        key, _ = key_type.split(":", 1)
        values[key] = value
    return values


def _same_path(left: str, right: Path) -> bool:
    try:
        left_path = str(Path(left).resolve())
        right_path = str(right.resolve())
    except OSError:
        return False
    return left_path.casefold() == right_path.casefold() if os.name == "nt" else left_path == right_path


def cache_compatibility(
    cache_path: Path = CACHE_PATH,
    source: Path = TEMPLATE_ROOT,
    build: Path = BUILD_DIR,
    *,
    platform_name: str | None = None,
) -> str:
    values = _cache_values(cache_path)
    if not values:
        return "missing"
    if not _same_path(values.get("CMAKE_HOME_DIRECTORY", ""), source) or not _same_path(
        values.get("CMAKE_CACHEFILE_DIR", ""), build
    ):
        return "foreign"
    required = {
        "GAME_DEVAPI_ENABLED": "ON",
        "CMAKE_GENERATOR": "Ninja",
        "CMAKE_BUILD_TYPE": "Debug",
    }
    if any(values.get(key) != value for key, value in required.items()):
        return "incompatible"
    if "clang" not in values.get("CMAKE_C_COMPILER", "").lower():
        return "incompatible"
    if "clang++" not in values.get("CMAKE_CXX_COMPILER", "").lower():
        return "incompatible"
    platform_name = platform_name or sys.platform
    if platform_name.startswith("linux") and "-fsanitize=address,undefined" not in values.get(
        "CMAKE_EXE_LINKER_FLAGS_DEBUG", ""
    ):
        return "incompatible"
    return "compatible"


def configuration_needs_prepare(cache_path: Path = CACHE_PATH, *, platform_name: str | None = None) -> bool:
    return cache_compatibility(cache_path, platform_name=platform_name) != "compatible"


def read_expected_fixtures(c_path: Path = PROOF_SOURCE, schema_path: Path = STATE_SCHEMA) -> dict[str, str]:
    try:
        c_text = c_path.read_text(encoding="utf-8")
        match = re.search(r'^#define\s+GAME_ITERATION_C_FIXTURE\s+"([^"\r\n]+)"\s*$', c_text, re.MULTILINE)
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        schema_fixture = schema["fields"]["test_label_text"]["default"]
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as exc:
        raise IterationError(f"cannot read iteration fixtures: {exc}") from exc
    if match is None:
        raise IterationError(f"missing GAME_ITERATION_C_FIXTURE in {c_path}")
    if not isinstance(schema_fixture, str) or not schema_fixture:
        raise IterationError(f"missing string test_label_text default in {schema_path}")
    return {"cFixture": match.group(1), "schemaFixture": schema_fixture}


def require_exact_proof(actual: Any, expected: dict[str, str]) -> None:
    if not isinstance(actual, dict):
        raise IterationError("game.iteration.proof returned a non-object")
    for key in ("cFixture", "schemaFixture"):
        if actual.get(key) != expected[key]:
            raise IterationError(f"{key} mismatch: expected {expected[key]!r}, got {actual.get(key)!r}")


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hash_claimed_artifacts() -> dict[str, str]:
    missing = [str(path) for path in (NATIVE_DEVAPI_EXE, PACK_PATH) if not path.is_file()]
    if missing:
        raise IterationError("built artifact missing: " + ", ".join(missing))
    return {"executable": _sha256_file(NATIVE_DEVAPI_EXE), "pack": _sha256_file(PACK_PATH)}


def _run_git_bytes(args: list[str]) -> bytes:
    completed = subprocess.run(["git", *args], cwd=ROOT, check=False, capture_output=True)
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).decode("utf-8", "replace").strip()
        raise IterationError(f"git {' '.join(args)} failed: {detail}")
    return completed.stdout


def _run_git(args: list[str]) -> str:
    return _run_git_bytes(args).decode("utf-8", "replace").strip()


def _scoped_worktree_snapshot() -> dict[str, Any]:
    scope = list(SCOPED_GUARD_PATHS)
    diff = _run_git_bytes(["diff", "--binary", "--no-ext-diff", "HEAD", "--", *scope])
    tracked_raw = _run_git_bytes(["diff", "--name-only", "-z", "--no-ext-diff", "HEAD", "--", *scope])
    untracked_raw = _run_git_bytes(["ls-files", "--others", "--exclude-standard", "-z", "--", *scope])
    tracked = {item.decode("utf-8", "surrogateescape") for item in tracked_raw.split(b"\0") if item}
    untracked = sorted(item.decode("utf-8", "surrogateescape") for item in untracked_raw.split(b"\0") if item)
    digest = hashlib.sha256()
    digest.update(diff)
    digest.update(b"\0<untracked>\0")
    for relative in untracked:
        path = ROOT_PATH / relative
        digest.update(relative.encode("utf-8", "surrogateescape") + b"\0")
        if path.is_file():
            digest.update(path.read_bytes())
        else:
            digest.update(b"<missing>")
        digest.update(b"\0")
    dirty_paths = sorted(tracked | set(untracked))
    return {
        "scopedWorktree": digest.hexdigest(),
        "worktreeScope": scope,
        "dirtyPaths": dirty_paths,
        "dirtyPathCount": len(dirty_paths),
        "_toolCalls": 3,
    }


def read_consistency_guard() -> dict[str, Any]:
    commit = _run_git(["rev-parse", "HEAD"])
    tree = _run_git(["ls-tree", "HEAD", "external/neotolis-engine"]).split()
    engine_gitlink = tree[2] if len(tree) >= 3 and tree[0] == "160000" else None
    engine_status = _run_git([
        "status", "--porcelain=v1", "--ignore-submodules=none", "--", "external/neotolis-engine"
    ])
    snapshot = _scoped_worktree_snapshot()
    tool_calls = 3 + int(snapshot.pop("_toolCalls"))
    return {
        "commit": commit,
        **snapshot,
        "engineGitlink": engine_gitlink,
        "engineDirty": bool(engine_status),
        "_toolCalls": tool_calls,
    }


def run_process(command: list[str], phase: str) -> ToolResult:
    started = time.perf_counter()
    try:
        completed = subprocess.run(command, cwd=ROOT, check=False, capture_output=True, text=True)
    except OSError as exc:
        raise ProcessError(phase, -1, str(exc)) from exc
    elapsed = (time.perf_counter() - started) * 1000.0
    stdout = completed.stdout or ""
    stderr = completed.stderr or ""
    output_bytes = len(stdout.encode("utf-8")) + len(stderr.encode("utf-8"))
    if completed.returncode != 0:
        raise ProcessError(phase, completed.returncode, stdout + stderr, elapsed, output_bytes)
    return ToolResult(elapsed, output_bytes, stdout, stderr)


def collect_tool_metadata(record_process: Callable[[list[str], str], ToolResult]) -> dict[str, str | None]:
    values = _cache_values(CACHE_PATH)
    compiler = values.get("CMAKE_C_COMPILER", "clang")
    cmake = record_process(["cmake", "--version"], "metadata").stdout.splitlines()
    ninja = record_process(["ninja", "--version"], "metadata").stdout.splitlines()
    clang = record_process([compiler, "--version"], "metadata").stdout.splitlines()
    return {
        "cmake": cmake[0] if cmake else None,
        "buildTool": ninja[0] if ninja else None,
        "compiler": clang[0] if clang else None,
        "generator": values.get("CMAKE_GENERATOR"),
        "compilerPath": compiler,
    }


def ninja_log_offset(path: Path = NINJA_LOG) -> int:
    try:
        return path.stat().st_size
    except OSError:
        return 0


def read_ninja_inventory(offset: int, path: Path = NINJA_LOG) -> dict[str, Any]:
    try:
        size = path.stat().st_size
        if size < offset:
            return {"status": "unavailable-log-rewritten", "rebuiltFiles": None, "rebuiltTargets": None}
        with path.open("rb") as handle:
            handle.seek(offset)
            text = handle.read().decode("utf-8", "replace")
    except OSError:
        return {"status": "unavailable", "rebuiltFiles": None, "rebuiltTargets": None}
    files: list[str] = []
    for line in text.splitlines():
        parts = line.split("\t")
        if len(parts) >= 4 and parts[0].isdigit():
            files.append(parts[3].replace("\\", "/"))
    files = sorted(set(files))
    total_files = len(files)
    targets: list[str] = []
    if any(item.endswith(("bin/game", "bin/game.exe")) for item in files):
        targets.append("game")
    if any(item.endswith("game.ntpack") for item in files):
        targets.append("game_asset_packs")
    files = files[:INVENTORY_LIMIT]
    return {
        "status": "available" if total_files <= INVENTORY_LIMIT else "available-truncated",
        "rebuiltFiles": files,
        "rebuiltFileCount": total_files,
        "rebuiltTargets": targets,
    }


def _phase(status: str, wall_ms: float | None) -> dict[str, Any]:
    return {"status": status, "wallMs": wall_ms}


def _new_result(reuse: bool, port: int | None) -> dict[str, Any]:
    skipped = _phase("skipped", 0.0)
    return {
        "schema": RESULT_SCHEMA,
        "status": "running",
        "mode": "attach-only" if reuse else "fresh-build",
        "freshnessClaim": False,
        "phases": {
            name: dict(skipped) if reuse and name not in ("launchToDevapiReady", "total") else _phase("pending", None)
            for name in PHASE_NAMES
        },
        "toolCalls": {"process": 0, "devapi": 0, "guard": 0, "total": 0},
        "outputBytes": {"process": 0, "devapi": 0, "total": 0},
        "devapiMetrics": {"requests": 0, "requestBytes": 0, "normalizedResponseBytes": 0},
        "proof": {"status": "skipped", "reason": "attach-only reuse"} if reuse else None,
        "runtime": {"port": port, "processId": None},
        "consistency": {"start": None, "end": None},
        "artifactHashes": {"afterBuild": None, "afterProof": None, "afterCapture": None},
        "artifacts": {},
        "build": {"status": "skipped" if reuse else "pending", "rebuiltFiles": None, "rebuiltTargets": None},
        "commandPath": {
            "configure": None,
            "build": None if reuse else canonical_build_command(),
            "readiness": {"method": "endpoints", "requires": PROOF_METHOD} if not reuse else {"method": "endpoints"},
            "proof": None if reuse else {"method": PROOF_METHOD},
        },
        "environment": {
            "os": platform.system(),
            "osVersion": platform.release(),
            "machine": platform.machine(),
            "cpu": platform.processor() or os.environ.get("PROCESSOR_IDENTIFIER", "unknown"),
            "python": platform.python_version(),
            "tools": None,
        },
        "error": None,
    }


def _bounded(value: str) -> str:
    return value[-ERROR_TAIL_LIMIT:]


def _same_guard(start: dict[str, Any], current: dict[str, Any]) -> bool:
    if start.get("engineDirty") is not False or current.get("engineDirty") is not False:
        return False
    return all(
        start.get(key) == current.get(key)
        for key in ("commit", "scopedWorktree", "engineGitlink")
    )


def execute_iteration(
    *,
    reuse: bool = False,
    port: int | None = None,
    executable: str | os.PathLike[str] = NATIVE_DEVAPI_EXE,
    capture: bool = True,
    process_runner: Callable[[list[str], str], ToolResult] | None = None,
    game_launcher: Callable[..., Any] | None = None,
    attach_client: Callable[..., Any] | None = None,
    **legacy_ignored: Any,
) -> dict[str, Any]:
    del legacy_ignored
    started_total = time.perf_counter()
    resolved_port = (DEFAULT_DEVAPI_PORT if port is None else port) if reuse else port
    result = _new_result(reuse, resolved_port)
    process_runner = process_runner or run_process
    game_launcher = game_launcher or running_game
    attach_client = attach_client or connect_existing
    current_phase = "validation"
    current_phase_started = started_total

    def set_phase(name: str, status: str, started: float | None = None) -> None:
        wall = None if started is None else (time.perf_counter() - started) * 1000.0
        result["phases"][name] = _phase(status, wall)

    def record_process(command: list[str], phase: str) -> ToolResult:
        try:
            tool = process_runner(command, phase)
        except ProcessError as exc:
            result["toolCalls"]["process"] += 1
            result["outputBytes"]["process"] += exc.output_bytes
            raise
        result["toolCalls"]["process"] += 1
        result["outputBytes"]["process"] += tool.output_bytes
        if phase in result["phases"]:
            result["phases"][phase] = _phase("executed", tool.duration_ms)
        return tool

    def devapi_result(game: Any, method: str, params: dict[str, Any] | None = None) -> Any:
        params = params or {}
        request_id = str(getattr(game, "next_request_id", result["devapiMetrics"]["requests"] + 1))
        payload = {"request_id": request_id, "method": method, "params": params}
        request_bytes = len((json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8"))
        response = game.request(method, params)
        response_bytes = len((json.dumps(response, separators=(",", ":")) + "\n").encode("utf-8"))
        result["toolCalls"]["devapi"] += 1
        result["devapiMetrics"]["requests"] += 1
        result["devapiMetrics"]["requestBytes"] += request_bytes
        result["devapiMetrics"]["normalizedResponseBytes"] += response_bytes
        result["outputBytes"]["devapi"] += response_bytes
        if not isinstance(response, dict) or response.get("ok") is not True:
            raise IterationError(f"{method} failed: {response}")
        return response.get("result")

    def take_guard() -> dict[str, Any]:
        guard = read_consistency_guard()
        result["toolCalls"]["guard"] += int(guard.pop("_toolCalls", 0))
        return guard

    try:
        if not reuse and resolved_port is None:
            resolved_port = pick_free_port()
            result["runtime"]["port"] = resolved_port
        if reuse:
            current_phase = "launchToDevapiReady"
            current_phase_started = time.perf_counter()
            started = time.perf_counter()
            game = attach_client(port=resolved_port, timeout=0.5)
            if game is None:
                raise IterationError(f"attach-only reuse found no DevAPI game on port {resolved_port}")
            try:
                devapi_result(game, "endpoints")
                set_phase(current_phase, "executed", started)
            finally:
                game.close()
            result["status"] = "passed"
            result["consistency"] = {"start": None, "end": None}
        else:
            current_phase = "validation"
            current_phase_started = time.perf_counter()
            started = time.perf_counter()
            if Path(executable).resolve() != NATIVE_DEVAPI_EXE.resolve():
                raise IterationError(f"fresh iteration must launch the canonical built executable: {NATIVE_DEVAPI_EXE}")
            start_guard = take_guard()
            result["consistency"]["start"] = start_guard
            if start_guard.get("engineDirty"):
                raise IterationError("external/neotolis-engine is dirty")
            if not start_guard.get("engineGitlink"):
                raise IterationError("external/neotolis-engine gitlink SHA is unavailable")
            expected = read_expected_fixtures()
            set_phase(current_phase, "executed", started)

            cache_state = cache_compatibility()
            if cache_state != "compatible":
                current_phase = "configure"
                current_phase_started = time.perf_counter()
                command = canonical_configure_command(fresh=cache_state in ("foreign", "incompatible"))
                result["commandPath"]["configure"] = command
                record_process(command, current_phase)
            else:
                result["phases"]["configure"] = _phase("skipped-compatible-cache", 0.0)

            current_phase = "metadata"
            current_phase_started = time.perf_counter()
            started = time.perf_counter()
            result["environment"]["tools"] = collect_tool_metadata(record_process)
            set_phase(current_phase, "executed", started)
            result["phases"]["codegen"] = _phase("included-in-build", None)
            log_offset = ninja_log_offset()
            current_phase = "compile"
            current_phase_started = time.perf_counter()
            record_process(canonical_build_command(), current_phase)
            result["phases"]["link"] = _phase("included-in-build", None)
            result["build"] = read_ninja_inventory(log_offset)
            result["artifactHashes"]["afterBuild"] = hash_claimed_artifacts()

            current_phase = "launchToDevapiReady"
            current_phase_started = time.perf_counter()
            started = time.perf_counter()
            result["commandPath"]["launch"] = [
                str(executable), "--devapi", str(resolved_port), "--fresh-state", "--disable-autosave"
            ]
            with game_launcher(
                port=resolved_port,
                exe=str(executable),
                cwd=ROOT,
                reuse_existing=False,
                fresh_state=True,
                autosave_enabled=False,
            ) as game:
                result["runtime"]["processId"] = getattr(game, "process_id", None)
                if (
                    not isinstance(result["runtime"]["processId"], int)
                    or isinstance(result["runtime"]["processId"], bool)
                    or result["runtime"]["processId"] <= 0
                ):
                    raise IterationError("fresh launch did not retain a positive process PID")
                listing = devapi_result(game, "endpoints")
                methods = {
                    item.get("method") if isinstance(item, dict) else item
                    for item in (listing.get("commands", []) if isinstance(listing, dict) else [])
                }
                if PROOF_METHOD not in methods:
                    raise IterationError(f"endpoints does not contain required {PROOF_METHOD}")
                set_phase(current_phase, "executed", started)

                current_phase = "semanticProof"
                current_phase_started = time.perf_counter()
                started = time.perf_counter()
                actual = devapi_result(game, PROOF_METHOD)
                require_exact_proof(actual, expected)
                result["proof"] = {"status": "passed", "expected": expected, "actual": actual}
                set_phase(current_phase, "executed", started)
                result["artifactHashes"]["afterProof"] = hash_claimed_artifacts()
                if result["artifactHashes"]["afterProof"] != result["artifactHashes"]["afterBuild"]:
                    raise IterationError("built executable or claimed pack changed during proof")

                current_phase = "captureConsistency"
                current_phase_started = time.perf_counter()
                before_capture_fixtures = read_expected_fixtures()
                before_capture_guard = take_guard()
                if before_capture_guard.get("engineDirty") is not False:
                    raise IterationError("external/neotolis-engine became dirty before capture")
                if before_capture_fixtures != expected or not _same_guard(start_guard, before_capture_guard):
                    raise IterationError("source fixtures or scoped worktree changed before capture")

                if capture:
                    devapi_result(game, "frame.wait", {"frames": 1})
                    payload = devapi_result(game, "capture.frame")
                    shot_path = write_engine_capture_payload_png(payload, ensure_output_dir(SHOT))
                    result["artifacts"]["screenshot"] = {"path": str(shot_path), "status": "captured"}

                after_capture_fixtures = read_expected_fixtures()
                end_guard = take_guard()
                if end_guard.get("engineDirty") is not False:
                    raise IterationError("external/neotolis-engine became dirty during capture")
                result["consistency"]["end"] = end_guard
                result["artifactHashes"]["afterCapture"] = hash_claimed_artifacts()
                raced = (
                    after_capture_fixtures != expected
                    or not _same_guard(start_guard, end_guard)
                    or result["artifactHashes"]["afterCapture"] != result["artifactHashes"]["afterProof"]
                )
                if raced:
                    if "screenshot" in result["artifacts"]:
                        result["artifacts"]["screenshot"]["status"] = "invalidated-race"
                    raise IterationError("source, worktree, executable, or pack changed during capture")
                set_phase(current_phase, "executed" if capture else "checked-no-capture", started)

            result["status"] = "passed"
            result["freshnessClaim"] = True
    except Exception as exc:  # Every failure becomes one bounded stable JSON envelope.
        if current_phase in result["phases"]:
            failed_wall = (
                exc.duration_ms
                if isinstance(exc, ProcessError) and exc.duration_ms is not None
                else (time.perf_counter() - current_phase_started) * 1000.0
            )
            result["phases"][current_phase] = _phase("failed", failed_wall)
        tail = exc.tail if isinstance(exc, ProcessError) else str(exc)
        result["status"] = "failed"
        result["error"] = {
            "phase": exc.phase if isinstance(exc, ProcessError) else current_phase,
            "type": type(exc).__name__,
            "message": _bounded(str(exc)),
            "tail": _bounded(tail),
            **({"returnCode": exc.returncode} if isinstance(exc, ProcessError) else {}),
        }

    result["phases"]["total"] = _phase("executed", (time.perf_counter() - started_total) * 1000.0)
    for phase in result["phases"].values():
        if isinstance(phase.get("wallMs"), (int, float)):
            phase["wallMs"] = round(float(phase["wallMs"]), 3)
    result["toolCalls"]["total"] = sum(result["toolCalls"][key] for key in ("process", "devapi", "guard"))
    result["outputBytes"]["total"] = result["outputBytes"]["process"] + result["outputBytes"]["devapi"]
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", nargs="?", type=int)
    parser.add_argument("--reuse", action="store_true", help="Attach only; no fixtures, build, launch, proof, or capture.")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--no-capture", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    result = execute_iteration(reuse=args.reuse, port=args.port, capture=not args.no_capture and not args.reuse)
    if args.json or result["status"] != "passed":
        print(json.dumps(result, separators=(",", ":"), ensure_ascii=False))
    elif result["freshnessClaim"]:
        print(
            f"iterate: passed fresh build in {result['phases']['total']['wallMs']:.1f} ms; "
            f"{PROOF_METHOD} exact match"
        )
    else:
        print(
            f"iterate: passed attach-only readiness in {result['phases']['total']['wallMs']:.1f} ms; "
            "freshness claim: false"
        )
    return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
