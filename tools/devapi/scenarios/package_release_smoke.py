#!/usr/bin/env python3
"""Smoke-test the packaged native PC release from its own folder."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import zipfile
from datetime import datetime
from pathlib import Path
from typing import Any

from base import Scenario, finish
from devapi_client import convert_ppm_to_png, run_capture_screenshot
from pixel_health import PixelHealthError, analyze_png, assert_pixel_health


ROOT = Path(__file__).resolve().parents[3]
PACKAGE_DIR = ROOT / "build" / "release" / "67-world-pc" / "67-world"
PACKAGE_ZIP = ROOT / "build" / "release" / "67-world-pc" / "67-world-pc.zip"
EXPORT_BUNDLE = PACKAGE_DIR / "child_test_results_for_return.zip"
DEFAULT_REPORT_PATH = ROOT / "build" / "reports" / "package_release_smoke_v2_evidence.json"
EXPECTED_SELF_CHECK_COUNT = 19


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def package_files() -> dict[str, Path]:
    return {
        "67-world.exe": PACKAGE_DIR / "67-world.exe",
        "assets/world67_art.ntpack": PACKAGE_DIR / "assets" / "world67_art.ntpack",
        "START_HERE.bat": PACKAGE_DIR / "START_HERE.bat",
        "RUN_67_WORLD.bat": PACKAGE_DIR / "RUN_67_WORLD.bat",
        "START_CHILD_TEST_FRESH.bat": PACKAGE_DIR / "START_CHILD_TEST_FRESH.bat",
        "CREATE_CHILD_TEST_REPORT.ps1": PACKAGE_DIR / "CREATE_CHILD_TEST_REPORT.ps1",
        "CREATE_CHILD_TEST_REPORT.bat": PACKAGE_DIR / "CREATE_CHILD_TEST_REPORT.bat",
        "VALIDATE_CHILD_TEST_REPORT.ps1": PACKAGE_DIR / "VALIDATE_CHILD_TEST_REPORT.ps1",
        "VALIDATE_CHILD_TEST_REPORT.bat": PACKAGE_DIR / "VALIDATE_CHILD_TEST_REPORT.bat",
        "EXPORT_CHILD_TEST_RESULTS.ps1": PACKAGE_DIR / "EXPORT_CHILD_TEST_RESULTS.ps1",
        "EXPORT_CHILD_TEST_RESULTS.bat": PACKAGE_DIR / "EXPORT_CHILD_TEST_RESULTS.bat",
        "VERIFY_PACKAGE.ps1": PACKAGE_DIR / "VERIFY_PACKAGE.ps1",
        "VERIFY_PACKAGE.bat": PACKAGE_DIR / "VERIFY_PACKAGE.bat",
        "README.txt": PACKAGE_DIR / "README.txt",
        "RETURN_INSTRUCTIONS.txt": PACKAGE_DIR / "RETURN_INSTRUCTIONS.txt",
        "PARENT_OBSERVER_GUIDE.md": PACKAGE_DIR / "PARENT_OBSERVER_GUIDE.md",
        "CHILD_TEST_ACCEPTANCE.md": PACKAGE_DIR / "CHILD_TEST_ACCEPTANCE.md",
        "CHILD_TEST_RESULT_TEMPLATE.md": PACKAGE_DIR / "CHILD_TEST_RESULT_TEMPLATE.md",
        "release_manifest.json": PACKAGE_DIR / "release_manifest.json",
        "CHECKSUMS.txt": PACKAGE_DIR / "CHECKSUMS.txt",
    }


def validate_checksums() -> dict[str, Any]:
    checksums_path = PACKAGE_DIR / "CHECKSUMS.txt"
    expected: dict[str, str] = {}
    for line in checksums_path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        digest, rel = line.split(None, 1)
        expected[rel.strip()] = digest
    actual = {rel: sha256(path) for rel, path in package_files().items() if rel != "CHECKSUMS.txt"}
    return {
        "expected_count": len(expected),
        "actual_count": len(actual),
        "matches": expected == actual,
        "expected": expected,
        "actual": actual,
    }


def validate_zip() -> dict[str, Any]:
    with zipfile.ZipFile(PACKAGE_ZIP) as archive:
        names = sorted(archive.namelist())
        bad_file = archive.testzip()
    required = [
        "67-world/67-world.exe",
        "67-world/assets/world67_art.ntpack",
        "67-world/START_HERE.bat",
        "67-world/RUN_67_WORLD.bat",
        "67-world/START_CHILD_TEST_FRESH.bat",
        "67-world/CREATE_CHILD_TEST_REPORT.ps1",
        "67-world/CREATE_CHILD_TEST_REPORT.bat",
        "67-world/VALIDATE_CHILD_TEST_REPORT.ps1",
        "67-world/VALIDATE_CHILD_TEST_REPORT.bat",
        "67-world/EXPORT_CHILD_TEST_RESULTS.ps1",
        "67-world/EXPORT_CHILD_TEST_RESULTS.bat",
        "67-world/VERIFY_PACKAGE.ps1",
        "67-world/VERIFY_PACKAGE.bat",
        "67-world/README.txt",
        "67-world/RETURN_INSTRUCTIONS.txt",
        "67-world/PARENT_OBSERVER_GUIDE.md",
        "67-world/CHILD_TEST_ACCEPTANCE.md",
        "67-world/CHILD_TEST_RESULT_TEMPLATE.md",
        "67-world/release_manifest.json",
        "67-world/CHECKSUMS.txt",
    ]
    return {
        "zip": str(PACKAGE_ZIP),
        "zip_sha256": sha256(PACKAGE_ZIP),
        "entries": len(names),
        "bad_file": bad_file,
        "required_present": all(name in names for name in required),
    }


def validate_manifest() -> dict[str, Any]:
    manifest_path = PACKAGE_DIR / "release_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    package = manifest.get("package", {})
    validation = manifest.get("validation", {})
    files = package.get("files", [])
    file_paths = [entry.get("path") for entry in files if isinstance(entry, dict)]
    expected = {
        "child_test_acceptance_kit": "CHILD_TEST_ACCEPTANCE.md",
        "parent_observer_guide": "PARENT_OBSERVER_GUIDE.md",
        "parent_observer_guide_source": "gamedesign/meme-evolution/parent_observer_guide.md",
        "requires_child_test_acceptance": True,
        "child_test_acceptance_kit_source": "gamedesign/meme-evolution/child_test_acceptance.md",
        "self_check_script": "VERIFY_PACKAGE.ps1",
        "self_check_launcher": "VERIFY_PACKAGE.bat",
        "start_here_launcher": "START_HERE.bat",
        "child_test_launcher": "START_CHILD_TEST_FRESH.bat",
        "child_test_report_script": "CREATE_CHILD_TEST_REPORT.ps1",
        "child_test_report_launcher": "CREATE_CHILD_TEST_REPORT.bat",
        "child_test_report_validator_script": "VALIDATE_CHILD_TEST_REPORT.ps1",
        "child_test_report_validator_launcher": "VALIDATE_CHILD_TEST_REPORT.bat",
        "child_test_results_export_script": "EXPORT_CHILD_TEST_RESULTS.ps1",
        "child_test_results_export_launcher": "EXPORT_CHILD_TEST_RESULTS.bat",
        "child_test_result_template": "CHILD_TEST_RESULT_TEMPLATE.md",
    }
    return {
        "expected": expected,
        "child_test_acceptance_kit": package.get("child_test_acceptance_kit"),
        "requires_child_test_acceptance": validation.get("requires_child_test_acceptance"),
        "child_test_acceptance_kit_source": validation.get("child_test_acceptance_kit_source"),
        "self_check_script": package.get("self_check_script"),
        "self_check_launcher": package.get("self_check_launcher"),
        "start_here_launcher": package.get("start_here_launcher"),
        "child_test_launcher": package.get("child_test_launcher"),
        "parent_observer_guide": package.get("parent_observer_guide"),
        "parent_observer_guide_source": validation.get("parent_observer_guide_source"),
        "child_test_report_script": package.get("child_test_report_script"),
        "child_test_report_launcher": package.get("child_test_report_launcher"),
        "child_test_report_validator_script": package.get("child_test_report_validator_script"),
        "child_test_report_validator_launcher": package.get("child_test_report_validator_launcher"),
        "child_test_results_export_script": package.get("child_test_results_export_script"),
        "child_test_results_export_launcher": package.get("child_test_results_export_launcher"),
        "child_test_result_template": package.get("child_test_result_template"),
        "manifest_files": file_paths,
        "matches": (
            package.get("child_test_acceptance_kit") == expected["child_test_acceptance_kit"]
            and package.get("self_check_script") == expected["self_check_script"]
            and package.get("self_check_launcher") == expected["self_check_launcher"]
            and package.get("start_here_launcher") == expected["start_here_launcher"]
            and package.get("child_test_launcher") == expected["child_test_launcher"]
            and package.get("parent_observer_guide") == expected["parent_observer_guide"]
            and package.get("child_test_report_script") == expected["child_test_report_script"]
            and package.get("child_test_report_launcher") == expected["child_test_report_launcher"]
            and package.get("child_test_report_validator_script") == expected["child_test_report_validator_script"]
            and package.get("child_test_report_validator_launcher") == expected["child_test_report_validator_launcher"]
            and package.get("child_test_results_export_script") == expected["child_test_results_export_script"]
            and package.get("child_test_results_export_launcher") == expected["child_test_results_export_launcher"]
            and package.get("child_test_result_template") == expected["child_test_result_template"]
            and validation.get("requires_child_test_acceptance") is expected["requires_child_test_acceptance"]
            and validation.get("parent_observer_guide_source") == expected["parent_observer_guide_source"]
            and validation.get("child_test_acceptance_kit_source") == expected["child_test_acceptance_kit_source"]
            and expected["child_test_acceptance_kit"] in file_paths
            and expected["parent_observer_guide"] in file_paths
            and expected["self_check_script"] in file_paths
            and expected["self_check_launcher"] in file_paths
            and expected["start_here_launcher"] in file_paths
            and expected["child_test_launcher"] in file_paths
            and expected["child_test_report_script"] in file_paths
            and expected["child_test_report_launcher"] in file_paths
            and expected["child_test_report_validator_script"] in file_paths
            and expected["child_test_report_validator_launcher"] in file_paths
            and expected["child_test_results_export_script"] in file_paths
            and expected["child_test_results_export_launcher"] in file_paths
            and expected["child_test_result_template"] in file_paths
        ),
    }


def report_path_from_stdout(stdout: str) -> Path | None:
    for line in reversed(stdout.splitlines()):
        text = line.strip()
        if text.lower().endswith(".md") and "child_test_result_" in text:
            return Path(text)
    return None


def cleanup_smoke_artifact(path: Path | None) -> dict[str, Any]:
    if path is None:
        return {"path": "", "attempted": False, "deleted": False, "remaining": False, "safe": False}
    resolved = path.resolve()
    allowed_dirs = [
        (PACKAGE_DIR / "child_test_results").resolve(),
        (ROOT / "build" / "tmp").resolve(),
    ]
    safe_dir = any(resolved == allowed or resolved.is_relative_to(allowed) for allowed in allowed_dirs)
    safe_name = path.name.startswith("child_test_result_") or path.name == "valid_child_test_report_for_smoke.md"
    if not safe_dir or not safe_name:
        return {"path": str(path), "attempted": False, "deleted": False, "remaining": path.exists(), "safe": False}
    deleted = False
    if path.exists():
        path.unlink()
        deleted = True
    return {"path": str(path), "attempted": True, "deleted": deleted, "remaining": path.exists(), "safe": True}


def cleanup_empty_child_test_results_dir() -> dict[str, Any]:
    results_dir = PACKAGE_DIR / "child_test_results"
    if not results_dir.exists():
        return {"path": str(results_dir), "exists": False, "removed": False, "remaining_entries": 0}
    remaining_entries = list(results_dir.iterdir())
    removed = False
    if not remaining_entries:
        results_dir.rmdir()
        removed = True
    return {
        "path": str(results_dir),
        "exists": results_dir.exists(),
        "removed": removed,
        "remaining_entries": len(list(results_dir.iterdir())) if results_dir.exists() else 0,
    }


def cleanup_export_bundle() -> dict[str, Any]:
    deleted = False
    if EXPORT_BUNDLE.exists():
        EXPORT_BUNDLE.unlink()
        deleted = True
    return {
        "path": str(EXPORT_BUNDLE),
        "deleted": deleted,
        "remaining": EXPORT_BUNDLE.exists(),
    }


def cleanup_smoke_evidence_dir() -> dict[str, Any]:
    evidence_dir = PACKAGE_DIR / "child_test_results" / "evidence"
    deleted = False
    if evidence_dir.exists():
        shutil.rmtree(evidence_dir)
        deleted = True
    cleanup_empty_child_test_results_dir()
    return {
        "path": str(evidence_dir),
        "deleted": deleted,
        "remaining": evidence_dir.exists(),
    }


def validate_smoke_report_cleanup(paths: list[str]) -> dict[str, Any]:
    remaining = [path for path in paths if path and Path(path).exists()]
    dir_status = cleanup_empty_child_test_results_dir()
    return {
        "paths": paths,
        "remaining": remaining,
        "child_test_results_dir": dir_status,
        "passed": not remaining,
    }


def run_child_test_report_recorder() -> dict[str, Any]:
    script = PACKAGE_DIR / "CREATE_CHILD_TEST_REPORT.ps1"
    results_dir = PACKAGE_DIR / "child_test_results"
    before = set(results_dir.glob("child_test_result_*.md")) if results_dir.exists() else set()
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script)],
        cwd=str(PACKAGE_DIR),
        capture_output=True,
        text=True,
        timeout=20,
    )
    after = set(results_dir.glob("child_test_result_*.md")) if results_dir.exists() else set()
    created = sorted(after - before)
    latest = created[-1] if created else report_path_from_stdout(completed.stdout)
    latest_text = latest.read_text(encoding="utf-8", errors="replace") if latest else ""
    return {
        "script": str(script),
        "exit_code": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
        "created": str(latest) if latest else "",
        "created_count": len(created),
        "contains_template_title": "# 67 World Child-Test Result" in latest_text,
        "contains_package_folder": "Package folder:" in latest_text,
        "passed": completed.returncode == 0
        and "PASS child-test result report created" in completed.stdout
        and bool(latest)
        and "# 67 World Child-Test Result" in latest_text,
    }


def synthetic_valid_report_path() -> Path:
    path = ROOT / "build" / "tmp" / "valid_child_test_report_for_smoke.md"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        """# 67 World Child-Test Result

