#!/usr/bin/env python3
"""Capture a native game window by PID/HWND and write a PNG.

This intentionally avoids PowerShell and third-party packages so DevAPI visual
automation can run with the same Python runtime as the rest of the harness.
"""

from __future__ import annotations

import argparse
import ctypes
import os
import struct
import sys
import time
import zlib
from ctypes import wintypes


if os.name != "nt":
    raise SystemExit("capture_window.py currently supports Windows only")


user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
dwmapi = ctypes.WinDLL("dwmapi", use_last_error=True)

EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

SW_RESTORE = 9
SRCCOPY = 0x00CC0020
BI_RGB = 0
DIB_RGB_COLORS = 0
HWND_TOPMOST = wintypes.HWND(-1)
HWND_NOTOPMOST = wintypes.HWND(-2)
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_SHOWWINDOW = 0x0040
DWMWA_EXTENDED_FRAME_BOUNDS = 9


class RECT(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long),
    ]


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", wintypes.LONG),
        ("biHeight", wintypes.LONG),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wintypes.DWORD * 3)]


def raise_last_error(action: str) -> None:
    err = ctypes.get_last_error()
    raise RuntimeError(f"{action} failed with Win32 error {err}")


def find_window_for_pid(pid: int) -> int:
    matches: list[int] = []

    @EnumWindowsProc
    def callback(hwnd: int, _param: int) -> bool:
        proc_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(proc_id))
        if proc_id.value == pid and user32.IsWindowVisible(hwnd):
            rect = RECT()
            if user32.GetWindowRect(hwnd, ctypes.byref(rect)):
                if rect.right > rect.left and rect.bottom > rect.top:
                    matches.append(hwnd)
        return True

    if not user32.EnumWindows(callback, 0):
        raise_last_error("EnumWindows")
    if not matches:
        raise RuntimeError(f"no visible top-level window found for PID {pid}")
    return matches[0]


def bring_window_forward(hwnd: int) -> RECT:
    user32.ShowWindow(hwnd, SW_RESTORE)
    user32.SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW)
    user32.SetForegroundWindow(hwnd)
    time.sleep(1.0)
    rect = RECT()
    if dwmapi.DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, ctypes.byref(rect), ctypes.sizeof(rect)) != 0:
        if not user32.GetWindowRect(hwnd, ctypes.byref(rect)):
            raise_last_error("GetWindowRect")
    if rect.right <= rect.left or rect.bottom <= rect.top:
        raise RuntimeError("target window has an empty rectangle")
    return rect


def release_topmost(hwnd: int) -> None:
    user32.SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)


def capture_region(x: int, y: int, width: int, height: int) -> bytes:
    screen_dc = user32.GetDC(None)
    if not screen_dc:
        raise_last_error("GetDC")
    mem_dc = gdi32.CreateCompatibleDC(screen_dc)
    if not mem_dc:
        user32.ReleaseDC(None, screen_dc)
        raise_last_error("CreateCompatibleDC")
    bitmap = gdi32.CreateCompatibleBitmap(screen_dc, width, height)
    if not bitmap:
        gdi32.DeleteDC(mem_dc)
        user32.ReleaseDC(None, screen_dc)
        raise_last_error("CreateCompatibleBitmap")

    old_obj = gdi32.SelectObject(mem_dc, bitmap)
    try:
        if not gdi32.BitBlt(mem_dc, 0, 0, width, height, screen_dc, x, y, SRCCOPY):
            raise_last_error("BitBlt")

        info = BITMAPINFO()
        info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        info.bmiHeader.biWidth = width
        info.bmiHeader.biHeight = -height
        info.bmiHeader.biPlanes = 1
        info.bmiHeader.biBitCount = 32
        info.bmiHeader.biCompression = BI_RGB

        raw = ctypes.create_string_buffer(width * height * 4)
        lines = gdi32.GetDIBits(mem_dc, bitmap, 0, height, raw, ctypes.byref(info), DIB_RGB_COLORS)
        if lines != height:
            raise_last_error("GetDIBits")
        return raw.raw
    finally:
        if old_obj:
            gdi32.SelectObject(mem_dc, old_obj)
        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(mem_dc)
        user32.ReleaseDC(None, screen_dc)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_png(path: str, width: int, height: int, bgra: bytes) -> None:
    rows = []
    stride = width * 4
    for y in range(height):
        row = bgra[y * stride : (y + 1) * stride]
        rgb = bytearray(width * 3)
        for x in range(width):
            b = row[x * 4]
            g = row[x * 4 + 1]
            r = row[x * 4 + 2]
            rgb[x * 3 : x * 3 + 3] = bytes((r, g, b))
        rows.append(b"\x00" + bytes(rgb))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    data = b"".join([
        b"\x89PNG\r\n\x1a\n",
        png_chunk(b"IHDR", ihdr),
        png_chunk(b"IDAT", zlib.compress(b"".join(rows), 6)),
        png_chunk(b"IEND", b""),
    ])
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="build/captures/screenshot.png")
    parser.add_argument("--process-id", type=int, default=0)
    parser.add_argument("--x", type=int, default=0)
    parser.add_argument("--y", type=int, default=0)
    parser.add_argument("--width", type=int, default=0)
    parser.add_argument("--height", type=int, default=0)
    args = parser.parse_args()

    hwnd = 0
    try:
        if args.process_id > 0:
            hwnd = find_window_for_pid(args.process_id)
            rect = bring_window_forward(hwnd)
            x = rect.left
            y = rect.top
            width = rect.right - rect.left
            height = rect.bottom - rect.top
        else:
            x = args.x
            y = args.y
            width = args.width
            height = args.height
            if width <= 0 or height <= 0:
                raise RuntimeError("pass --process-id or an explicit --x/--y/--width/--height region")

        pixels = capture_region(x, y, width, height)
        write_png(args.output, width, height, pixels)
        print(os.path.abspath(args.output))
        return 0
    finally:
        if hwnd:
            release_topmost(hwnd)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"FAIL capture_window: {exc}", file=sys.stderr)
        raise SystemExit(1)
