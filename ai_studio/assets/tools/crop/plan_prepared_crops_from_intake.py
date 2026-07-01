#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import write_json_atomic, write_text_atomic


def project_path(value: str | Path) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return ROOT / path


def rel(path: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise SystemExit(f"{rel(path)} must contain a JSON object")
    return data


def rect_tuple(value: Any, *, label: str) -> tuple[int, int, int, int]:
    if not isinstance(value, list) or len(value) != 4:
        raise SystemExit(f"{label} must be a four-number rect")
    rect = tuple(int(item) for item in value)
    if rect[2] <= 0 or rect[3] <= 0:
        raise SystemExit(f"{label} must have positive width and height")
    return rect


def sort_components_row_major(components: list[dict[str, Any]], row_tolerance: int) -> list[dict[str, Any]]:
    rows: list[list[dict[str, Any]]] = []
    row_centers: list[float] = []
    for component in sorted(components, key=lambda item: rect_tuple(item["bbox"], label=f"{item.get('id', 'component')} bbox")[1]):
        x, y, width, height = rect_tuple(component["bbox"], label=f"{component.get('id', 'component')} bbox")
        center_y = y + height / 2
        placed = False
        for index, row_center in enumerate(row_centers):
            if abs(center_y - row_center) <= row_tolerance:
                rows[index].append(component)
                count = len(rows[index])
                row_centers[index] = ((row_center * (count - 1)) + center_y) / count
                placed = True
                break
        if not placed:
            rows.append([component])
            row_centers.append(center_y)
    ordered_rows = sorted(zip(row_centers, rows), key=lambda item: item[0])
    ordered: list[dict[str, Any]] = []
    for _center, row in ordered_rows:
        ordered.extend(sorted(row, key=lambda item: rect_tuple(item["bbox"], label=f"{item.get('id', 'component')} bbox")[0]))
    return ordered


def expanded_rect(bbox: tuple[int, int, int, int], padding: int, size: tuple[int, int]) -> list[int]:
    x, y, width, height = bbox
    sheet_width, sheet_height = size
    left = max(0, x - padding)
    top = max(0, y - padding)
    right = min(sheet_width, x + width + padding)
    bottom = min(sheet_height, y + height + padding)
    return [left, top, right - left, bottom - top]


def parse_ids(raw_ids: str) -> list[str]:
    ids = [item.strip() for item in raw_ids.split(",") if item.strip()]
    if not ids:
        raise SystemExit("--ids must provide at least one id")
    if len(ids) != len(set(ids)):
        raise SystemExit("--ids must not contain duplicate ids")
    return ids


def parse_ids_file(path: Path) -> list[str]:
    if path.suffix.lower() == ".json":
        data = load_json(path)
        raw_ids = data.get("ids")
        if not isinstance(raw_ids, list):
            raise SystemExit("--ids-file JSON must contain an ids array")
        ids = [str(item).strip() for item in raw_ids if str(item).strip()]
    else:
        ids = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip() and not line.strip().startswith("#")]
    if not ids:
        raise SystemExit("--ids-file must provide at least one id")
    if len(ids) != len(set(ids)):
        raise SystemExit("--ids-file must not contain duplicate ids")
    return ids


def load_ids(args: argparse.Namespace) -> list[str]:
    if bool(args.ids) == bool(args.ids_file):
        raise SystemExit("provide exactly one of --ids or --ids-file")
    if args.ids:
        return parse_ids(args.ids)
    return parse_ids_file(project_path(args.ids_file))