## Session

- Package zip: 67-world-pc.zip
- Package folder: package
- Report created: 2026-06-13 02:00:00
- Observer: Smoke Test
- Child age: 8
- Device: Native PC
- Speaker/headphones: Speakers
- Session length in minutes: 60

## Required Setup Checks

- `VERIFY_PACKAGE.bat` passed before the session: yes
- `START_CHILD_TEST_FRESH.bat` was used: yes
- Game started at first `TAP BOX` FTUE: yes
- Real audio output was enabled: yes

## First Minute Result

- Child found `TAP BOX`: yes
- Child understood matching pairs: yes
- Child completed first merge: yes
- Text was readable from normal distance: yes
- Audio feedback was audible: yes
- First-minute pass: yes
- Notes: synthetic validator smoke

## Five-Minute Result

- Child kept spawning/merging without confusion: yes
- Child noticed new 67 variants: yes
- Child understood the upgrade tile when it showed `BUY`: yes
- Child recovered from a full board using `FREE SLOT`: yes
- Screen stayed readable and not overloaded: yes
- Five-minute pass: yes
- Notes: synthetic validator smoke

## One-Hour Result

- Minutes played: 60
- Highest 67 reached: Cosmic 67
- Collection count: 30
- Better Crate level if visible: 21
- Child still understood the next goal near the end: yes
- Progress felt too slow: no
- Progress felt too fast: no
- One-hour pass: yes
- Notes: synthetic validator smoke

