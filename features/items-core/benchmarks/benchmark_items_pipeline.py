#!/usr/bin/env python3
"""Measure the finished Items authoring/build/runtime pipeline."""

from __future__ import annotations

import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[3]


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
        evaluator == {"module": "lupa.lua54", "version": "5.4"}
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
