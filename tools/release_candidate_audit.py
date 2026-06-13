#!/usr/bin/env python3
"""Aggregate release-candidate evidence for 67 World.

This audit is intentionally stricter than a smoke test: automated gates can
pass while release_ready stays false until manual child-test acceptance exists.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PACKAGE_DIR = ROOT / "build" / "release" / "67-world-pc" / "67-world"
PACKAGE_ZIP = ROOT / "build" / "release" / "67-world-pc" / "67-world-pc.zip"
DEFAULT_OUTPUT = ROOT / "build" / "reports" / "release_candidate_audit_v1.json"
PACKAGE_SMOKE_REPORT = ROOT / "build" / "reports" / "package_release_smoke_v2_evidence.json"
CHILD_READINESS_REPORT = ROOT / "build" / "reports" / "child_test_readiness_v29_manual_55.json"
ONE_HOUR_REPORT = ROOT / "build" / "reports" / "one_hour_progression_runtime_v2_balance.json"
ONE_HOUR_SCREENSHOT = ROOT / "build" / "captures" / "scenarios" / "one_hour_progression_runtime_v2_balance.png"
RETURN_BUNDLE_CANDIDATES = [
    PACKAGE_DIR / "child_test_results_for_return.zip",
    PACKAGE_ZIP.parent / "child_test_results_for_return.zip",
]

sys.path.insert(0, str(ROOT / "tools" / "devapi"))
from pixel_health import PixelHealthError, analyze_png, assert_pixel_health  # noqa: E402


REQUIRED_PACKAGE_FILES = [
    "67-world.exe",
    "assets/world67_art.ntpack",
    "START_HERE.bat",
    "RUN_67_WORLD.bat",
    "START_CHILD_TEST_FRESH.bat",
    "CREATE_CHILD_TEST_REPORT.ps1",
    "CREATE_CHILD_TEST_REPORT.bat",
    "VALIDATE_CHILD_TEST_REPORT.ps1",
    "VALIDATE_CHILD_TEST_REPORT.bat",
    "EXPORT_CHILD_TEST_RESULTS.ps1",
    "EXPORT_CHILD_TEST_RESULTS.bat",
    "VERIFY_PACKAGE.ps1",
    "VERIFY_PACKAGE.bat",
    "README.txt",
    "RETURN_INSTRUCTIONS.txt",
    "PARENT_OBSERVER_GUIDE.md",
    "CHILD_TEST_ACCEPTANCE.md",
    "CHILD_TEST_RESULT_TEMPLATE.md",
    "release_manifest.json",
    "CHECKSUMS.txt",
]

EXPECTED_SCREENSHOTS = [
    "build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png",
    str(ONE_HOUR_SCREENSHOT.relative_to(ROOT)).replace("\\", "/"),
    "build/captures/scenarios/package_save_isolation_v1.png",
    "build/captures/scenarios/child_test_readiness_v29_manual_55/child_test_desktop_first_loop.png",
    "build/captures/scenarios/child_test_readiness_v29_manual_55/child_test_desktop_upgrade.png",
    "build/captures/scenarios/child_test_readiness_v29_manual_55/child_test_desktop_stuck.png",
    "build/captures/scenarios/child_test_readiness_v29_manual_55/child_test_portrait_first_loop.png",
    "build/captures/scenarios/child_test_readiness_v29_manual_55/child_test_portrait_stuck.png",
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def passfail(passed: bool, details: dict[str, Any] | None = None) -> dict[str, Any]:
    result = {"passed": bool(passed)}
    if details:
        result.update(details)
    return result


def audit_handoff_reports() -> dict[str, Any]:
    results_dir = PACKAGE_DIR / "child_test_results"
    if not results_dir.exists():
        return passfail(True, {"results_dir": str(results_dir), "exists": False, "reports": [], "unexpected_files": []})

    reports = sorted(results_dir.glob("child_test_result_*.md"))
    expected_names = {path.name for path in reports}
    unexpected_files = [
        path
        for path in results_dir.iterdir()
        if path.is_file() and path.name not in expected_names
    ]
    valid_reports = [path for path in reports if filled_manual_acceptance_report(path)]
    invalid_reports = [path for path in reports if path not in valid_reports]
    return passfail(
        not unexpected_files and not invalid_reports,
        {
            "results_dir": str(results_dir),
            "exists": True,
            "reports": [str(path) for path in reports],
            "valid_reports": [str(path) for path in valid_reports],
            "invalid_reports": [str(path) for path in invalid_reports],
            "unexpected_files": [str(path) for path in unexpected_files],
            "rule": "child_test_results must be absent, or every child_test_result_*.md must be a filled passing acceptance report",
        },
    )


def audit_package() -> dict[str, Any]:
    files = {rel: PACKAGE_DIR / rel for rel in REQUIRED_PACKAGE_FILES}
    file_status = {
        rel: {"exists": path.exists(), "bytes": path.stat().st_size if path.exists() else 0}
        for rel, path in files.items()
    }
    files_ok = all(item["exists"] and item["bytes"] > 0 for item in file_status.values())

    checksum_expected: dict[str, str] = {}
    checksums_path = files["CHECKSUMS.txt"]
    if checksums_path.exists():
        for line in checksums_path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            digest, rel = line.split(None, 1)
            checksum_expected[rel.strip()] = digest
    checksum_actual = {
        rel: sha256(path)
        for rel, path in files.items()
        if rel != "CHECKSUMS.txt" and path.exists()
    }
    checksums_ok = checksum_expected == checksum_actual

    manifest_path = files["release_manifest.json"]
    manifest = read_json(manifest_path) if manifest_path.exists() else {}
    package = manifest.get("package", {}) if isinstance(manifest, dict) else {}
    validation = manifest.get("validation", {}) if isinstance(manifest, dict) else {}
    manifest_file_paths = [
        entry.get("path")
        for entry in package.get("files", [])
        if isinstance(entry, dict)
    ]
    manifest_ok = (
        package.get("executable") == "67-world.exe"
        and package.get("art_pack") == "assets/world67_art.ntpack"
        and package.get("start_here_launcher") == "START_HERE.bat"
        and package.get("child_test_launcher") == "START_CHILD_TEST_FRESH.bat"
        and package.get("child_test_acceptance_kit") == "CHILD_TEST_ACCEPTANCE.md"
        and package.get("child_test_result_template") == "CHILD_TEST_RESULT_TEMPLATE.md"
        and package.get("child_test_report_validator_script") == "VALIDATE_CHILD_TEST_REPORT.ps1"
        and package.get("child_test_report_validator_launcher") == "VALIDATE_CHILD_TEST_REPORT.bat"
        and package.get("child_test_results_export_script") == "EXPORT_CHILD_TEST_RESULTS.ps1"
        and package.get("child_test_results_export_launcher") == "EXPORT_CHILD_TEST_RESULTS.bat"
        and validation.get("requires_child_test_acceptance") is True
        and all(rel in manifest_file_paths for rel in REQUIRED_PACKAGE_FILES if rel not in {"release_manifest.json", "CHECKSUMS.txt"})
    )

    zip_ok = False
    zip_info: dict[str, Any] = {"exists": PACKAGE_ZIP.exists(), "bytes": PACKAGE_ZIP.stat().st_size if PACKAGE_ZIP.exists() else 0}
    if PACKAGE_ZIP.exists():
        with zipfile.ZipFile(PACKAGE_ZIP) as archive:
            names = set(archive.namelist())
            bad_file = archive.testzip()
        required_zip = {f"67-world/{rel}" for rel in REQUIRED_PACKAGE_FILES}
        report_entries = sorted(name for name in names if name.startswith("67-world/child_test_results/"))
        zip_info.update({
            "bad_file": bad_file,
            "required_present": required_zip.issubset(names),
            "entries": len(names),
            "report_entries": report_entries,
        })
        zip_ok = bad_file is None and required_zip.issubset(names) and not report_entries

    child_launcher = files["START_CHILD_TEST_FRESH.bat"].read_text(encoding="utf-8", errors="replace") if files["START_CHILD_TEST_FRESH.bat"].exists() else ""
    child_launcher_ok = "67-world.exe" in child_launcher and "--fresh-state" in child_launcher and "--disable-autosave" in child_launcher
    start_here = files["START_HERE.bat"].read_text(encoding="utf-8", errors="replace") if files["START_HERE.bat"].exists() else ""
    start_here_tokens = [
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
        "choice /C 12345678Q",
    ]
    start_here_ok = all(token in start_here for token in start_here_tokens)
    return_instructions = files["RETURN_INSTRUCTIONS.txt"].read_text(encoding="utf-8", errors="replace") if files["RETURN_INSTRUCTIONS.txt"].exists() else ""
    return_instruction_tokens = [
        "67 World child-test return instructions",
        "Run START_HERE.bat.",
        "Choose [6] Create child-test report",
        "Choose [7] Validate filled child-test report",
        "Choose [8] Export validated child-test results",
        "child_test_results_for_return.zip",
        "Do not return the whole game folder unless asked.",
    ]
    return_instructions_ok = all(token in return_instructions for token in return_instruction_tokens)
    handoff_reports = audit_handoff_reports()

    return passfail(
        files_ok and checksums_ok and manifest_ok and zip_ok and child_launcher_ok and start_here_ok and return_instructions_ok and handoff_reports["passed"],
        {
            "package_dir": str(PACKAGE_DIR),
            "package_zip": str(PACKAGE_ZIP),
            "files": file_status,
            "checksums_match": checksums_ok,
            "manifest_ok": manifest_ok,
            "zip": zip_info,
            "child_test_launcher_ok": child_launcher_ok,
            "start_here_launcher_ok": start_here_ok,
            "start_here_required_tokens": start_here_tokens,
            "return_instructions_ok": return_instructions_ok,
            "return_instruction_required_tokens": return_instruction_tokens,
            "handoff_reports": handoff_reports,
        },
    )


def audit_package_smoke_evidence() -> dict[str, Any]:
    if not PACKAGE_SMOKE_REPORT.exists():
        return passfail(False, {"report": str(PACKAGE_SMOKE_REPORT), "error": "package smoke evidence report is missing"})

    report = read_json(PACKAGE_SMOKE_REPORT)
    package_hashes = report.get("package_hashes", {}) if isinstance(report, dict) else {}
    checks = report.get("checks", {}) if isinstance(report, dict) else {}
    framebuffer = checks.get("framebuffer_visual_proof", {}) if isinstance(checks, dict) else {}
    cleanup = checks.get("report_cleanup", {}) if isinstance(checks, dict) else {}
    launch = checks.get("launch_probe", {}) if isinstance(checks, dict) else {}
    checksums = checks.get("checksums", {}) if isinstance(checks, dict) else {}
    results_export = checks.get("results_export", {}) if isinstance(checks, dict) else {}

    hash_paths = {
        "67-world.exe": PACKAGE_DIR / "67-world.exe",
        "assets/world67_art.ntpack": PACKAGE_DIR / "assets" / "world67_art.ntpack",
        "release_manifest.json": PACKAGE_DIR / "release_manifest.json",
        "CHECKSUMS.txt": PACKAGE_DIR / "CHECKSUMS.txt",
        "67-world-pc.zip": PACKAGE_ZIP,
    }
    missing_hash_paths = {name: str(path) for name, path in hash_paths.items() if not path.exists()}
    if missing_hash_paths:
        return passfail(False, {"report": str(PACKAGE_SMOKE_REPORT), "missing_package_files": missing_hash_paths})

    expected_hashes = {
        name: sha256(path)
        for name, path in hash_paths.items()
    }
    hash_matches = package_hashes == expected_hashes

    capture_path = Path(framebuffer.get("capture") or "")
    if capture_path and not capture_path.is_absolute():
        capture_path = ROOT / capture_path
    capture_expected = ROOT / "build" / "captures" / "scenarios" / "package_release_framebuffer_proof_v2_clean_smoke.png"
    capture_hash_ok = (
        capture_path == capture_expected
        and capture_path.exists()
        and framebuffer.get("capture_sha256") == sha256(capture_path)
    )
    visual_health = framebuffer.get("visual_health", {}) if isinstance(framebuffer, dict) else {}
    passed = (
        report.get("passed") is True
        and hash_matches
        and checksums.get("matches") is True
        and framebuffer.get("passed") is True
        and visual_health.get("passed") is True
        and capture_hash_ok
        and cleanup.get("passed") is True
        and results_export.get("passed") is True
        and launch.get("still_running_after_4s") is True
    )
    return passfail(
        passed,
        {
            "report": str(PACKAGE_SMOKE_REPORT),
            "checked_at": report.get("checked_at"),
            "report_passed": report.get("passed"),
            "hash_matches": hash_matches,
            "expected_hashes": expected_hashes,
            "reported_hashes": package_hashes,
            "capture": str(capture_path),
            "capture_expected": str(capture_expected),
            "capture_hash_ok": capture_hash_ok,
            "framebuffer_visual_proof": framebuffer,
            "report_cleanup": cleanup,
            "results_export": results_export,
            "launch_probe": launch,
        },
    )


def audit_one_hour() -> dict[str, Any]:
    path = ONE_HOUR_REPORT
    report = read_json(path) if path.exists() else {}
    minutes = float(report.get("final_minutes") or -1)
    target_window = report.get("target_window_minutes")
    target_window_ok = target_window == [55.0, 60.0]
    screenshot_value = str(report.get("screenshot") or "")
    screenshot_path = Path(screenshot_value) if screenshot_value else Path()
    if screenshot_value and not screenshot_path.is_absolute():
        screenshot_path = ROOT / screenshot_path
    screenshot_ok = bool(screenshot_value) and screenshot_path.resolve() == ONE_HOUR_SCREENSHOT.resolve() and screenshot_path.exists()
    actions = report.get("actions", {})
    actions_ok = (
        int(actions.get("spawn") or 0) >= 3000
        and int(actions.get("merge") or 0) >= 3000
        and int(actions.get("buy_faster_spawn") or 0) >= 1
        and int(actions.get("buy_better_crate") or 0) >= 20
        and int(actions.get("recycle") or 0) >= 1
        and int(actions.get("tick_passive") or 0) >= 3500
        and int(report.get("max_board_used") or 0) >= 12
    )
    unlocks = report.get("unlock_times_minutes", {})
    cosmic_minutes = float(unlocks.get("cosmic_67") or -1)
    unlock_timing_ok = 55.0 <= cosmic_minutes <= 60.0 and abs(minutes - cosmic_minutes) <= 0.25
    passed = (
        report.get("passed") is True
        and report.get("method") == "native C runtime one-hour progression bot using game_67 action functions"
        and target_window_ok
        and 55.0 <= minutes <= 60.0
        and report.get("final_collection_discovered_count") == 30
        and report.get("final_next_goal") == "WORLD COMPLETE"
        and int(report.get("count_cosmic_67") or 0) >= 1
        and int(report.get("final_better_crate_level") or 0) >= 20
        and screenshot_ok
        and actions_ok
        and unlock_timing_ok
    )
    return passfail(
        passed,
        {
            "report": str(path),
            "final_minutes": minutes,
            "target_window_minutes": target_window,
            "target_window_ok": target_window_ok,
            "collection_discovered_count": report.get("final_collection_discovered_count"),
            "next_goal": report.get("final_next_goal"),
            "better_crate_level": report.get("final_better_crate_level"),
            "count_cosmic_67": report.get("count_cosmic_67"),
            "screenshot": screenshot_value,
            "expected_screenshot": str(ONE_HOUR_SCREENSHOT),
            "screenshot_ok": screenshot_ok,
            "actions": actions,
            "actions_ok": actions_ok,
            "cosmic_unlock_minutes": cosmic_minutes,
            "unlock_timing_ok": unlock_timing_ok,
        },
    )


def audit_child_readiness() -> dict[str, Any]:
    path = CHILD_READINESS_REPORT
    report = read_json(path) if path.exists() else {}
    package = report.get("package", {})
    cue_counts = report.get("audio", {}).get("cue_play_counts", {})
    portrait_cue_counts = report.get("portrait_audio", {}).get("cue_play_counts", {})
    required_cues_ok = all(int(cue_counts.get(name) or 0) >= 1 for name in ["spawn", "merge", "upgrade", "recycle", "blocked"])
    portrait_cues_ok = int(portrait_cue_counts.get("spawn") or 0) >= 2 and int(portrait_cue_counts.get("merge") or 0) >= 1
    current_package_hashes = {
        "exe_sha256": sha256(PACKAGE_DIR / "67-world.exe") if (PACKAGE_DIR / "67-world.exe").exists() else "",
        "art_pack_sha256": sha256(PACKAGE_DIR / "assets" / "world67_art.ntpack") if (PACKAGE_DIR / "assets" / "world67_art.ntpack").exists() else "",
        "manifest_sha256": sha256(PACKAGE_DIR / "release_manifest.json") if (PACKAGE_DIR / "release_manifest.json").exists() else "",
        "checksums_sha256": sha256(PACKAGE_DIR / "CHECKSUMS.txt") if (PACKAGE_DIR / "CHECKSUMS.txt").exists() else "",
        "zip_sha256": sha256(PACKAGE_ZIP) if PACKAGE_ZIP.exists() else "",
    }
    reported_package_hashes = {name: package.get(name, "") for name in current_package_hashes}
    package_hashes_match = reported_package_hashes == current_package_hashes and all(current_package_hashes.values())
    passed = (
        report.get("automated_review_passed") is True
        and report.get("ready_for_manual_child_test") is True
        and report.get("release_ready") is False
        and package.get("ok") is True
        and package_hashes_match
        and report.get("first_loop", {}).get("tutorial_done") is True
        and report.get("first_loop", {}).get("next_goal") == "NEXT BANANA"
        and report.get("stuck_recovery", {}).get("can_spawn_after_recycle") is True
        and required_cues_ok
        and portrait_cues_ok
    )
    return passfail(
        passed,
        {
            "report": str(path),
            "automated_review_passed": report.get("automated_review_passed"),
            "ready_for_manual_child_test": report.get("ready_for_manual_child_test"),
            "release_ready_in_child_readiness": report.get("release_ready"),
            "audio_backend": report.get("audio", {}).get("backend"),
            "audio_cues": cue_counts,
            "portrait_audio_cues": portrait_cue_counts,
            "package_hashes_match": package_hashes_match,
            "reported_package_hashes": reported_package_hashes,
            "current_package_hashes": current_package_hashes,
        },
    )


def audit_save_isolation() -> dict[str, Any]:
    path = ROOT / "build" / "reports" / "package_save_isolation_v1.json"
    report = read_json(path) if path.exists() else {}
    phases = report.get("phases", {})
    normal = phases.get("normal_autosave", {})
    reload = phases.get("normal_reload", {})
    fresh = phases.get("fresh_no_autosave_child_test", {})
    after = phases.get("normal_reload_after_child_test", {})
    passed = (
        report.get("passed") is True
        and normal.get("count_berry_67") == 1
        and normal.get("tutorial_done") is True
        and reload.get("count_berry_67") == 1
        and reload.get("count_banana_67") == 0
        and fresh.get("count_banana_67") == 1
        and after.get("count_berry_67") == 1
        and after.get("count_banana_67") == 0
    )
    return passfail(passed, {"report": str(path), "phases": phases})


def audit_screenshots() -> dict[str, Any]:
    results = {}
    passed = True
    for rel in EXPECTED_SCREENSHOTS:
        path = ROOT / rel
        try:
            if "package_release_framebuffer_proof" in rel:
                health = assert_pixel_health(
                    str(path),
                    min_unique_colors=64,
                    min_unique_buckets=16,
                    min_luma_range=48.0,
                    min_luma_stdev=12.0,
                )
                mean_ok = 20.0 <= health.luma_mean <= 240.0
                results[rel] = {
                    "passed": mean_ok,
                    "summary": health.summary(),
                    "luma_mean_ok": mean_ok,
                    "luma_mean_bounds": [20.0, 240.0],
                }
                passed = passed and mean_ok
            else:
                health = assert_pixel_health(str(path))
                results[rel] = {"passed": True, "summary": health.summary()}
        except PixelHealthError as exc:
            result = {"passed": False, "error": str(exc)}
            if path.exists() and path.stat().st_size > 0:
                try:
                    result["summary"] = analyze_png(str(path)).summary()
                except PixelHealthError:
                    pass
            results[rel] = result
            passed = False
    return passfail(passed, {"screenshots": results})


def filled_manual_acceptance_text(text: str) -> bool:
    def exact_yes(label: str) -> bool:
        return (
            re.search(rf"^- {re.escape(label)}:[ \t]*yes[ \t]*\r?$", text, flags=re.MULTILINE | re.IGNORECASE)
            is not None
        )

    def field(label: str) -> str:
        match = re.search(rf"^- {re.escape(label)}:[ \t]*([^\r\n]*?)[ \t]*\r?$", text, flags=re.MULTILINE)
        return match.group(1).strip() if match else ""

    def field_values(label: str) -> list[str]:
        return [
            match.group(1).strip()
            for match in re.finditer(rf"^- {re.escape(label)}:[ \t]*([^\r\n]*?)[ \t]*\r?$", text, flags=re.MULTILINE)
        ]

    def filled(label: str) -> bool:
        value = field(label)
        return bool(value) and value.lower() not in {"yes / no", "pass / fail / needs tuning"}

    def meaningful(value: str, minimum_length: int) -> bool:
        stripped = value.strip()
        return bool(stripped) and stripped.lower() not in {"none", "n/a", "na", "-", "todo", "placeholder"} and len(
            stripped
        ) >= minimum_length

    def meaningful_count(label: str, minimum_length: int) -> int:
        return sum(1 for value in field_values(label) if meaningful(value, minimum_length))

    def line_value(label: str) -> str:
        match = re.search(rf"^{re.escape(label)}:[ \t]*([^\r\n]*?)[ \t]*\r?$", text, flags=re.MULTILINE)
        return match.group(1).strip() if match else ""

    required_yes = [
        "`VERIFY_PACKAGE.bat` passed before the session",
        "`START_CHILD_TEST_FRESH.bat` was used",
        "Game started at first `TAP BOX` FTUE",
        "Real audio output was enabled",
        "Child found `TAP BOX`",
        "Child understood matching pairs",
        "Child completed first merge",
        "Text was readable from normal distance",
        "Audio feedback was audible",
        "First-minute pass",
        "Child kept spawning/merging without confusion",
        "Child noticed new 67 variants",
        "Child understood the upgrade tile when it showed `BUY`",
        "Child recovered from a full board using `FREE SLOT`",
        "Screen stayed readable and not overloaded",
        "Five-minute pass",
        "Child still understood the next goal near the end",
        "One-hour pass",
        "Spawn sound audible",
        "Merge sound audible",
        "Upgrade sound audible",
        "Blocked/full-board sound audible",
        "Free slot/recycle sound audible",
        "Sounds were pleasant for children",
    ]
    required_fields = [
        "Observer",
        "Child age",
        "Device",
        "Speaker/headphones",
        "Session length in minutes",
        "Minutes played",
        "Highest 67 reached",
        "Collection count",
    ]
    try:
        session_minutes = int(re.search(r"\d+", field("Session length in minutes")).group(0))  # type: ignore[union-attr]
        played_minutes = int(re.search(r"\d+", field("Minutes played")).group(0))  # type: ignore[union-attr]
    except AttributeError:
        return False

    return (
        all(exact_yes(label) for label in required_yes)
        and all(filled(label) for label in required_fields)
        and session_minutes >= 55
        and played_minutes >= 55
        and meaningful_count("Notes", 12) >= 4
        and meaningful(line_value("Observer summary"), 20)
        and re.search(r"^Overall result:[ \t]*pass[ \t]*\r?$", text, flags=re.MULTILINE | re.IGNORECASE)
        is not None
    )


def filled_manual_acceptance_report(path: Path) -> bool:
    return filled_manual_acceptance_text(path.read_text(encoding="utf-8", errors="replace"))


def audit_return_bundle(path: Path) -> dict[str, Any]:
    if not path.exists():
        return passfail(False, {"path": str(path), "exists": False})
    details: dict[str, Any] = {
        "path": str(path),
        "exists": True,
        "bytes": path.stat().st_size,
    }
    try:
        with zipfile.ZipFile(path) as archive:
            names = sorted(archive.namelist())
            bad_file = archive.testzip()
            report_names = [
                name
                for name in names
                if Path(name).name.startswith("child_test_result_") and Path(name).suffix.lower() == ".md"
            ]
            valid_report_names = []
            invalid_report_names = []
            for name in report_names:
                text = archive.read(name).decode("utf-8-sig", errors="replace")
                if filled_manual_acceptance_text(text):
                    valid_report_names.append(name)
                else:
                    invalid_report_names.append(name)
    except (OSError, zipfile.BadZipFile) as exc:
        details["error"] = str(exc)
        return passfail(False, details)

    required_entries = {
        "release_manifest.json",
        "CHECKSUMS.txt",
        "CHILD_TEST_ACCEPTANCE.md",
        "PARENT_OBSERVER_GUIDE.md",
        "RETURN_INSTRUCTIONS.txt",
    }
    present_leaf_names = {Path(name).name for name in names}
    missing_required = sorted(required_entries - present_leaf_names)
    details.update(
        {
            "entries": names,
            "bad_file": bad_file,
            "report_names": report_names,
            "valid_report_names": valid_report_names,
            "invalid_report_names": invalid_report_names,
            "evidence_entries": [name for name in names if name.startswith("evidence/")],
            "missing_required_entries": missing_required,
            "rule": "return bundle must contain package metadata/instructions plus at least one filled passing child_test_result_*.md",
        }
    )
    return passfail(
        bad_file is None and bool(valid_report_names) and not missing_required,
        details,
    )


def audit_manual_acceptance() -> dict[str, Any]:
    results_dir = PACKAGE_DIR / "child_test_results"
    candidates = sorted(results_dir.glob("child_test_result_*.md")) if results_dir.exists() else []
    accepted = [path for path in candidates if filled_manual_acceptance_report(path)]
    return_bundle_results = [audit_return_bundle(path) for path in RETURN_BUNDLE_CANDIDATES]
    accepted_bundles = [result for result in return_bundle_results if result["passed"]]
    return passfail(
        bool(accepted) or bool(accepted_bundles),
        {
            "results_dir": str(results_dir),
            "candidate_count": len(candidates),
            "accepted_reports": [str(path) for path in accepted],
            "return_bundle_candidates": [str(path) for path in RETURN_BUNDLE_CANDIDATES],
            "return_bundles": return_bundle_results,
            "accepted_return_bundles": [result["path"] for result in accepted_bundles],
            "required": "completed child-test report or child_test_results_for_return.zip with setup, first-minute, five-minute, one-hour >=55 minutes, audio, meaningful notes/summary, and Overall result: pass",
        },
    )


def build_audit() -> dict[str, Any]:
    gates = {
        "package": audit_package(),
        "package_smoke_evidence": audit_package_smoke_evidence(),
        "one_hour_progression": audit_one_hour(),
        "child_test_readiness": audit_child_readiness(),
        "save_isolation": audit_save_isolation(),
        "screenshot_health": audit_screenshots(),
        "manual_child_test_acceptance": audit_manual_acceptance(),
    }
    automated_names = [name for name in gates if name != "manual_child_test_acceptance"]
    automated_gates_passed = all(gates[name]["passed"] for name in automated_names)
    manual_passed = gates["manual_child_test_acceptance"]["passed"]
    blockers = []
    if not automated_gates_passed:
        blockers.append("One or more automated release gates failed.")
    if not manual_passed:
        blockers.append("Manual child-test/user acceptance report is missing or incomplete.")
    return {
        "checked_at": datetime.now(timezone.utc).isoformat(),
        "product": "67 World",
        "platform": "native-pc",
        "automated_gates_passed": automated_gates_passed,
        "release_ready": automated_gates_passed and manual_passed,
        "blockers": blockers,
        "gates": gates,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit 67 World release candidate evidence.")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT))
    args = parser.parse_args()
    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)

    audit = build_audit()
    output.write_text(json.dumps(audit, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    print("report:", output)
    print("automated_gates_passed:", audit["automated_gates_passed"])
    print("release_ready:", audit["release_ready"])
    for blocker in audit["blockers"]:
        print("blocker:", blocker)

    # Exit nonzero only when automated evidence is broken. Missing manual
    # acceptance is a release blocker, not an automation failure.
    return 0 if audit["automated_gates_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