## Audio Result

- Spawn sound audible: yes
- Merge sound audible: yes
- Upgrade sound audible: yes
- Blocked/full-board sound audible: yes
- Free slot/recycle sound audible: yes
- Sounds were pleasant for children: yes
- Notes: synthetic validator smoke

## Final Acceptance

Overall result: pass

Observer summary: Synthetic smoke summary confirms the child-test path produced meaningful observations.
""",
        encoding="utf-8",
    )
    return path


def synthetic_missing_notes_report_path() -> Path:
    path = ROOT / "build" / "tmp" / "child_test_result_missing_notes_for_smoke.md"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        """# 67 World Child-Test Result

## Session

- Package zip: 67-world-pc.zip
- Package folder: package
- Report created: 2026-06-13 02:00:00
- Observer: Smoke Test
- Child age: 8
- Device: Native PC
- Speaker/headphones: Speakers
- Session length in minutes: 60

## Required Setup Checks

- `VERIFY_PACKAGE.bat` passed before the session: yes
- `START_CHILD_TEST_FRESH.bat` was used: yes
- Game started at first `TAP BOX` FTUE: yes
- Real audio output was enabled: yes

## First Minute Result

- Child found `TAP BOX`: yes
- Child understood matching pairs: yes
- Child completed first merge: yes
- Text was readable from normal distance: yes
- Audio feedback was audible: yes
- First-minute pass: yes
- Notes:

## Five-Minute Result

- Child kept spawning/merging without confusion: yes
- Child noticed new 67 variants: yes
- Child understood the upgrade tile when it showed `BUY`: yes
- Child recovered from a full board using `FREE SLOT`: yes
- Screen stayed readable and not overloaded: yes
- Five-minute pass: yes
- Notes:

## One-Hour Result

- Minutes played: 60
- Highest 67 reached: Cosmic 67
- Collection count: 30
- Better Crate level if visible: 21
- Child still understood the next goal near the end: yes
- Progress felt too slow: no
- Progress felt too fast: no
- One-hour pass: yes
- Notes:

## Audio Result

- Spawn sound audible: yes
- Merge sound audible: yes
- Upgrade sound audible: yes
- Blocked/full-board sound audible: yes
- Free slot/recycle sound audible: yes
- Sounds were pleasant for children: yes
- Notes:

