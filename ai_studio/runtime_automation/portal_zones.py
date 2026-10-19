#!/usr/bin/env python3
"""Check a captured ui.tree against a publish target's reserved zones.

Reserved zones are portal overlays drawn over the game frame (the Poki Pill).
They live as data in
features/platform-sdk/publish-targets/<target>.json under metadata.reserved_zones,
in CSS px of the game frame, which equals native window px.
All rects here are window px, origin top-left, Y-down: (left, top, right, bottom).
"""

from __future__ import annotations

import fnmatch
import json
from pathlib import Path
from typing import Any, Iterable

STUDIO_ROOT = Path(__file__).resolve().parents[2]
PUBLISH_TARGETS = STUDIO_ROOT / "features" / "platform-sdk" / "publish-targets"

Rect = tuple[float, float, float, float]


class PortalZoneError(ValueError):
    pass


def load_target_zones(target: str, targets_dir: Path = PUBLISH_TARGETS) -> list[dict[str, Any]]:
    path = Path(targets_dir) / f"{target}.json"
    if not path.is_file():
        raise PortalZoneError(f"unknown publish target: {target} ({path} not found)")
    manifest = json.loads(path.read_text(encoding="utf-8"))
    zones = (manifest.get("metadata") or {}).get("reserved_zones", [])
    if not isinstance(zones, list):
        raise PortalZoneError(f"{target} reserved_zones must be a list")
    for zone in zones:
        if not isinstance(zone, dict) or zone.get("anchor") not in ("top-left", "top-right") or not zone.get("sizes"):
            raise PortalZoneError(f"{target} reserved zone is invalid: {zone!r}")
    return zones


def zone_size(zone: dict[str, Any], frame_width: float) -> dict[str, Any]:
    for size in zone["sizes"]:
        low = size.get("min_frame_width_px")
        high = size.get("max_frame_width_px")
        if (low is None or frame_width >= low) and (high is None or frame_width <= high):
            return size
    raise PortalZoneError(f"zone {zone.get('id')} has no size for frame width {frame_width}")


def zone_rect(zone: dict[str, Any], window_w: float, window_h: float) -> Rect:
    """Zone rect at its default position, clipped to the window."""
    size = zone_size(zone, window_w)
    top = float(zone.get("top_px", 0))
    width = float(size["width_px"])
    left = 0.0 if zone["anchor"] == "top-left" else window_w - width
    right = left + width
    bottom = top + float(size["height_px"])
    return (max(0.0, left), max(0.0, top), min(float(window_w), right), min(float(window_h), bottom))


def layout_to_window(tree: dict[str, Any]) -> tuple[float, float, float, float] | None:
    """(scale_x, offset_x, scale_y, offset_y) from Y-down layout px to window px, or None without a 2D viewport.

    The engine's viewport block is the device image of Y-down layout (0,0) and (width,height);
    device px are framebuffer px, so dividing by dpr gives window px.
    """
    lw = tree.get("width")
    lh = tree.get("height")
    if not isinstance(lw, (int, float)) or not isinstance(lh, (int, float)) or lw <= 0 or lh <= 0:
        return None
    vp = tree.get("viewport")
    if not isinstance(vp, dict):
        return None
    dpr = tree.get("dpr")
    dpr = float(dpr) if isinstance(dpr, (int, float)) and dpr > 0 else 1.0
    try:
        vx, vy, vw, vh = (float(vp[key]) for key in ("x", "y", "w", "h"))
    except (KeyError, TypeError, ValueError):
        return None
    return (vw / lw / dpr, vx / dpr, vh / lh / dpr, vy / dpr)


def node_window_rect(bounds: dict[str, Any], tree: dict[str, Any]) -> Rect | None:
    """Map Y-up layout bounds {x,y,w,h} to a normalised window rect."""
    mapping = layout_to_window(tree)
    if mapping is None:
        return None
    sx, ox, sy, oy = mapping
    lh = float(tree["height"])
    x, y, w, h = (float(bounds[key]) for key in ("x", "y", "w", "h"))
    x0, x1 = ox + x * sx, ox + (x + w) * sx
    # Y-up layout -> Y-down layout is lh - y; mapping both corners and sorting tolerates a flipped viewport (h < 0).
    y0, y1 = oy + (lh - (y + h)) * sy, oy + (lh - y) * sy
    return (min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1))

BACKDROP_WIDTH_SHARE = 0.9


# Overlaps thinner than half a pixel are float noise at a shared edge, not a covered element.
TOUCH_TOLERANCE_PX = 0.5


def intersects(a: Rect, b: Rect) -> bool:
    return (min(a[2], b[2]) - max(a[0], b[0]) > TOUCH_TOLERANCE_PX
            and min(a[3], b[3]) - max(a[1], b[1]) > TOUCH_TOLERANCE_PX)


def _is_visible_leaf(node: dict[str, Any]) -> bool:
    bounds = node.get("bounds")
    if node.get("child_count") != 0 or node.get("visible") is False or not isinstance(bounds, dict):
        return False
    try:
        return float(bounds["w"]) > 0 and float(bounds["h"]) > 0
    except (KeyError, TypeError, ValueError):
        return False


def _ignored(node: dict[str, Any], patterns: Iterable[str]) -> bool:
    id_string = node.get("id_string")
    return isinstance(id_string, str) and bool(id_string) and any(fnmatch.fnmatchcase(id_string, pattern) for pattern in patterns)


def check_tree(
    tree: dict[str, Any],
    zones: list[dict[str, Any]],
    window_w: float,
    window_h: float,
    *,
    ignore: Iterable[str] = (),
) -> dict[str, Any]:
    """Report visible ui leaves whose window rect overlaps a reserved zone."""
    patterns = list(ignore)
    zone_rects = [{"id": zone.get("id"), "rect": list(zone_rect(zone, window_w, window_h))} for zone in zones]
    result: dict[str, Any] = {"zones": zone_rects, "violations": [], "checked_leaves": 0, "backdrops": 0}
    if zones and layout_to_window(tree) is None:
        result["error"] = "ui.tree has no 2D viewport block; zone overlap cannot be mapped"
        return result
    nodes = tree.get("nodes") if isinstance(tree.get("nodes"), list) else []
    for node in nodes:
        if not isinstance(node, dict) or not _is_visible_leaf(node) or _ignored(node, patterns):
            continue
        rect = node_window_rect(node["bounds"], tree)
        if rect is None:
            continue
        # A leaf spanning the window is a backdrop (root panel, scrim); the
        # text and controls drawn over it are leaves of their own and are checked.
        if rect[2] - rect[0] >= BACKDROP_WIDTH_SHARE * window_w:
            result["backdrops"] += 1
            continue
        result["checked_leaves"] += 1
        for zone in zone_rects:
            if intersects(rect, tuple(zone["rect"])):
                result["violations"].append({
                    "zone": zone["id"],
                    "zone_rect": zone["rect"],
                    "id": node.get("id"),
                    "id_string": node.get("id_string", ""),
                    "role": node.get("role", ""),
                    "text": node.get("text", ""),
                    "rect": [round(value, 2) for value in rect],
                })
    return result
