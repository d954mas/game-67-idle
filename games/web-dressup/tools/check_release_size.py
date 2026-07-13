#!/usr/bin/env python3
"""Fail closed when the complete Poki release payload exceeds 6.5 MiB."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


LIMIT_BYTES = 6_815_744  # 6.5 MiB
EXPECTED_BUILD_NAME = "wasm-release-poki"
REQUIRED_FILES = (
    "assets/game.ntpack",
    "game.js",
    "game.wasm",
    "index.html",
    "platform-sdk-adapter.js",
    "platform-sdk-core.js",
    "platform-sdk.js",
)
FORBIDDEN_SUFFIXES = (".pdb", ".map")


class ReleaseGateError(RuntimeError):
    def __init__(self, message: str, manifest: dict | None = None):
        super().__init__(message)
        self.manifest = manifest


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _read_cmake_cache(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise ReleaseGateError(f"missing required build provenance: {path.name}")
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not raw_line or raw_line.startswith(("#", "//")) or "=" not in raw_line:
            continue
        typed_key, value = raw_line.split("=", 1)
        key = typed_key.split(":", 1)[0]
        values[key] = value
    return values


def build_manifest(bin_dir: Path, limit_bytes: int = LIMIT_BYTES) -> dict:
    """Inspect every uploaded file and return a deterministic passing manifest."""
    bin_dir = Path(bin_dir).resolve()
    if bin_dir.name != "bin" or bin_dir.parent.name != EXPECTED_BUILD_NAME:
        raise ReleaseGateError(
            f"expected {EXPECTED_BUILD_NAME}/bin; only the release Poki build may pass"
        )
    if not bin_dir.is_dir():
        raise ReleaseGateError(f"release payload directory does not exist: {bin_dir}")

    cache = _read_cmake_cache(bin_dir.parent / "CMakeCache.txt")
    release_safe = (
        cache.get("CMAKE_BUILD_TYPE") == "Release"
        and cache.get("GAME_PUBLISH_TARGET") == "poki"
        and cache.get("GAME_DEVAPI_ENABLED", "OFF").upper() in {"0", "FALSE", "OFF"}
        and cache.get("GAME_PLATFORM_SDK_DEBUG_UI", "OFF").upper() in {"0", "FALSE", "OFF"}
    )
    if not release_safe:
        raise ReleaseGateError("CMake configuration is not release-safe Poki (Release, DevAPI/debug UI OFF)")

    missing = [relative for relative in REQUIRED_FILES if not (bin_dir / relative).is_file()]
    if missing:
        raise ReleaseGateError(f"missing required release files: {', '.join(missing)}")

    paths = sorted(path for path in bin_dir.rglob("*") if path.is_file())
    forbidden = [
        path.relative_to(bin_dir).as_posix()
        for path in paths
        if path.suffix.lower() in FORBIDDEN_SUFFIXES
        or "devapi" in path.relative_to(bin_dir).as_posix().lower()
        or "debug" in path.relative_to(bin_dir).as_posix().lower()
    ]
    if forbidden:
        raise ReleaseGateError(f"forbidden release file: {', '.join(forbidden)}")

    files = [
        {
            "path": path.relative_to(bin_dir).as_posix(),
            "bytes": path.stat().st_size,
            "sha256": _sha256(path),
        }
        for path in paths
    ]
    total_bytes = sum(item["bytes"] for item in files)
    manifest = {
        "schema": "runway_awakening.release_payload.v1",
        "build": EXPECTED_BUILD_NAME,
        "limit_bytes": limit_bytes,
        "total_bytes": total_bytes,
        "status": "pass" if total_bytes <= limit_bytes else "fail",
        "files": files,
    }
    if total_bytes > limit_bytes:
        raise ReleaseGateError(
            f"release payload {total_bytes} bytes exceeds {limit_bytes} byte limit",
            manifest,
        )
    return manifest


def _write_manifest(path: Path, manifest: dict) -> None:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", type=Path, required=True, help="wasm-release-poki/bin directory")
    parser.add_argument("--manifest", type=Path, required=True, help="JSON output outside the payload")
    parser.add_argument("--limit-bytes", type=int, default=LIMIT_BYTES)
    args = parser.parse_args(argv)

    bin_dir = args.bin.resolve()
    manifest_path = args.manifest.resolve()
    if manifest_path == bin_dir or bin_dir in manifest_path.parents:
        print("manifest must be outside the uploaded bin payload", file=sys.stderr)
        return 2

    try:
        manifest = build_manifest(bin_dir, args.limit_bytes)
    except ReleaseGateError as exc:
        if exc.manifest is not None:
            _write_manifest(manifest_path, exc.manifest)
        print(f"release size gate failed: {exc}", file=sys.stderr)
        return 1

    _write_manifest(manifest_path, manifest)
    print(
        f"release size gate passed: {manifest['total_bytes']} / {manifest['limit_bytes']} bytes; "
        f"manifest={manifest_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