## Final Acceptance

Overall result: pass

Observer summary:
""",
        encoding="utf-8",
    )
    return path


def synthetic_too_short_report_path() -> Path:
    path = ROOT / "build" / "tmp" / "child_test_result_too_short_for_smoke.md"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        """# 67 World Child-Test Result

## Session

- Package zip: 67-world-pc.zip
- Package folder: package
- Report created: 2026-06-13 02:00:00
- Observer: Smoke Test
- Child age: 8
- Device: Native PC
- Speaker/headphones: Speakers
- Session length in minutes: 45

## Required Setup Checks

- `VERIFY_PACKAGE.bat` passed before the session: yes
- `START_CHILD_TEST_FRESH.bat` was used: yes
- Game started at first `TAP BOX` FTUE: yes
- Real audio output was enabled: yes

## First Minute Result

- Child found `TAP BOX`: yes
- Child understood matching pairs: yes
- Child completed first merge: yes
- Text was readable from normal distance: yes
- Audio feedback was audible: yes
- First-minute pass: yes
- Notes: synthetic short-session smoke observation

## Five-Minute Result

- Child kept spawning/merging without confusion: yes
- Child noticed new 67 variants: yes
- Child understood the upgrade tile when it showed `BUY`: yes
- Child recovered from a full board using `FREE SLOT`: yes
- Screen stayed readable and not overloaded: yes
- Five-minute pass: yes
- Notes: synthetic short-session smoke observation

## One-Hour Result

- Minutes played: 45
- Highest 67 reached: Cosmic 67
- Collection count: 30
- Better Crate level if visible: 21
- Child still understood the next goal near the end: yes
- Progress felt too slow: no
- Progress felt too fast: no
- One-hour pass: yes
- Notes: synthetic short-session smoke observation

## Audio Result

- Spawn sound audible: yes
- Merge sound audible: yes
- Upgrade sound audible: yes
- Blocked/full-board sound audible: yes
- Free slot/recycle sound audible: yes
- Sounds were pleasant for children: yes
- Notes: synthetic short-session smoke observation

## Final Acceptance

Overall result: pass

