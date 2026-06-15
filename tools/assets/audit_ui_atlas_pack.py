#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from time import perf_counter
from typing import Any

from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

try:
    import numpy as np
except ImportError:  # pragma: no cover - fallback keeps the tool portable.
    np = None

try:
    from tools.assets.atomic_io import write_json_atomic, write_text_atomic
except ModuleNotFoundError:  # pragma: no cover - supports direct script execution by path.
    from atomic_io import write_json_atomic, write_text_atomic


ROOT = Path.cwd()
DEFAULT_LABEL_FONT_SIZE = 18
LABEL_PAD_X = 4
LABEL_PAD_Y = 2
LABEL_LINE_GAP_Y = 2
LABEL_OUTER_MARGIN = 8
_LABEL_FONTS: dict[int, ImageFont.ImageFont] = {}


def analysis_engine() -> str:
    return "numpy" if np is not None else "python"


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


def project_path(value: str) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return ROOT / path


def norm_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    write_json_atomic(path, data)


def write_text(path: Path, text: str) -> None:
    write_text_atomic(path, text)


def rect_valid(rect: Any) -> bool:
    return (
        isinstance(rect, list)
        and len(rect) == 4
        and all(isinstance(value, (int, float)) for value in rect)
        and rect[0] >= 0
        and rect[1] >= 0
        and rect[2] > 0
        and rect[3] > 0
    )


def rect_tuple(rect: list[int | float]) -> tuple[int, int, int, int]:
    return tuple(int(value) for value in rect)  # type: ignore[return-value]