def build_plan(args: argparse.Namespace) -> dict[str, Any]:
    intake_path = project_path(args.intake_audit)
    intake = load_json(intake_path)
    if intake.get("schema") != "game.source_sheet_intake_audit":
        raise SystemExit("--intake-audit must point to a game.source_sheet_intake_audit JSON")
    if intake.get("status") not in {"pass", "warn"}:
        raise SystemExit(f"intake audit status must be pass or warn, got {intake.get('status')!r}")
    components = intake.get("components")
    if not isinstance(components, list) or not components:
        raise SystemExit("intake audit must contain non-empty components")
    ids = load_ids(args)
    if len(ids) != len(components):
        raise SystemExit(f"--ids count ({len(ids)}) must match intake component count ({len(components)})")
    sheet_size = rect_tuple([0, 0, *(int(item) for item in intake.get("size", []))], label="intake size")[2:]
    ordered = sort_components_row_major(components, args.row_tolerance)
    crops: list[dict[str, Any]] = []
    for asset_id, component in zip(ids, ordered):
        component_id = str(component.get("id") or "")
        bbox = rect_tuple(component.get("bbox"), label=f"{component_id} bbox")
        crop_rect = expanded_rect(bbox, args.padding, sheet_size)
        output_name = f"{asset_id}{args.output_suffix}"
        crop: dict[str, Any] = {
            "id": asset_id,
            "kind": args.kind,
            "source_component_id": component_id,
            "component_bbox": list(bbox),
            "rect": crop_rect,
            "output": f"{args.output_dir.rstrip('/')}/{output_name}",
            "trim": {
                "padding": args.trim_padding,
            },
            "chroma_key": {
                "key": intake.get("key_color", args.key_color),
            },
            "atlas": {
                "pack_group": args.pack_group,
                "allow_rotation": False,
                "extrude": args.extrude,
                "shape_padding": args.shape_padding,
            },
        }
        if args.kind == "icon":
            crop["semantic_role"] = args.semantic_role_prefix + asset_id
            crop["size_class"] = args.size_class
            crop["preview_sizes"] = [[32, 32], [48, 48]]
        else:
            crop["semantic_role"] = args.semantic_role_prefix + asset_id
        crops.append(crop)
    return {
        "schema": "game.prepared_crop_plan",
        "version": 1,
        "source": intake.get("source"),
        "source_id": args.source_id,
        "source_role": args.source_role,
        "intake_audit": rel(intake_path),
        "component_sort": {
            "mode": "row_major",
            "row_tolerance": args.row_tolerance,
        },
        "padding": args.padding,
        "output_dir": args.output_dir.rstrip("/"),
        "crop_count": len(crops),
        "crops": crops,
    }


def write_report(path: Path, plan: dict[str, Any]) -> None:
    lines = [
        "# Prepared Crop Plan From Intake",
        "",
        f"source: `{plan['source']}`",
        f"intake_audit: `{plan['intake_audit']}`",
        f"source_role: {plan['source_role']}",
        f"component_sort: `{plan['component_sort']['mode']}` row_tolerance={plan['component_sort']['row_tolerance']}",
        f"padding: {plan['padding']}",
        f"output_dir: `{plan['output_dir']}`",
        f"crop_count: **{plan['crop_count']}**",
        "",
        "## Crops",
        "",
    ]
    for crop in plan["crops"]:
        lines.append(
            f"- `{crop['id']}`: kind={crop['kind']}, component={crop['source_component_id']}, "
            f"component_bbox={crop['component_bbox']}, rect={crop['rect']}, output=`{crop['output']}`"
        )
    write_text_atomic(path, "\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create a named prepared crop plan from a passing source-sheet intake audit.")
    parser.add_argument("--intake-audit", required=True)
    parser.add_argument("--ids", help="Comma-separated asset ids in visual row-major order.")
    parser.add_argument("--ids-file", help="Text file with one id per line, or JSON object with an ids array.")
    parser.add_argument("--kind", required=True, choices=["icon", "decor", "sprite"])
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--source-role", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--json-output", required=True)
    parser.add_argument("--report")
    parser.add_argument("--padding", type=int, default=12)
    parser.add_argument("--trim-padding", type=int, default=8)
    parser.add_argument("--row-tolerance", type=int, default=96)
    parser.add_argument("--output-suffix", default=".png")
    parser.add_argument("--pack-group", default="ui_common")
    parser.add_argument("--semantic-role-prefix", default="")
    parser.add_argument("--size-class", default="96px_source")
    parser.add_argument("--key-color", default="#00ff00")
    parser.add_argument("--extrude", type=int, default=2)
    parser.add_argument("--shape-padding", type=int, default=2)
    args = parser.parse_args()
    if args.padding < 0 or args.trim_padding < 0:
        raise SystemExit("--padding and --trim-padding must be >= 0")
    plan = build_plan(args)
    json_output = project_path(args.json_output)
    write_json_atomic(json_output, plan)
    if args.report:
        write_report(project_path(args.report), plan)
    print(f"wrote {plan['crop_count']} named crop plan entries to {rel(json_output)}")


if __name__ == "__main__":
    main()