Observer summary: Synthetic smoke summary has enough text but must fail because the session is too short.
""",
        encoding="utf-8",
    )
    return path


def run_child_test_report_validator(report_path: Path) -> dict[str, Any]:
    script = PACKAGE_DIR / "VALIDATE_CHILD_TEST_REPORT.ps1"
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script), str(report_path)],
        cwd=str(PACKAGE_DIR),
        capture_output=True,
        text=True,
        timeout=20,
    )
    return {
        "script": str(script),
        "report": str(report_path),
        "exit_code": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }


def validate_report_validator(blank_report: str) -> dict[str, Any]:
    blank_status = run_child_test_report_validator(Path(blank_report)) if blank_report else {
        "exit_code": -1,
        "stdout": "",
        "stderr": "blank report missing",
    }
    missing_notes_report = synthetic_missing_notes_report_path()
    missing_notes_status = run_child_test_report_validator(missing_notes_report)
    too_short_report = synthetic_too_short_report_path()
    too_short_status = run_child_test_report_validator(too_short_report)
    valid_report = synthetic_valid_report_path()
    valid_status = run_child_test_report_validator(valid_report)
    blank_cleanup = cleanup_smoke_artifact(Path(blank_report)) if blank_report else cleanup_smoke_artifact(None)
    missing_notes_cleanup = cleanup_smoke_artifact(missing_notes_report)
    too_short_cleanup = cleanup_smoke_artifact(too_short_report)
    valid_cleanup = cleanup_smoke_artifact(valid_report)
    return {
        "blank": blank_status,
        "missing_notes": missing_notes_status,
        "too_short": too_short_status,
        "synthetic_valid": valid_status,
        "blank_cleanup": blank_cleanup,
        "missing_notes_cleanup": missing_notes_cleanup,
        "too_short_cleanup": too_short_cleanup,
        "synthetic_valid_cleanup": valid_cleanup,
        "passed": blank_status["exit_code"] != 0
        and "FAIL child-test report validation" in blank_status["stdout"]
        and missing_notes_status["exit_code"] != 0
        and "need 4 meaningful entries for Notes" in missing_notes_status["stdout"]
        and "missing meaningful line: Observer summary" in missing_notes_status["stdout"]
        and too_short_status["exit_code"] != 0
        and "Session length in minutes must be at least 55 minutes" in too_short_status["stdout"]
        and "Minutes played must be at least 55 minutes" in too_short_status["stdout"]
        and valid_status["exit_code"] == 0
        and "PASS child-test report validation" in valid_status["stdout"]
        and blank_cleanup["remaining"] is False
        and missing_notes_cleanup["remaining"] is False
        and too_short_cleanup["remaining"] is False
        and valid_cleanup["remaining"] is False,
    }


def run_child_test_results_export(report_path: Path) -> dict[str, Any]:
    script = PACKAGE_DIR / "EXPORT_CHILD_TEST_RESULTS.ps1"
    if EXPORT_BUNDLE.exists():
        EXPORT_BUNDLE.unlink()
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script), str(report_path)],
        cwd=str(PACKAGE_DIR),
        capture_output=True,
        text=True,
        timeout=30,
    )
    bundle_entries: list[str] = []
    bad_file = None
    if EXPORT_BUNDLE.exists():
        with zipfile.ZipFile(EXPORT_BUNDLE) as archive:
            bundle_entries = sorted(archive.namelist())
            bad_file = archive.testzip()
    return {
        "script": str(script),
        "report": str(report_path),
        "exit_code": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
        "bundle": str(EXPORT_BUNDLE),
        "bundle_exists": EXPORT_BUNDLE.exists(),
        "bundle_bytes": EXPORT_BUNDLE.stat().st_size if EXPORT_BUNDLE.exists() else 0,
        "bundle_bad_file": bad_file,
        "bundle_entries": bundle_entries,
    }


def validate_results_export(blank_report: str) -> dict[str, Any]:
    blank_status = run_child_test_results_export(Path(blank_report)) if blank_report else {
        "exit_code": -1,
        "stdout": "",
        "stderr": "blank report missing",
        "bundle_exists": False,
        "bundle_entries": [],
    }
    blank_cleanup = cleanup_export_bundle()
    valid_report = synthetic_valid_report_path()
    evidence_dir = PACKAGE_DIR / "child_test_results" / "evidence"
    evidence_dir.mkdir(parents=True, exist_ok=True)
    evidence_file = evidence_dir / "evidence_note_for_smoke.txt"
    evidence_file.write_text("synthetic optional evidence file for smoke\n", encoding="utf-8")
    valid_status = run_child_test_results_export(valid_report)
    expected_entries = {
        valid_report.name,
        "release_manifest.json",
        "CHECKSUMS.txt",
        "CHILD_TEST_ACCEPTANCE.md",
        "PARENT_OBSERVER_GUIDE.md",
        "RETURN_INSTRUCTIONS.txt",
        "evidence/evidence_note_for_smoke.txt",
    }
    valid_entries = set(valid_status.get("bundle_entries") or [])
    valid_cleanup = cleanup_export_bundle()
    valid_report_cleanup = cleanup_smoke_artifact(valid_report)
    evidence_cleanup = cleanup_smoke_evidence_dir()
    return {
        "blank": blank_status,
        "blank_cleanup": blank_cleanup,
        "synthetic_valid": valid_status,
        "synthetic_valid_cleanup": valid_cleanup,
        "synthetic_valid_report_cleanup": valid_report_cleanup,
        "synthetic_evidence_cleanup": evidence_cleanup,
        "expected_entries": sorted(expected_entries),
        "passed": blank_status["exit_code"] != 0
        and "FAIL export blocked because report validation failed" in blank_status["stdout"]
        and blank_cleanup["remaining"] is False
        and valid_status["exit_code"] == 0
        and "PASS child-test results export created" in valid_status["stdout"]
        and valid_status["bundle_exists"] is True
        and valid_status["bundle_bad_file"] is None
        and expected_entries.issubset(valid_entries)
        and valid_cleanup["remaining"] is False
        and valid_report_cleanup["remaining"] is False
        and evidence_cleanup["remaining"] is False,
    }


def validate_child_test_launcher(files: dict[str, Path]) -> dict[str, Any]:
    launcher = files["START_CHILD_TEST_FRESH.bat"]
    text = launcher.read_text(encoding="utf-8", errors="replace")
    expected = ["67-world.exe", "--fresh-state", "--disable-autosave"]
    return {
        "launcher": str(launcher),
        "expected_tokens": expected,
        "content": text,
        "matches": all(token in text for token in expected),
    }


def validate_start_here_launcher(files: dict[str, Path]) -> dict[str, Any]:
    launcher = files["START_HERE.bat"]
    text = launcher.read_text(encoding="utf-8", errors="replace")
    expected = [
        "VERIFY_PACKAGE.bat",
        "RUN_67_WORLD.bat",
        "START_CHILD_TEST_FRESH.bat",
        "CREATE_CHILD_TEST_REPORT.bat",
        "VALIDATE_CHILD_TEST_REPORT.bat",
        "EXPORT_CHILD_TEST_RESULTS.bat",
        "PARENT_OBSERVER_GUIDE.md",
        "CHILD_TEST_ACCEPTANCE.md",
        "Read parent observer guide",
        "Read child-test acceptance kit",
        "[4] Start fresh child-test",
        "[6] Create report after the session",
        "[7] Validate the filled report",
        "[8] Export validated child-test results zip",
        "choice /C 12345678Q",
    ]
    return {
        "launcher": str(launcher),
        "expected_tokens": expected,
        "content": text,
        "matches": all(token in text for token in expected),
    }


def validate_readme_guided_path(files: dict[str, Path]) -> dict[str, Any]:
    readme = files["README.txt"]
    text = readme.read_text(encoding="utf-8", errors="replace")
    expected = [
        "Guided child-test menu path",
        "[1] Verify package.",
        "[2] Read PARENT_OBSERVER_GUIDE.md.",
        "[3] Read CHILD_TEST_ACCEPTANCE.md.",
        "[4] Start fresh child-test.",
        "[6] Create report after the session.",
        "[7] Validate the filled report.",
        "[8] Export validated child-test results zip.",
        "Direct launchers",
    ]
    return {
        "readme": str(readme),
        "expected_tokens": expected,
        "content": text,
        "matches": all(token in text for token in expected),
    }


def validate_return_instructions(files: dict[str, Path]) -> dict[str, Any]:
    instructions = files["RETURN_INSTRUCTIONS.txt"]
    text = instructions.read_text(encoding="utf-8", errors="replace")
    expected = [
        "67 World child-test return instructions",
        "Run START_HERE.bat.",
        "Choose [6] Create child-test report",
        "Choose [7] Validate filled child-test report",
        "Choose [8] Export validated child-test results",
        "child_test_results_for_return.zip",
        "Do not return the whole game folder unless asked.",
    ]
    return {
        "instructions": str(instructions),
        "expected_tokens": expected,
        "content": text,
        "matches": all(token in text for token in expected),
    }


def run_start_here_choice(choice: str) -> dict[str, Any]:
    launcher = PACKAGE_DIR / "START_HERE.bat"
    results_dir = PACKAGE_DIR / "child_test_results"
    before = set(results_dir.glob("child_test_result_*.md")) if results_dir.exists() else set()
    completed = subprocess.run(
        ["cmd.exe", "/d", "/c", str(launcher)],
        cwd=str(PACKAGE_DIR),
        input=f"{choice}\n\n",
        capture_output=True,
        text=True,
        timeout=30,
    )
    after = set(results_dir.glob("child_test_result_*.md")) if results_dir.exists() else set()
    created = sorted(after - before)
    latest = created[-1] if created else report_path_from_stdout(completed.stdout)
    return {
        "launcher": str(launcher),
        "choice": choice,
        "exit_code": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
        "created": str(latest) if latest else "",
        "created_count": len(created),
    }


def run_start_here_verify() -> dict[str, Any]:
    status = run_start_here_choice("1")
    expected_count = f"Checked files: {EXPECTED_SELF_CHECK_COUNT}"
    status["expected_checked_files"] = EXPECTED_SELF_CHECK_COUNT
    status["contains_expected_count"] = expected_count in status["stdout"]
    status["passed"] = (
        status["exit_code"] == 0
        and "PASS 67 World package self-check" in status["stdout"]
        and status["contains_expected_count"]
    )
    return status


def run_start_here_document_choice(choice: str, expected_title: str) -> dict[str, Any]:
    status = run_start_here_choice(choice)
    status["contains_title"] = expected_title in status["stdout"]
    status["passed"] = (
        status["exit_code"] == 0
        and status["contains_title"]
        and status["created_count"] == 0
    )
    return status


def run_start_here_create_report() -> dict[str, Any]:
    status = run_start_here_choice("6")
    latest = Path(status["created"]) if status["created"] else None
    latest_text = latest.read_text(encoding="utf-8", errors="replace") if latest else ""
    status["contains_template_title"] = "# 67 World Child-Test Result" in latest_text
    status["contains_package_folder"] = "Package folder:" in latest_text
    status["passed"] = (
        status["exit_code"] == 0
        and "PASS child-test result report created" in status["stdout"]
        and bool(latest)
        and status["contains_template_title"]
        and status["contains_package_folder"]
    )
    status["cleanup"] = cleanup_smoke_artifact(latest)
    status["passed"] = status["passed"] and status["cleanup"]["remaining"] is False
    return status


def packaged_game_processes() -> list[dict[str, Any]]:
    command = (
        "$target=[System.IO.Path]::GetFullPath($env:PACKAGE_EXE); "
        "$items=Get-Process -Name '67-world' -ErrorAction SilentlyContinue | "
        "Where-Object { $_.Path -and ([System.IO.Path]::GetFullPath($_.Path) -ieq $target) } | "
        "Select-Object @{Name='ProcessId';Expression={$_.Id}},@{Name='ExecutablePath';Expression={$_.Path}},@{Name='CommandLine';Expression={''}}; "
        "$items | ConvertTo-Json -Compress"
    )
    env = os.environ.copy()
    env["PACKAGE_EXE"] = str(PACKAGE_DIR / "67-world.exe")
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-Command", command],
        capture_output=True,
        text=True,
        env=env,
        timeout=20,
    )
    if completed.returncode != 0 or not completed.stdout.strip():
        return []
    data = json.loads(completed.stdout)
    if isinstance(data, dict):
        return [data]
    if isinstance(data, list):
        return [item for item in data if isinstance(item, dict)]
    return []


def stop_packaged_game_processes(processes: list[dict[str, Any]]) -> list[int]:
    stopped: list[int] = []
    for process in processes:
        pid = int(process.get("ProcessId") or 0)
        if pid <= 0:
            continue
        subprocess.run(
            ["powershell", "-NoProfile", "-Command", "Stop-Process -Id $env:PID_TO_STOP -Force"],
            capture_output=True,
            text=True,
            env={**os.environ, "PID_TO_STOP": str(pid)},
            timeout=10,
        )
        stopped.append(pid)
    return stopped


def run_start_here_launch_choice(choice: str, expected_flags: list[str]) -> dict[str, Any]:
    before = {int(item.get("ProcessId") or 0) for item in packaged_game_processes()}
    launcher = PACKAGE_DIR / "START_HERE.bat"
    proc = subprocess.Popen(
        ["cmd.exe", "/d", "/c", str(launcher)],
        cwd=str(PACKAGE_DIR),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    assert proc.stdin is not None
    proc.stdin.write(f"{choice}\n")
    proc.stdin.flush()
    proc.stdin.close()
    time.sleep(3.0)
    after = packaged_game_processes()
    launched = [item for item in after if int(item.get("ProcessId") or 0) not in before]
    candidates = launched or after
    stopped = stop_packaged_game_processes(launched)
    timed_out = False
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=3)
            timed_out = True
    stdout, stderr = proc.communicate(timeout=3)
    matching = [item for item in candidates if all(flag in str(item.get("CommandLine") or "") for flag in expected_flags)]
    return {
        "launcher": str(launcher),
        "choice": choice,
        "exit_code": proc.returncode,
        "stdout": stdout,
        "stderr": stderr,
        "timed_out": timed_out,
        "before_pids": sorted(before),
        "after": after,
        "launched": launched,
        "stopped_pids": stopped,
        "expected_flags": expected_flags,
        "matching": matching,
        "passed": bool(launched) and bool(matching) and not timed_out,
    }


def run_package_self_check() -> dict[str, Any]:
    script = PACKAGE_DIR / "VERIFY_PACKAGE.ps1"
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script)],
        cwd=str(PACKAGE_DIR),
        capture_output=True,
        text=True,
        timeout=20,
    )
    expected_count = f"Checked files: {EXPECTED_SELF_CHECK_COUNT}"
    return {
        "script": str(script),
        "exit_code": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
        "expected_checked_files": EXPECTED_SELF_CHECK_COUNT,
        "contains_expected_count": expected_count in completed.stdout,
        "passed": completed.returncode == 0
        and "PASS 67 World package self-check" in completed.stdout
        and expected_count in completed.stdout,
    }


def validate_windows_version_info(exe: Path) -> dict[str, Any]:
    command = (
        "$v=(Get-Item -LiteralPath $env:VERIFY_EXE).VersionInfo; "
        "[pscustomobject]@{"
        "ProductName=$v.ProductName;"
        "FileDescription=$v.FileDescription;"
        "FileVersion=$v.FileVersion;"
        "ProductVersion=$v.ProductVersion;"
        "OriginalFilename=$v.OriginalFilename"
        "} | ConvertTo-Json -Compress"
    )
    env = os.environ.copy()
    env["VERIFY_EXE"] = str(exe)
    completed = subprocess.run(
        ["powershell", "-NoProfile", "-Command", command],
        check=True,
        capture_output=True,
        text=True,
        env=env,
    )
    data = json.loads(completed.stdout)
    expected = {
        "ProductName": "67 World",
        "FileDescription": "67 World native PC game",
        "FileVersion": "1.0.0.0",
        "ProductVersion": "1.0.0.0",
        "OriginalFilename": "67-world.exe",
    }
    return {
        "metadata": data,
        "expected": expected,
        "matches": all(data.get(key) == value for key, value in expected.items()),
    }


def validate_windows_resources(exe: Path) -> dict[str, Any]:
    llvm_readobj = shutil.which("llvm-readobj") or r"C:\Program Files\LLVM\bin\llvm-readobj.exe"
    if not Path(llvm_readobj).exists():
        return {"tool": llvm_readobj, "available": False, "has_icon": False, "has_versioninfo": False}
    completed = subprocess.run(
        [llvm_readobj, "--coff-resources", str(exe)],
        check=True,
        capture_output=True,
        text=True,
    )
    output = completed.stdout
    return {
        "tool": llvm_readobj,
        "available": True,
        "has_icon": "Type: ICON" in output,
        "has_group_icon": "Type: GROUP_ICON" in output,
        "has_versioninfo": "Type: VERSIONINFO" in output,
    }


def tail(path: Path, lines: int = 80) -> str:
    if not path.exists():
        return ""
    content = path.read_text(encoding="utf-8", errors="replace").splitlines()
    return "\n".join(content[-lines:])


def smoke_launch_release(exe: Path, capture_path: Path) -> dict[str, Any]:
    log_dir = ROOT / "build" / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    log_path = log_dir / f"package_release_smoke_{stamp}.log"
    with log_path.open("w", encoding="utf-8", errors="replace") as log:
        log.write(f"command: {exe} --fresh-state --disable-autosave\n")
        log.write(f"cwd: {PACKAGE_DIR}\n\n")
        log.flush()
        proc = subprocess.Popen(
            [str(exe), "--fresh-state", "--disable-autosave"],
            cwd=str(PACKAGE_DIR),
            stdout=log,
            stderr=subprocess.STDOUT,
        )
        time.sleep(4.0)
        still_running = proc.poll() is None
        captured = False
        capture_error = ""
        if still_running:
            try:
                run_capture_screenshot(str(capture_path), process_id=proc.pid)
                captured = capture_path.exists() and capture_path.stat().st_size > 0
            except Exception as exc:  # noqa: BLE001 - smoke report should preserve capture failure text.
                capture_error = str(exc)
        if still_running:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=3)
        return {
            "log": str(log_path),
            "still_running_after_4s": still_running,
            "exit_code": proc.returncode,
            "capture": str(capture_path),
            "captured": captured,
            "capture_error": capture_error,
            "log_tail": tail(log_path),
        }


def visual_health(path: Path) -> dict[str, Any]:
    try:
        health = assert_pixel_health(
            str(path),
            min_unique_colors=64,
            min_unique_buckets=16,
            min_luma_range=48.0,
            min_luma_stdev=12.0,
        )
        mean_ok = 20.0 <= health.luma_mean <= 240.0
        return {
            "passed": mean_ok,
            "summary": health.summary(),
            "luma_mean_ok": mean_ok,
            "luma_mean_bounds": [20.0, 240.0],
        }
    except PixelHealthError as exc:
        details = {"passed": False, "error": str(exc)}
        if path.exists() and path.stat().st_size > 0:
            try:
                health = analyze_png(str(path))
                details["summary"] = health.summary()
            except PixelHealthError:
                pass
        return details


def capture_packaged_framebuffer(exe: Path, capture_path: Path) -> dict[str, Any]:
    log_dir = ROOT / "build" / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    capture_path.parent.mkdir(parents=True, exist_ok=True)
    ppm_path = capture_path.with_suffix(capture_path.suffix + ".ppm")
    if capture_path.exists():
        capture_path.unlink()
    if ppm_path.exists():
        ppm_path.unlink()
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    log_path = log_dir / f"package_framebuffer_capture_{stamp}.log"
    command = [
        str(exe),
        "--fresh-state",
        "--disable-autosave",
        "--capture-framebuffer-once",
        str(ppm_path),
    ]
    with log_path.open("w", encoding="utf-8", errors="replace") as log:
        log.write(f"command: {' '.join(command)}\n")
        log.write(f"cwd: {PACKAGE_DIR}\n\n")
        log.flush()
        completed = subprocess.run(
            command,
            cwd=str(PACKAGE_DIR),
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=30,
        )
    converted = False
    convert_error = ""
    if ppm_path.exists() and ppm_path.stat().st_size > 0:
        try:
            convert_ppm_to_png(str(ppm_path), str(capture_path))
            converted = True
        except Exception as exc:  # noqa: BLE001 - smoke report should preserve conversion failure text.
            convert_error = str(exc)
    health = visual_health(capture_path) if converted else {"passed": False, "error": convert_error or "PPM capture missing"}
    return {
        "command": command,
        "log": str(log_path),
        "exit_code": completed.returncode,
        "ppm": str(ppm_path),
        "ppm_bytes": ppm_path.stat().st_size if ppm_path.exists() else 0,
        "capture": str(capture_path),
        "png_bytes": capture_path.stat().st_size if capture_path.exists() else 0,
        "capture_sha256": sha256(capture_path) if capture_path.exists() and capture_path.stat().st_size > 0 else "",
        "converted": converted,
        "convert_error": convert_error,
        "visual_health": health,
        "passed": completed.returncode == 0 and converted and health["passed"],
        "log_tail": tail(log_path),
    }


def write_smoke_report(report_path: Path, report: dict[str, Any]) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")


def main() -> int:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9280
    capture_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("build/captures/scenarios/package_release_smoke.png")
    if not capture_path.is_absolute():
        capture_path = ROOT / capture_path
    capture_path.parent.mkdir(parents=True, exist_ok=True)
    report_path = Path(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_REPORT_PATH
    if not report_path.is_absolute():
        report_path = ROOT / report_path
    del port  # Release builds intentionally do not expose DevAPI.

    scenario = Scenario(None)  # type: ignore[arg-type]
    report: dict[str, Any] = {
        "schema_version": 1,
        "checked_at": datetime.now().isoformat(),
        "package_dir": str(PACKAGE_DIR),
        "package_zip": str(PACKAGE_ZIP),
        "capture_path": str(capture_path),
        "checks": {},
        "package_hashes": {},
        "passed": False,
    }
    files = package_files()
    for rel, path in files.items():
        scenario.check(f"package file {rel}", path.exists() and path.is_file() and path.stat().st_size > 0, str(path))
    scenario.check("package zip exists", PACKAGE_ZIP.exists() and PACKAGE_ZIP.stat().st_size > 0, str(PACKAGE_ZIP))
    if not scenario.ok:
        report["passed"] = False
        write_smoke_report(report_path, report)
        return finish(False)

    report["package_hashes"] = {
        "67-world.exe": sha256(files["67-world.exe"]),
        "assets/world67_art.ntpack": sha256(files["assets/world67_art.ntpack"]),
        "release_manifest.json": sha256(files["release_manifest.json"]),
        "CHECKSUMS.txt": sha256(files["CHECKSUMS.txt"]),
        "67-world-pc.zip": sha256(PACKAGE_ZIP),
    }
    checksum_status = validate_checksums()
    report["checks"]["checksums"] = checksum_status
    scenario.check("package checksums match", checksum_status["matches"], checksum_status)
    manifest_status = validate_manifest()
    report["checks"]["manifest"] = manifest_status
    scenario.check("package manifest includes child-test acceptance kit", manifest_status["matches"], manifest_status)
    child_test_launcher_status = validate_child_test_launcher(files)
    report["checks"]["child_test_launcher"] = child_test_launcher_status
    scenario.check("child-test fresh launcher uses fresh no-autosave flags", child_test_launcher_status["matches"], child_test_launcher_status)
    start_here_status = validate_start_here_launcher(files)
    report["checks"]["start_here_launcher"] = start_here_status
    scenario.check("start-here launcher exposes package actions", start_here_status["matches"], start_here_status)
    readme_status = validate_readme_guided_path(files)
    report["checks"]["readme_guided_path"] = readme_status
    scenario.check("package readme lists guided child-test menu path", readme_status["matches"], readme_status)
    return_instructions_status = validate_return_instructions(files)
    report["checks"]["return_instructions"] = return_instructions_status
    scenario.check("package return instructions tell tester what to return", return_instructions_status["matches"], return_instructions_status)
    start_here_verify_status = run_start_here_verify()
    report["checks"]["start_here_verify"] = start_here_verify_status
    scenario.check("start-here verify choice runs self-check", start_here_verify_status["passed"], start_here_verify_status)
    start_here_parent_guide_status = run_start_here_document_choice("2", "# 67 World Parent Observer Guide")
    report["checks"]["start_here_parent_guide"] = start_here_parent_guide_status
    scenario.check("start-here parent guide choice shows guide", start_here_parent_guide_status["passed"], start_here_parent_guide_status)
    start_here_acceptance_status = run_start_here_document_choice("3", "# 67 World Child-Test Acceptance Kit")
    report["checks"]["start_here_acceptance"] = start_here_acceptance_status
    scenario.check("start-here acceptance choice shows acceptance kit", start_here_acceptance_status["passed"], start_here_acceptance_status)
    start_here_report_status = run_start_here_create_report()
    report["checks"]["start_here_create_report"] = start_here_report_status
    scenario.check("start-here report choice creates report", start_here_report_status["passed"], start_here_report_status)
    start_here_normal_status = run_start_here_launch_choice("5", [])
    report["checks"]["start_here_normal_launch"] = start_here_normal_status
    scenario.check("start-here normal play choice launches packaged exe", start_here_normal_status["passed"], start_here_normal_status)
    start_here_fresh_status = run_start_here_launch_choice("4", [])
    report["checks"]["start_here_fresh_launch"] = start_here_fresh_status
    scenario.check("start-here child-test choice launches fresh packaged exe", start_here_fresh_status["passed"], start_here_fresh_status)
    report_recorder_status = run_child_test_report_recorder()
    report["checks"]["report_recorder"] = report_recorder_status
    scenario.check("child-test result recorder creates report", report_recorder_status["passed"], report_recorder_status)
    results_export_status = validate_results_export(report_recorder_status["created"])
    report["checks"]["results_export"] = results_export_status
    scenario.check("child-test results export blocks blank and exports filled report", results_export_status["passed"], results_export_status)
    report_validator_status = validate_report_validator(report_recorder_status["created"])
    report["checks"]["report_validator"] = report_validator_status
    scenario.check("child-test report validator rejects blank and accepts filled report", report_validator_status["passed"], report_validator_status)
    report_cleanup_status = validate_smoke_report_cleanup([
        start_here_report_status["created"],
        report_recorder_status["created"],
    ])
    report["checks"]["report_cleanup"] = report_cleanup_status
    scenario.check("package smoke leaves no generated child-test reports", report_cleanup_status["passed"], report_cleanup_status)
    zip_status = validate_zip()
    report["checks"]["zip"] = zip_status
    scenario.check("package zip is valid", zip_status["bad_file"] is None and zip_status["required_present"], zip_status)
    self_check_status = run_package_self_check()
    report["checks"]["self_check"] = self_check_status
    scenario.check("packaged self-check passes", self_check_status["passed"], self_check_status)
    version_status = validate_windows_version_info(files["67-world.exe"])
    report["checks"]["version_info"] = version_status
    scenario.check("packaged exe version metadata", version_status["matches"], version_status)
    resource_status = validate_windows_resources(files["67-world.exe"])
    report["checks"]["windows_resources"] = resource_status
    scenario.check(
        "packaged exe icon/version resources",
        resource_status["has_icon"] and resource_status["has_group_icon"] and resource_status["has_versioninfo"],
        resource_status,
    )
    if not scenario.ok:
        report["passed"] = False
        write_smoke_report(report_path, report)
        return finish(False)

    framebuffer = capture_packaged_framebuffer(files["67-world.exe"], capture_path)
    report["checks"]["framebuffer_visual_proof"] = framebuffer
    scenario.check("packaged release framebuffer visual proof", framebuffer["passed"], framebuffer)

    launch = smoke_launch_release(files["67-world.exe"], capture_path.with_name(capture_path.stem + "_window_probe.png"))
    report["checks"]["launch_probe"] = launch
    scenario.check("packaged release stays open", launch["still_running_after_4s"], launch)

    report["passed"] = scenario.ok
    write_smoke_report(report_path, report)
    return finish(scenario.ok)


if __name__ == "__main__":
    raise SystemExit(main())