def intersects(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> bool:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and ax + aw > bx and ay < by + bh and ay + ah > by


def rect_has_visible_pixel(image: Image.Image, rect: tuple[int, int, int, int]) -> bool:
    x, y, width, height = rect
    alpha = image.getchannel("A")
    pixels = alpha.load()
    for py in range(y, y + height):
        for px in range(x, x + width):
            if pixels[px, py] > 0:
                return True
    return False


def transparent_nonzero_rgb_count(image: Image.Image) -> int:
    if np is not None:
        array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
        alpha = array[..., 3]
        rgb_nonzero = np.any(array[..., :3] != 0, axis=2)
        return int(np.count_nonzero((alpha == 0) & rgb_nonzero))
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    count = 0
    for y in range(rgba.height):
        for x in range(rgba.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha == 0 and (red != 0 or green != 0 or blue != 0):
                count += 1
    return count


def visible_pixels_outside_rects_count(image: Image.Image, allowed_rects: list[tuple[int, int, int, int]]) -> int:
    rgba = image.convert("RGBA")
    if np is not None:
        array = np.asarray(rgba, dtype=np.uint8)
        visible = array[..., 3] > 0
        allowed = np.zeros(visible.shape, dtype=bool)
        for x, y, width, height in allowed_rects:
            x0 = max(0, x)
            y0 = max(0, y)
            x1 = min(rgba.width, x + width)
            y1 = min(rgba.height, y + height)
            if x0 < x1 and y0 < y1:
                allowed[y0:y1, x0:x1] = True
        return int(np.count_nonzero(visible & ~allowed))

    pixels = rgba.load()
    count = 0
    for y in range(rgba.height):
        for x in range(rgba.width):
            if pixels[x, y][3] == 0:
                continue
            if not any(rx <= x < rx + rw and ry <= y < ry + rh for rx, ry, rw, rh in allowed_rects):
                count += 1
    return count


def changed_pixels_outside_rects_count(
    base: Image.Image, overlay: Image.Image, allowed_rects: list[tuple[int, int, int, int]]
) -> int:
    base_rgba = base.convert("RGBA")
    overlay_rgba = overlay.convert("RGBA")
    if base_rgba.size != overlay_rgba.size:
        return 0
    if np is not None:
        base_array = np.asarray(base_rgba, dtype=np.uint8)
        overlay_array = np.asarray(overlay_rgba, dtype=np.uint8)
        changed = np.any(base_array != overlay_array, axis=2)
        allowed = np.zeros(changed.shape, dtype=bool)
        for x, y, width, height in allowed_rects:
            x0 = max(0, x)
            y0 = max(0, y)
            x1 = min(base_rgba.width, x + width)
            y1 = min(base_rgba.height, y + height)
            if x0 < x1 and y0 < y1:
                allowed[y0:y1, x0:x1] = True
        return int(np.count_nonzero(changed & ~allowed))

    base_pixels = base_rgba.load()
    overlay_pixels = overlay_rgba.load()
    count = 0
    for y in range(base_rgba.height):
        for x in range(base_rgba.width):
            if base_pixels[x, y] == overlay_pixels[x, y]:
                continue
            if not any(rx <= x < rx + rw and ry <= y < ry + rh for rx, ry, rw, rh in allowed_rects):
                count += 1
    return count


def label_font(font_size: int = DEFAULT_LABEL_FONT_SIZE) -> ImageFont.ImageFont:
    if font_size in _LABEL_FONTS:
        return _LABEL_FONTS[font_size]
    for name in ("DejaVuSans.ttf", "Arial.ttf"):
        try:
            _LABEL_FONTS[font_size] = ImageFont.truetype(name, font_size)
            return _LABEL_FONTS[font_size]
        except OSError:
            continue
    _LABEL_FONTS[font_size] = ImageFont.load_default()
    return _LABEL_FONTS[font_size]


def measure_label(label: str, font_size: int = DEFAULT_LABEL_FONT_SIZE) -> tuple[int, int]:
    probe = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    draw = ImageDraw.Draw(probe)
    bbox = draw.textbbox((0, 0), label, font=label_font(font_size))
    return int(bbox[2] - bbox[0]), int(bbox[3] - bbox[1])


def check_review_label_lines(entry_id: str, review_label: dict[str, Any], label_rect: tuple[int, int, int, int]) -> list[str]:
    problems: list[str] = []
    _, _, label_width, label_height = label_rect
    raw_lines = review_label.get("lines")
    if raw_lines is None:
        raw_lines = [review_label.get("text") or entry_id]
    if not isinstance(raw_lines, list) or not raw_lines or not all(isinstance(line, str) and line for line in raw_lines):
        return [f"{entry_id} review_label lines must be non-empty strings"]
    raw_font_size = review_label.get("font_size", DEFAULT_LABEL_FONT_SIZE)
    if not isinstance(raw_font_size, int) or raw_font_size < 8:
        return [f"{entry_id} review_label font_size must be an integer >= 8"]
    line_sizes = [measure_label(line, raw_font_size) for line in raw_lines]
    text_width = max((width for width, _ in line_sizes), default=0)
    text_line_height = max((height for _, height in line_sizes), default=0)
    required_width = text_width + LABEL_PAD_X * 2
    required_height = text_line_height * len(raw_lines) + LABEL_LINE_GAP_Y * max(0, len(raw_lines) - 1) + LABEL_PAD_Y * 2
    if required_width > label_width:
        problems.append(f"{entry_id} review_label lines exceed review_label rect width")
    if required_height > label_height:
        problems.append(f"{entry_id} review_label lines exceed review_label rect height")
    return problems


def expected_review_label_text(entry_id: str, entries_by_id: dict[str, dict[str, Any]], expected_assets: dict[str, dict[str, Any]]) -> str:
    alias_ids: set[str] = set()
    for other_id, other in entries_by_id.items():
        if other_id != entry_id and other.get("alias_of") == entry_id:
            alias_ids.add(other_id)
    for asset_id, asset in expected_assets.items():
        if asset_id != entry_id and asset.get("alias_of") == entry_id:
            alias_ids.add(asset_id)
    if not alias_ids:
        return entry_id
    return f"{entry_id} (+{','.join(sorted(alias_ids))})"


def check_labeled_preview_policy(owner: str, value: Any) -> list[str]:
    if not isinstance(value, dict):
        return [f"{owner} labeled_preview_policy must be present"]
    problems: list[str] = []
    if value.get("mode") != "label_overlay_only":
        problems.append(f"{owner} labeled_preview_policy.mode must be label_overlay_only")
    if value.get("allowed_delta") != "review_label_rects_only":
        problems.append(f"{owner} labeled_preview_policy.allowed_delta must be review_label_rects_only")
    if value.get("debug_outlines") is not False:
        problems.append(f"{owner} labeled_preview_policy.debug_outlines must be false")
    return problems


def check_extrusion(atlas: Image.Image, entry: dict[str, Any]) -> list[str]:
    problems: list[str] = []
    entry_id = str(entry.get("id") or "(unknown)")
    atlas_rect = rect_tuple(entry["atlas_rect"])
    padded_rect = rect_tuple(entry["padded_rect"])
    ax, ay, aw, ah = atlas_rect
    px, py, pw, ph = padded_rect
    extrude = int(entry.get("extrude") or 0)
    if extrude < 1:
        problems.append(f"{entry_id} extrude must be >= 1")
        return problems
    if (ax, ay, aw, ah) != (px + extrude, py + extrude, pw - extrude * 2, ph - extrude * 2):
        problems.append(f"{entry_id} atlas_rect must be inset from padded_rect by extrude")
        return problems

    top_edge_y = ay
    bottom_edge_y = ay + ah - 1
    left_edge_x = ax
    right_edge_x = ax + aw - 1
    for offset in range(extrude):
        y_top = py + offset
        y_bottom = ay + ah + offset
        for x in range(ax, ax + aw):
            if atlas.getpixel((x, y_top)) != atlas.getpixel((x, top_edge_y)):
                problems.append(f"{entry_id} top extrusion pixel mismatch at {x},{y_top}")
                return problems
            if atlas.getpixel((x, y_bottom)) != atlas.getpixel((x, bottom_edge_y)):
                problems.append(f"{entry_id} bottom extrusion pixel mismatch at {x},{y_bottom}")
                return problems
        x_left = px + offset
        x_right = ax + aw + offset
        for y in range(ay, ay + ah):
            if atlas.getpixel((x_left, y)) != atlas.getpixel((left_edge_x, y)):
                problems.append(f"{entry_id} left extrusion pixel mismatch at {x_left},{y}")
                return problems
            if atlas.getpixel((x_right, y)) != atlas.getpixel((right_edge_x, y)):
                problems.append(f"{entry_id} right extrusion pixel mismatch at {x_right},{y}")
                return problems
    return problems


def audit_pack(pack_path: Path, asset_manifest_path: Path | None = None, profile: bool = False) -> dict[str, Any]:
    started = perf_counter()
    pack = read_json(pack_path)
    timings: dict[str, float] = {}
    problems: list[str] = []
    if pack.get("schema") != "game.ui_atlas_pack":
        problems.append("atlas pack schema must be game.ui_atlas_pack")

    manifest_path = asset_manifest_path
    if manifest_path is None and isinstance(pack.get("asset_manifest"), str):
        manifest_path = project_path(pack["asset_manifest"])
    expected_assets: dict[str, dict[str, Any]] = {}
    if manifest_path:
        if not manifest_path.exists():
            problems.append(f"asset manifest missing: {norm_path(manifest_path)}")
        else:
            manifest = read_json(manifest_path)
            if manifest.get("schema") != "game.asset_manifest":
                problems.append("asset manifest schema must be game.asset_manifest")
            for asset in manifest.get("assets", []):
                if isinstance(asset, dict) and isinstance(asset.get("id"), str):
                    expected_assets[asset["id"]] = asset
            if isinstance(pack.get("asset_manifest"), str) and norm_path(manifest_path) != pack["asset_manifest"]:
                problems.append("pack asset_manifest must match provided asset manifest")

    reported_ids: set[str] = set()
    atlas_reports: list[dict[str, Any]] = []
    atlases = pack.get("atlases")
    if not isinstance(atlases, list) or not atlases:
        problems.append("atlas pack needs non-empty atlases")
        atlases = []
    labeled_preview_policy = None
    if pack.get("label_overlay"):
        labeled_preview_policy = pack.get("labeled_preview_policy")
        problems.extend(check_labeled_preview_policy("atlas pack", labeled_preview_policy))

    for atlas_info in atlases:
        atlas_started = perf_counter()
        atlas_problems: list[str] = []
        atlas_path_value = atlas_info.get("path") if isinstance(atlas_info, dict) else None
        atlas_path = project_path(atlas_path_value) if isinstance(atlas_path_value, str) else None
        atlas = None
        if atlas_path is None:
            atlas_problems.append("atlas needs path")
        elif not atlas_path.exists():
            atlas_problems.append(f"atlas image missing: {atlas_path_value}")
        else:
            atlas = Image.open(atlas_path).convert("RGBA")

        labeled_preview = None
        labeled_preview_path_value = atlas_info.get("labeled_preview_path") if isinstance(atlas_info, dict) else None
        if atlas_info.get("label_overlay"):
            atlas_problems.extend(check_labeled_preview_policy("atlas", atlas_info.get("labeled_preview_policy")))
            if not isinstance(labeled_preview_path_value, str) or not labeled_preview_path_value:
                atlas_problems.append("labeled review atlas needs labeled_preview_path")
            else:
                labeled_preview_path = project_path(labeled_preview_path_value)
                if not labeled_preview_path.exists():
                    atlas_problems.append(f"labeled preview image missing: {labeled_preview_path_value}")
                else:
                    labeled_preview = Image.open(labeled_preview_path).convert("RGBA")

        size = atlas_info.get("size") if isinstance(atlas_info, dict) else None
        transparent_nonzero_rgb_pixels = 0
        if atlas is not None:
            if not isinstance(size, list) or len(size) != 2 or [int(size[0]), int(size[1])] != [atlas.width, atlas.height]:
                atlas_problems.append(f"atlas size metadata must match image {atlas.width}x{atlas.height}")
            transparent_nonzero_rgb_pixels = transparent_nonzero_rgb_count(atlas)
            if transparent_nonzero_rgb_pixels:
                atlas_problems.append(
                    f"clean atlas transparent pixels must have zero RGB; found {transparent_nonzero_rgb_pixels}"
                )
        if atlas is not None and labeled_preview is not None and labeled_preview.size != atlas.size:
            atlas_problems.append("labeled preview size must match atlas image")

        entries = atlas_info.get("entries") if isinstance(atlas_info, dict) else None
        if not isinstance(entries, list) or not entries:
            atlas_problems.append("atlas needs non-empty entries")
            entries = []

        entries_by_id = {str(entry.get("id") or ""): entry for entry in entries if isinstance(entry, dict)}
        padded_rects: list[tuple[str, tuple[int, int, int, int]]] = []
        review_label_rects: list[tuple[str, tuple[int, int, int, int]]] = []
        for entry in entries:
            entry_id = str(entry.get("id") or "")
            if not entry_id:
                atlas_problems.append("atlas entry needs id")
                continue
            if entry_id in reported_ids:
                atlas_problems.append(f"duplicate atlas entry id {entry_id}")
            reported_ids.add(entry_id)
            if entry_id in expected_assets:
                expected_kind = expected_assets[entry_id].get("kind")
                if entry.get("kind") != expected_kind:
                    atlas_problems.append(f"{entry_id} kind must match asset manifest")
                expected_alias = expected_assets[entry_id].get("alias_of")
                if expected_alias and entry.get("alias_of") != expected_alias:
                    atlas_problems.append(f"{entry_id} alias_of must match asset manifest")

            for rect_name in ("atlas_rect", "padded_rect"):
                if not rect_valid(entry.get(rect_name)):
                    atlas_problems.append(f"{entry_id} needs valid {rect_name}")
                    continue
                x, y, w, h = rect_tuple(entry[rect_name])
                if atlas is not None and (x + w > atlas.width or y + h > atlas.height):
                    atlas_problems.append(f"{entry_id} {rect_name} exceeds atlas bounds")

            alias_of = entry.get("alias_of")
            if alias_of:
                target = entries_by_id.get(str(alias_of))
                if target is None:
                    atlas_problems.append(f"{entry_id} aliases missing atlas entry {alias_of}")
                elif rect_valid(entry.get("atlas_rect")) and rect_valid(entry.get("padded_rect")):
                    if entry["atlas_rect"] != target.get("atlas_rect"):
                        atlas_problems.append(f"{entry_id} atlas_rect must reuse alias target {alias_of}")
                    if entry["padded_rect"] != target.get("padded_rect"):
                        atlas_problems.append(f"{entry_id} padded_rect must reuse alias target {alias_of}")
                    if entry.get("extrude") != target.get("extrude"):
                        atlas_problems.append(f"{entry_id} extrude must reuse alias target {alias_of}")
                continue

            if rect_valid(entry.get("padded_rect")):
                padded = rect_tuple(entry["padded_rect"])
                for other_id, other in padded_rects:
                    if intersects(padded, other):
                        atlas_problems.append(f"{entry_id} padded_rect overlaps {other_id}")
                padded_rects.append((entry_id, padded))
                if atlas_info.get("label_overlay"):
                    review_label = entry.get("review_label")
                    if not isinstance(review_label, dict) or not isinstance(review_label.get("text"), str):
                        atlas_problems.append(f"{entry_id} needs review_label text for labeled review atlas")
                    elif not rect_valid(review_label.get("rect")):
                        atlas_problems.append(f"{entry_id} needs valid review_label rect")
                    else:
                        expected_label = expected_review_label_text(entry_id, entries_by_id, expected_assets)
                        if review_label["text"] != expected_label:
                            atlas_problems.append(f"{entry_id} review_label text must be `{expected_label}`")
                        placement = review_label.get("placement")
                        if placement not in {"bottom", "right"}:
                            atlas_problems.append(f"{entry_id} review_label placement must be bottom or right")
                        label_rect = rect_tuple(review_label["rect"])
                        atlas_problems.extend(check_review_label_lines(entry_id, review_label, label_rect))
                        if intersects(padded, label_rect):
                            atlas_problems.append(f"{entry_id} review_label rect overlaps padded_rect")
                        if atlas is not None:
                            lx, ly, lw, lh = label_rect
                            if lx + lw > atlas.width or ly + lh > atlas.height:
                                atlas_problems.append(f"{entry_id} review_label rect exceeds atlas bounds")
                            elif (
                                lx < LABEL_OUTER_MARGIN
                                or ly < LABEL_OUTER_MARGIN
                                or atlas.width - (lx + lw) < LABEL_OUTER_MARGIN
                                or atlas.height - (ly + lh) < LABEL_OUTER_MARGIN
                            ):
                                atlas_problems.append(f"{entry_id} review_label rect must keep {LABEL_OUTER_MARGIN}px atlas edge margin")
                            elif rect_has_visible_pixel(atlas, label_rect):
                                atlas_problems.append(f"{entry_id} review_label rect must be empty in clean atlas")
                        if labeled_preview is not None:
                            lx, ly, lw, lh = label_rect
                            if lx + lw <= labeled_preview.width and ly + lh <= labeled_preview.height and not rect_has_visible_pixel(labeled_preview, label_rect):
                                atlas_problems.append(f"{entry_id} review_label rect has no visible pixels in labeled preview")
                        review_label_rects.append((entry_id, label_rect))

            if atlas is not None and rect_valid(entry.get("atlas_rect")) and rect_valid(entry.get("padded_rect")):
                atlas_problems.extend(check_extrusion(atlas, entry))

        for label_id, label_rect in review_label_rects:
            for padded_id, padded_rect in padded_rects:
                if intersects(label_rect, padded_rect):
                    atlas_problems.append(f"{label_id} review_label rect overlaps padded_rect for {padded_id}")
        for index, (label_id, label_rect) in enumerate(review_label_rects):
            for other_id, other_label_rect in review_label_rects[index + 1 :]:
                if intersects(label_rect, other_label_rect):
                    atlas_problems.append(f"{label_id} review_label rect overlaps review_label rect for {other_id}")

        outside_padded_visible_pixels = 0
        if atlas is not None and padded_rects:
            outside_padded_visible_pixels = visible_pixels_outside_rects_count(atlas, [rect for _, rect in padded_rects])
            if outside_padded_visible_pixels:
                atlas_problems.append(
                    f"clean atlas visible pixels must be inside packed padded_rects; found {outside_padded_visible_pixels}"
                )
        labeled_preview_delta_outside_label_pixels = 0
        if atlas is not None and labeled_preview is not None and review_label_rects:
            labeled_preview_delta_outside_label_pixels = changed_pixels_outside_rects_count(
                atlas, labeled_preview, [rect for _, rect in review_label_rects]
            )
            if labeled_preview_delta_outside_label_pixels:
                atlas_problems.append(
                    "labeled preview pixels may differ from clean atlas only inside review_label rects; "
                    f"found {labeled_preview_delta_outside_label_pixels}"
                )

        atlas_report = {
            "pack_group": atlas_info.get("pack_group") if isinstance(atlas_info, dict) else None,
            "path": atlas_path_value,
            "labeled_preview_path": labeled_preview_path_value,
            "labeled_preview_policy": atlas_info.get("labeled_preview_policy") if isinstance(atlas_info, dict) and atlas_info.get("label_overlay") else None,
            "status": "pass" if not atlas_problems else "fail",
            "problems": atlas_problems,
            "entry_count": len(entries),
            "physical_entry_count": atlas_info.get("physical_entry_count") if isinstance(atlas_info, dict) else None,
            "alias_count": atlas_info.get("alias_count") if isinstance(atlas_info, dict) else None,
            "transparent_nonzero_rgb_pixels": transparent_nonzero_rgb_pixels,
            "outside_padded_visible_pixels": outside_padded_visible_pixels,
            "labeled_preview_delta_outside_label_pixels": labeled_preview_delta_outside_label_pixels,
            "analysis_engine": analysis_engine(),
        }
        if profile:
            atlas_report["timing_ms"] = {"total": round((perf_counter() - atlas_started) * 1000, 3)}
        atlas_reports.append(atlas_report)
        problems.extend(atlas_problems)

    for asset_id in expected_assets:
        if asset_id not in reported_ids:
            problems.append(f"missing packed asset id {asset_id}")

    result = {
        "schema": "game.ui_atlas_pack_audit",
        "version": 1,
        "atlas_pack": norm_path(pack_path),
        "asset_manifest": norm_path(manifest_path) if manifest_path else pack.get("asset_manifest"),
        "verdict": "pass" if not problems else "fail",
        "problems": problems,
        "atlases": atlas_reports,
    }
    if labeled_preview_policy is not None:
        result["labeled_preview_policy"] = labeled_preview_policy
    if profile:
        timings["total"] = round((perf_counter() - started) * 1000, 3)
        result["timing_ms"] = timings
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit UI atlas pack geometry and extrusion.")
    parser.add_argument("--atlas-pack", required=True)
    parser.add_argument("--asset-manifest")
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    parser.add_argument("--profile", action="store_true", help="Record atlas audit timing in JSON/Markdown and print the slowest atlas group.")
    args = parser.parse_args()

    pack_path = project_path(args.atlas_pack)
    if not pack_path.exists():
        fail(f"atlas pack not found: {args.atlas_pack}")
    manifest_path = project_path(args.asset_manifest) if args.asset_manifest else None
    audit = audit_pack(pack_path, manifest_path, args.profile)
    if args.json_output:
        write_json(project_path(args.json_output), audit)
    lines = [
        "# UI Atlas Pack Audit",
        "",
        f"atlas_pack: `{audit['atlas_pack']}`",
        f"asset_manifest: `{audit.get('asset_manifest')}`",
        f"verdict: **{audit['verdict']}**",
        "",
    ]
    if audit.get("timing_ms"):
        lines.extend(["## Timing", ""])
        for name, elapsed in audit["timing_ms"].items():
            lines.append(f"- {name}: {elapsed} ms")
        lines.append("")
    if audit.get("labeled_preview_policy"):
        policy = audit["labeled_preview_policy"]
        lines.extend(
            [
                "## Labeled Preview Policy",
                "",
                "Audit requires labeled preview pixels to differ from the clean atlas only inside declared review_label rects.",
                f"- mode: `{policy.get('mode', '-')}`",
                f"- allowed_delta: `{policy.get('allowed_delta', '-')}`",
                f"- debug_outlines: `{str(policy.get('debug_outlines', '-')).lower()}`",
                "",
            ]
        )
    lines.extend(["## Atlases", ""])
    for atlas in audit["atlases"]:
        suffix = ""
        if atlas["problems"]:
            suffix = ": " + "; ".join(atlas["problems"])
        physical = atlas.get("physical_entry_count")
        aliases = atlas.get("alias_count")
        detail = f"entries={atlas['entry_count']}"
        if physical is not None and aliases is not None:
            detail += f", physical={physical}, aliases={aliases}"
        detail += f", transparent_nonzero_rgb_pixels={atlas.get('transparent_nonzero_rgb_pixels', '-')}"
        detail += f", outside_padded_visible_pixels={atlas.get('outside_padded_visible_pixels', '-')}"
        detail += f", labeled_preview_delta_outside_label_pixels={atlas.get('labeled_preview_delta_outside_label_pixels', '-')}"
        detail += f", analysis_engine={atlas.get('analysis_engine', '-')}"
        if atlas.get("labeled_preview_path"):
            detail += f", labeled_preview=`{atlas.get('labeled_preview_path')}`"
        if atlas.get("labeled_preview_policy"):
            policy = atlas["labeled_preview_policy"]
            detail += (
                ", labels="
                f"{policy.get('mode', '-')}/"
                f"{policy.get('allowed_delta', '-')}/"
                f"debug_outlines={str(policy.get('debug_outlines', '-')).lower()}"
            )
        lines.append(f"- {atlas['status'].upper()} `{atlas.get('pack_group')}` {detail}{suffix}")
    lines.append("")
    if args.report:
        write_text(project_path(args.report), "\n".join(lines))
    else:
        print(json.dumps(audit, indent=2))
    if audit["problems"]:
        raise SystemExit(1)
    print(f"pass: audited {sum(atlas['entry_count'] for atlas in audit['atlases'])} packed UI asset(s)")
    if args.profile and audit["atlases"]:
        slowest = max(audit["atlases"], key=lambda atlas: atlas.get("timing_ms", {}).get("total", 0))
        print(f"profile: slowest atlas audit `{slowest.get('pack_group')}` {slowest.get('timing_ms', {}).get('total', 0)} ms")


if __name__ == "__main__":
    main()
