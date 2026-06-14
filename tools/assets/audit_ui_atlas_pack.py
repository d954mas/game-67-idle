#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from PIL import Image


ROOT = Path.cwd()


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


def audit_pack(pack_path: Path, asset_manifest_path: Path | None = None) -> dict[str, Any]:
    pack = read_json(pack_path)
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

        size = atlas_info.get("size") if isinstance(atlas_info, dict) else None
        if atlas is not None:
            if not isinstance(size, list) or len(size) != 2 or [int(size[0]), int(size[1])] != [atlas.width, atlas.height]:
                atlas_problems.append(f"atlas size metadata must match image {atlas.width}x{atlas.height}")

        entries = atlas_info.get("entries") if isinstance(atlas_info, dict) else None
        if not isinstance(entries, list) or not entries:
            atlas_problems.append("atlas needs non-empty entries")
            entries = []

        padded_rects: list[tuple[str, tuple[int, int, int, int]]] = []
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

            for rect_name in ("atlas_rect", "padded_rect"):
                if not rect_valid(entry.get(rect_name)):
                    atlas_problems.append(f"{entry_id} needs valid {rect_name}")
                    continue
                x, y, w, h = rect_tuple(entry[rect_name])
                if atlas is not None and (x + w > atlas.width or y + h > atlas.height):
                    atlas_problems.append(f"{entry_id} {rect_name} exceeds atlas bounds")

            if rect_valid(entry.get("padded_rect")):
                padded = rect_tuple(entry["padded_rect"])
                for other_id, other in padded_rects:
                    if intersects(padded, other):
                        atlas_problems.append(f"{entry_id} padded_rect overlaps {other_id}")
                padded_rects.append((entry_id, padded))

            if atlas is not None and rect_valid(entry.get("atlas_rect")) and rect_valid(entry.get("padded_rect")):
                atlas_problems.extend(check_extrusion(atlas, entry))

        atlas_reports.append(
            {
                "pack_group": atlas_info.get("pack_group") if isinstance(atlas_info, dict) else None,
                "path": atlas_path_value,
                "status": "pass" if not atlas_problems else "fail",
                "problems": atlas_problems,
                "entry_count": len(entries),
            }
        )
        problems.extend(atlas_problems)

    for asset_id in expected_assets:
        if asset_id not in reported_ids:
            problems.append(f"missing packed asset id {asset_id}")

    return {
        "schema": "game.ui_atlas_pack_audit",
        "version": 1,
        "atlas_pack": norm_path(pack_path),
        "asset_manifest": norm_path(manifest_path) if manifest_path else pack.get("asset_manifest"),
        "verdict": "pass" if not problems else "fail",
        "problems": problems,
        "atlases": atlas_reports,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit UI atlas pack geometry and extrusion.")
    parser.add_argument("--atlas-pack", required=True)
    parser.add_argument("--asset-manifest")
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    args = parser.parse_args()

    pack_path = project_path(args.atlas_pack)
    if not pack_path.exists():
        fail(f"atlas pack not found: {args.atlas_pack}")
    manifest_path = project_path(args.asset_manifest) if args.asset_manifest else None
    audit = audit_pack(pack_path, manifest_path)
    if args.json_output:
        write_json(project_path(args.json_output), audit)
    lines = [
        "# UI Atlas Pack Audit",
        "",
        f"atlas_pack: `{audit['atlas_pack']}`",
        f"asset_manifest: `{audit.get('asset_manifest')}`",
        f"verdict: **{audit['verdict']}**",
        "",
        "## Atlases",
        "",
    ]
    for atlas in audit["atlases"]:
        suffix = ""
        if atlas["problems"]:
            suffix = ": " + "; ".join(atlas["problems"])
        lines.append(f"- {atlas['status'].upper()} `{atlas.get('pack_group')}` entries={atlas['entry_count']}{suffix}")
    lines.append("")
    if args.report:
        write_text(project_path(args.report), "\n".join(lines))
    else:
        print(json.dumps(audit, indent=2))
    if audit["problems"]:
        raise SystemExit(1)
    print(f"pass: audited {sum(atlas['entry_count'] for atlas in audit['atlases'])} packed UI asset(s)")


if __name__ == "__main__":
    main()
