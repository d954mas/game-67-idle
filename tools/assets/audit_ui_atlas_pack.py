#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from time import perf_counter
from typing import Any

from PIL import Image
from PIL import ImageDraw


ROOT = Path.cwd()
LABEL_PAD_X = 2
LABEL_PAD_Y = 1
LABEL_LINE_GAP_Y = 1


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
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


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


def measure_label(label: str) -> tuple[int, int]:
    probe = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    draw = ImageDraw.Draw(probe)
    bbox = draw.textbbox((0, 0), label)
    return int(bbox[2] - bbox[0]), int(bbox[3] - bbox[1])


def check_review_label_lines(entry_id: str, review_label: dict[str, Any], label_rect: tuple[int, int, int, int]) -> list[str]:
    problems: list[str] = []
    _, _, label_width, label_height = label_rect
    raw_lines = review_label.get("lines")
    if raw_lines is None:
        raw_lines = [review_label.get("text") or entry_id]
    if not isinstance(raw_lines, list) or not raw_lines or not all(isinstance(line, str) and line for line in raw_lines):
        return [f"{entry_id} review_label lines must be non-empty strings"]
    line_sizes = [measure_label(line) for line in raw_lines]
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
            if not isinstance(labeled_preview_path_value, str) or not labeled_preview_path_value:
                atlas_problems.append("labeled review atlas needs labeled_preview_path")
            else:
                labeled_preview_path = project_path(labeled_preview_path_value)
                if not labeled_preview_path.exists():
                    atlas_problems.append(f"labeled preview image missing: {labeled_preview_path_value}")
                else:
                    labeled_preview = Image.open(labeled_preview_path).convert("RGBA")

        size = atlas_info.get("size") if isinstance(atlas_info, dict) else None
        if atlas is not None:
            if not isinstance(size, list) or len(size) != 2 or [int(size[0]), int(size[1])] != [atlas.width, atlas.height]:
                atlas_problems.append(f"atlas size metadata must match image {atlas.width}x{atlas.height}")
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
                        label_rect = rect_tuple(review_label["rect"])
                        atlas_problems.extend(check_review_label_lines(entry_id, review_label, label_rect))
                        if intersects(padded, label_rect):
                            atlas_problems.append(f"{entry_id} review_label rect overlaps padded_rect")
                        if atlas is not None:
                            lx, ly, lw, lh = label_rect
                            if lx + lw > atlas.width or ly + lh > atlas.height:
                                atlas_problems.append(f"{entry_id} review_label rect exceeds atlas bounds")
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

        atlas_report = {
            "pack_group": atlas_info.get("pack_group") if isinstance(atlas_info, dict) else None,
            "path": atlas_path_value,
            "labeled_preview_path": labeled_preview_path_value,
            "status": "pass" if not atlas_problems else "fail",
            "problems": atlas_problems,
            "entry_count": len(entries),
            "physical_entry_count": atlas_info.get("physical_entry_count") if isinstance(atlas_info, dict) else None,
            "alias_count": atlas_info.get("alias_count") if isinstance(atlas_info, dict) else None,
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
