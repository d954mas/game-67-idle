"""Fresh native-build provenance for OBS-backed capture commands."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
from typing import Any, Callable, Mapping, Sequence


class NativeCapturePreflightError(RuntimeError):
    """The requested capture cannot prove it launches the current native build."""


def canonical_native_executable(game_root: Path) -> Path:
    executable = "game.exe" if os.name == "nt" else "game"
    return game_root.resolve() / "build" / "devapi-debug" / "bin" / executable


def canonical_native_build_dir(game_root: Path) -> Path:
    return game_root.resolve() / "build" / "devapi-debug"


def executable_identity(executable: Path) -> dict[str, object]:
    digest = hashlib.sha256()
    with executable.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return {
        "path": str(executable.resolve()),
        "bytes": executable.stat().st_size,
        "sha256": digest.hexdigest(),
    }


def _build_instruction(game_root: Path) -> str:
    build_dir = canonical_native_build_dir(game_root)
    return (
        "configure the canonical DevAPI build first: "
        f"cmake -G Ninja -S {game_root.resolve()} -B {build_dir} "
        "-DCMAKE_BUILD_TYPE=Debug -DGAME_DEVAPI_ENABLED=ON"
    )


def _cache_values(cache_path: Path) -> dict[str, str]:
    values = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" not in line or ":" not in line.split("=", 1)[0]:
            continue
        key = line.split(":", 1)[0]
        values[key] = line.split("=", 1)[1]
    return values


def _cache_contract(
    game_root: Path,
    requested_executable: Path | None,
    studio_root: Path | None,
) -> tuple[Path, Path, dict[str, object]]:
    game_root = game_root.resolve()
    executable = canonical_native_executable(game_root)
    if requested_executable is not None and requested_executable.resolve() != executable:
        raise NativeCapturePreflightError(
            "capture executable must be the canonical DevAPI artifact: "
            f"{executable}"
        )
    build_dir = canonical_native_build_dir(game_root)
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.is_file():
        raise NativeCapturePreflightError(
            f"canonical CMake cache is missing: {cache_path}; {_build_instruction(game_root)}"
        )
    values = _cache_values(cache_path)
    cache_directory = values.get("CMAKE_CACHEFILE_DIR")
    if cache_directory is None or Path(cache_directory).resolve() != build_dir:
        raise NativeCapturePreflightError(
            f"CMAKE_CACHEFILE_DIR must be the canonical build directory: {build_dir}; {_build_instruction(game_root)}"
        )
    home_value = values.get("CMAKE_HOME_DIRECTORY")
    home = Path(home_value).resolve() if home_value else None
    if home != game_root:
        raise NativeCapturePreflightError(
            f"canonical CMake cache belongs to a different game: {cache_path}; {_build_instruction(game_root)}"
        )
    if values.get("GAME_DEVAPI_ENABLED") != "ON":
        raise NativeCapturePreflightError(
            f"GAME_DEVAPI_ENABLED must be ON in {cache_path}; {_build_instruction(game_root)}"
        )
    if values.get("CMAKE_GENERATOR") != "Ninja" or values.get("CMAKE_BUILD_TYPE") != "Debug":
        raise NativeCapturePreflightError(
            f"canonical DevAPI cache must use Ninja and Debug: {cache_path}; {_build_instruction(game_root)}"
        )
    repository = values.get("GAME_REPO_ROOT")
    if repository is not None:
        expected_root = studio_root.resolve() if studio_root is not None else find_studio_root(game_root)
        if Path(repository).resolve() != expected_root:
            raise NativeCapturePreflightError(
                f"GAME_REPO_ROOT must match the Studio root: {expected_root}; {_build_instruction(game_root)}"
            )
    contract = {
        "cacheFileDirectory": str(build_dir),
        "homeDirectory": str(game_root),
        "gameDevapiEnabled": "ON",
        "generator": "Ninja",
        "buildType": "Debug",
        "gameRepositoryRoot": str(studio_root.resolve()) if repository is not None and studio_root is not None else None,
    }
    return build_dir, executable, contract


def validate_native_build_cache(
    game_root: Path,
    requested_executable: Path | None = None,
    *,
    studio_root: Path | None = None,
) -> tuple[Path, Path]:
    build_dir, executable, _contract = _cache_contract(
        game_root, requested_executable, studio_root
    )
    return build_dir, executable


def find_studio_root(game_root: Path) -> Path:
    for candidate in (game_root.resolve(), *game_root.resolve().parents):
        if (candidate / "ai_studio" / "runtime_automation" / "native_capture_fingerprint.mjs").is_file():
            return candidate
    raise NativeCapturePreflightError(f"Studio root not found above game directory: {game_root}")


def _write_log(log_path: Path, command: Sequence[str], cwd: Path, result: subprocess.CompletedProcess[str]) -> None:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        "command: " + json.dumps(list(command)) + "\n"
        + "cwd: " + str(cwd) + "\n"
        + "returncode: " + str(result.returncode) + "\n\nstdout:\n"
        + (result.stdout or "") + "\n\nstderr:\n" + (result.stderr or ""),
        encoding="utf-8",
    )


def _run_external(
    command: Sequence[str],
    *,
    cwd: Path,
    log_path: Path,
    runner: Callable[..., subprocess.CompletedProcess[str]] | None = None,
    capture_stdout: bool = False,
) -> subprocess.CompletedProcess[str]:
    if runner is not None:
        result = runner(list(command), cwd=str(cwd), text=True, capture_output=True, check=False)
        _write_log(log_path, command, cwd, result)
    else:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        stdout = []
        tail = ""
        try:
            with log_path.open("w", encoding="utf-8") as log:
                log.write("command: " + json.dumps(list(command)) + "\n")
                log.write("cwd: " + str(cwd) + "\n\noutput:\n")
                process = subprocess.Popen(
                    list(command), cwd=str(cwd), text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                )
                assert process.stdout is not None
                for line in process.stdout:
                    log.write(line)
                    tail = (tail + line)[-4000:]
                    if capture_stdout:
                        stdout.append(line)
                returncode = process.wait()
        except OSError as exc:
            result = subprocess.CompletedProcess(list(command), -1, stdout="", stderr=str(exc))
            _write_log(log_path, command, cwd, result)
        else:
            result = subprocess.CompletedProcess(list(command), returncode, stdout="".join(stdout), stderr=tail)
    if result.returncode:
        tail = ((result.stderr or "") + "\n" + (result.stdout or ""))[-4000:].strip()
        raise NativeCapturePreflightError(
            f"native capture command failed ({result.returncode}); full log: {log_path}"
            + (f"\n{tail}" if tail else "")
        )
    return result


def _read_runtime_build_record(
    game_root: Path,
    studio_root: Path,
    log_path: Path,
    *,
    runner: Callable[..., subprocess.CompletedProcess[str]] | None,
    node_executable: str,
) -> dict[str, Any]:
    script = Path(__file__).with_name("native_capture_fingerprint.mjs")
    result = _run_external(
        [node_executable, str(script), str(game_root), str(studio_root)],
        cwd=studio_root,
        log_path=log_path,
        runner=runner,
        capture_stdout=True,
    )
    try:
        record = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise NativeCapturePreflightError(
            f"runtime build fingerprint output is invalid; full log: {log_path}"
        ) from exc
    fingerprint = record.get("fingerprint") if isinstance(record, Mapping) else None
    if not isinstance(fingerprint, str) or len(fingerprint) != 64:
        raise NativeCapturePreflightError(
            f"runtime build fingerprint is missing; full log: {log_path}"
        )
    return dict(record)


def prepare_native_capture(
    game_root: Path,
    output_root: Path,
    *,
    requested_executable: Path | None = None,
    studio_root: Path | None = None,
    runner: Callable[..., subprocess.CompletedProcess[str]] | None = None,
    node_executable: str = "node",
    cmake_executable: str = "cmake",
) -> dict[str, object]:
    """Rebuild the canonical native target and bind it to current source inputs."""
    game_root = game_root.resolve()
    studio_root = studio_root.resolve() if studio_root is not None else find_studio_root(game_root)
    build_dir, executable, cache_before = _cache_contract(
        game_root, requested_executable, studio_root
    )
    logs = output_root.resolve() / ".preflight"
    before = _read_runtime_build_record(
        game_root, studio_root, logs / "01-source-before-build.log",
        runner=runner, node_executable=node_executable,
    )
    _run_external(
        [cmake_executable, "--build", str(build_dir), "--target", "game"],
        cwd=game_root,
        log_path=logs / "02-cmake-build.log",
        runner=runner,
    )
    if not executable.is_file():
        raise NativeCapturePreflightError(
            f"canonical build did not produce its game target: {executable}; {_build_instruction(game_root)}"
        )
    try:
        _build_dir, _executable, cache_after = _cache_contract(
            game_root, requested_executable, studio_root
        )
    except NativeCapturePreflightError as exc:
        raise NativeCapturePreflightError(
            "critical CMake cache settings changed during native capture rebuild; rerun capture"
        ) from exc
    if cache_before != cache_after:
        raise NativeCapturePreflightError(
            "critical CMake cache settings changed during native capture rebuild; rerun capture"
        )
    after = _read_runtime_build_record(
        game_root, studio_root, logs / "03-source-after-build.log",
        runner=runner, node_executable=node_executable,
    )
    if before["fingerprint"] != after["fingerprint"]:
        raise NativeCapturePreflightError(
            "source inputs changed during native capture rebuild; rerun capture"
        )
    return {
        "proofKind": "native-rebuilt",
        "gameRoot": str(game_root),
        "studioRoot": str(studio_root),
        "sourceFingerprint": after["fingerprint"],
        "executable": executable_identity(executable),
        "target": "game",
        "buildDirectory": str(build_dir),
        "logDirectory": str(logs),
        "cacheContract": cache_after,
    }


def validate_native_capture_final(
    provenance: Mapping[str, object],
    *,
    runner: Callable[..., subprocess.CompletedProcess[str]] | None = None,
    node_executable: str = "node",
) -> None:
    """Fail a take when source inputs or the launched executable drifted during REC."""
    try:
        game_root = Path(str(provenance["gameRoot"])).resolve()
        studio_root = Path(str(provenance["studioRoot"])).resolve()
        log_directory = Path(str(provenance["logDirectory"])).resolve()
        expected_fingerprint = str(provenance["sourceFingerprint"])
        expected_executable = provenance["executable"]
        expected_cache = provenance["cacheContract"]
    except KeyError as exc:
        raise NativeCapturePreflightError("native capture provenance is incomplete") from exc
    if not isinstance(expected_executable, Mapping):
        raise NativeCapturePreflightError("native capture executable provenance is invalid")
    if not isinstance(expected_cache, Mapping):
        raise NativeCapturePreflightError("native capture cache provenance is invalid")
    _build_dir, _executable, actual_cache = _cache_contract(
        game_root, canonical_native_executable(game_root), studio_root
    )
    if dict(expected_cache) != actual_cache:
        raise NativeCapturePreflightError(
            "critical CMake cache settings changed during recording; capture proof is invalid"
        )
    actual_record = _read_runtime_build_record(
        game_root, studio_root, log_directory / "04-source-after-recording.log",
        runner=runner, node_executable=node_executable,
    )
    actual_executable = executable_identity(canonical_native_executable(game_root))
    if actual_record["fingerprint"] != expected_fingerprint or dict(expected_executable) != actual_executable:
        raise NativeCapturePreflightError(
            "source inputs or executable changed during recording; capture proof is invalid"
        )
