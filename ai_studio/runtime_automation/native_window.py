"""Native window geometry helpers for deterministic off-screen capture."""

from __future__ import annotations

import os


def resize_and_park_client(process_id: int | None, width: int, height: int) -> bool:
    """Resize a visible Win32 client beyond monitor clamps and park it off-screen.

    The framebuffer is probed separately after this request; a successful Win32
    call is never treated as proof of the resulting render size.
    """
    if os.name != "nt" or not process_id:
        return False
    import ctypes
    from capture_window import find_window_for_pid

    hwnd = find_window_for_pid(process_id)
    if not hwnd:
        return False
    user32 = ctypes.windll.user32
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE; minimized windows do not render.

    class RECT(ctypes.Structure):
        _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                    ("right", ctypes.c_long), ("bottom", ctypes.c_long)]

    rect = RECT(0, 0, width, height)
    style = user32.GetWindowLongW(hwnd, -16)  # GWL_STYLE
    if not user32.AdjustWindowRect(ctypes.byref(rect), style, False):
        return False
    outer_width = rect.right - rect.left
    outer_height = rect.bottom - rect.top
    # SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING bypasses GLFW's
    # monitor max-track clamp while preserving an active, capturable surface.
    flags = 0x0004 | 0x0010 | 0x0400
    # Use a coordinate well beyond multi-monitor desktops. A merely adjacent
    # x-position can still intersect a second display and re-enable max-track
    # clamping on tall portrait clients.
    user32.SetWindowPos(hwnd, 0, 10000, -30, 0, 0, flags | 0x0001)  # SWP_NOSIZE
    return bool(user32.SetWindowPos(hwnd, 0, 10000, -30, outer_width, outer_height, flags))
