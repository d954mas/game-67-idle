#!/usr/bin/env python3
"""Capture a native game window by PID/HWND and write a PNG.

This intentionally avoids PowerShell and third-party packages so DevAPI visual
automation can run with the same Python runtime as the rest of the harness.
"""

from __future__ import annotations

import argparse
import ctypes
import os
import sys
import time
from ctypes import wintypes

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from png_io import write_png_rgb  # noqa: E402


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
HWND_BOTTOM = wintypes.HWND(1)
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_NOACTIVATE = 0x0010
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


# ctypes defaults undeclared Win32 calls to 32-bit ``int`` arguments/results.
# That truncates GDI handles on 64-bit Python and can produce either ERROR_INVALID_HANDLE
# or a successful all-black BitBlt. Declare every handle-bearing call used below.
user32.EnumWindows.argtypes = [EnumWindowsProc, wintypes.LPARAM]
user32.EnumWindows.restype = wintypes.BOOL
user32.GetWindowThreadProcessId.argtypes = [
    wintypes.HWND,
    ctypes.POINTER(wintypes.DWORD),
]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.IsWindowVisible.restype = wintypes.BOOL
user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(RECT)]
user32.GetWindowRect.restype = wintypes.BOOL
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.ShowWindow.restype = wintypes.BOOL
user32.SetWindowPos.argtypes = [
    wintypes.HWND,
    wintypes.HWND,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.UINT,
]
user32.SetWindowPos.restype = wintypes.BOOL
user32.SetForegroundWindow.argtypes = [wintypes.HWND]
user32.SetForegroundWindow.restype = wintypes.BOOL
user32.EnableWindow.argtypes = [wintypes.HWND, wintypes.BOOL]
user32.EnableWindow.restype = wintypes.BOOL
user32.GetDC.argtypes = [wintypes.HWND]
user32.GetDC.restype = wintypes.HDC
user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
user32.ReleaseDC.restype = ctypes.c_int

dwmapi.DwmGetWindowAttribute.argtypes = [
    wintypes.HWND,
    wintypes.DWORD,
    wintypes.LPVOID,
    wintypes.DWORD,
]
dwmapi.DwmGetWindowAttribute.restype = ctypes.c_long

gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi32.CreateCompatibleDC.restype = wintypes.HDC
gdi32.CreateCompatibleBitmap.argtypes = [
    wintypes.HDC,
    ctypes.c_int,
    ctypes.c_int,
]
gdi32.CreateCompatibleBitmap.restype = wintypes.HBITMAP
gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.BitBlt.argtypes = [
    wintypes.HDC,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.HDC,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.DWORD,
]
gdi32.BitBlt.restype = wintypes.BOOL
gdi32.GetDIBits.argtypes = [
    wintypes.HDC,
    wintypes.HBITMAP,
    wintypes.UINT,
    wintypes.UINT,
    wintypes.LPVOID,
    ctypes.POINTER(BITMAPINFO),
    wintypes.UINT,
]
gdi32.GetDIBits.restype = ctypes.c_int
gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
gdi32.DeleteObject.restype = wintypes.BOOL
gdi32.DeleteDC.argtypes = [wintypes.HDC]
gdi32.DeleteDC.restype = wintypes.BOOL


def raise_last_error(action: str) -> None:
    err = ctypes.get_last_error()
    raise RuntimeError(f"{action} failed with Win32 error {err}")


def find_window_for_pid(pid: int) -> int:
    matches: list[tuple[int, int]] = []

    @EnumWindowsProc
    def callback(hwnd: int, _param: int) -> bool:
        proc_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(proc_id))
        if proc_id.value == pid and user32.IsWindowVisible(hwnd):
            rect = RECT()
            if user32.GetWindowRect(hwnd, ctypes.byref(rect)):
                width = rect.right - rect.left
                height = rect.bottom - rect.top
                if width > 0 and height > 0:
                    matches.append((width * height, hwnd))
        return True

    if not user32.EnumWindows(callback, 0):
        raise_last_error("EnumWindows")
    if not matches:
        raise RuntimeError(f"no visible top-level window found for PID {pid}")
    return max(matches, key=lambda item: item[0])[1]


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


def background_window(hwnd: int) -> None:
    user32.EnableWindow(hwnd, False)
    if not user32.SetWindowPos(
        hwnd,
        HWND_BOTTOM,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW,
    ):
        raise_last_error("SetWindowPos")


def restore_window_interaction(hwnd: int) -> None:
    user32.EnableWindow(hwnd, True)


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


def write_png(path: str, width: int, height: int, bgra: bytes) -> None:
    # GDI hands back top-down BGRA; convert to packed RGB, then let png_io own
    # the PNG framing (single source shared with devapi_client/pixel_health).
    stride = width * 4
    rgb = bytearray(width * height * 3)
    for y in range(height):
        row = bgra[y * stride : (y + 1) * stride]
        base = y * width * 3
        for x in range(width):
            rgb[base + x * 3] = row[x * 4 + 2]
            rgb[base + x * 3 + 1] = row[x * 4 + 1]
            rgb[base + x * 3 + 2] = row[x * 4]
    write_png_rgb(path, width, height, bytes(rgb))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="tmp/captures/screenshot.png")
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
