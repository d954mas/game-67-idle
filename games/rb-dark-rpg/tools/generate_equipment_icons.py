#!/usr/bin/env python3
"""Slice AI-generated equipment icon source sheets into runtime icons."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


GAME_DIR = Path(__file__).resolve().parents[1]
DEFAULT_ITEMS_PATH = GAME_DIR / "design" / "data" / "items.json"
DEFAULT_OUT_DIR = GAME_DIR / "assets" / "ui" / "generated" / "equipment_icons_01"
DEFAULT_CELL_SOURCE = DEFAULT_OUT_DIR / "cell_source.png"
DEFAULT_SLOTS_SOURCE = DEFAULT_OUT_DIR / "slots_source_sheet.png"
DEFAULT_GEAR_SOURCE = DEFAULT_OUT_DIR / "gear_source_sheet.png"
ICON_SIZE = 64


@dataclass(frozen=True)
class IconSpec:
    asset_id: str
    file: str
    role: str
    slot: str
    sheet_index: int
    item_id: str = ""


@dataclass(frozen=True)
class IconSpecs:
    cell: IconSpec
    slots: tuple[IconSpec, ...]
    gear: tuple[IconSpec, ...]
    reward_tokens: tuple[IconSpec, ...]
    reward_items: tuple[IconSpec, ...]


def load_items_doc(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def collect_icon_specs(items_doc: dict) -> IconSpecs:
    cell = IconSpec(
        asset_id="asset_equipment_slot_cell",
        file="cell/asset_equipment_slot_cell.png",
        role="slot_cell",
        slot="",
        sheet_index=0,
    )
    slots = []
    for index, slot in enumerate(sorted(items_doc.get("equipment_slots", []), key=lambda value: int(value.get("ui_order", 9999)))):
        slot_id = str(slot["id"])
        asset_id = f"asset_slot_icon_{slot_id}_empty"
        slots.append(IconSpec(asset_id=asset_id, file=f"slots/{asset_id}.png", role="slot_empty", slot=slot_id, sheet_index=index))

    gear = []
    for index, item in enumerate(item for item in items_doc.get("items", []) if item.get("kind") == "gear"):
        asset_id = str(item["icon_asset_id"])
        gear.append(
            IconSpec(
                asset_id=asset_id,
                file=f"gear/{asset_id}.png",
                role="gear_item",
                slot=str(item["slot"]),
                item_id=str(item["id"]),
                sheet_index=index,
            )
        )

    reward_items = []
    seen_reward_assets: set[str] = set()
    for item in items_doc.get("items", []):
        if item.get("kind") == "gear" or not item.get("icon_asset_id"):
            continue
        asset_id = str(item["icon_asset_id"])
        if asset_id in seen_reward_assets:
            continue
        seen_reward_assets.add(asset_id)
        reward_items.append(
            IconSpec(
                asset_id=asset_id,
                file=f"items/{asset_id}.png",
                role="reward_item",
                slot="",
                sheet_index=len(reward_items),
                item_id=str(item["id"]),
            )
        )

    reward_tokens = (
        IconSpec(
            asset_id="asset_reward_xp",
            file="rewards/asset_reward_xp.png",
            role="reward_token",
            slot="",
            sheet_index=0,
        ),
    )

    return IconSpecs(
        cell=cell,
        slots=tuple(slots),
        gear=tuple(gear),
        reward_tokens=reward_tokens,
        reward_items=tuple(reward_items),
    )


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def key_to_alpha(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    data = np.array(rgba)
    r = data[:, :, 0].astype(np.int16)
    g = data[:, :, 1].astype(np.int16)
    b = data[:, :, 2].astype(np.int16)

    key = (g > 160) & (r < 120) & (b < 120) & (g > r + 55) & (g > b + 55)
    data[:, :, 3][key] = 0

    spill = (data[:, :, 3] > 0) & (g > r + 30) & (g > b + 30)
    max_rb = np.maximum(data[:, :, 0], data[:, :, 2])
    data[:, :, 1][spill] = np.minimum(data[:, :, 1][spill], max_rb[spill] + 24)
    return Image.fromarray(data, mode="RGBA")


def non_key_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    data = np.array(image.convert("RGB"))
    r = data[:, :, 0].astype(np.int16)
    g = data[:, :, 1].astype(np.int16)
    b = data[:, :, 2].astype(np.int16)
    mask = ~((g > 160) & (r < 120) & (b < 120) & (g > r + 55) & (g > b + 55))
    ys, xs = np.where(mask)
    if len(xs) == 0 or len(ys) == 0:
        return (0, 0, image.width, image.height)
    return (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)


def slice_icon(source: Image.Image, columns: int, rows: int, index: int) -> Image.Image:
    col = index % columns
    row = index // columns
    if row >= rows:
        raise ValueError(f"sheet index {index} exceeds {columns}x{rows} sheet")

    cell_w = source.width / columns
    cell_h = source.height / rows
    x0 = int(round(col * cell_w))
    y0 = int(round(row * cell_h))
    x1 = int(round((col + 1) * cell_w))
    y1 = int(round((row + 1) * cell_h))
    cell = source.crop((x0, y0, x1, y1))
    bx0, by0, bx1, by1 = non_key_bbox(cell)

    pad = int(round(max(bx1 - bx0, by1 - by0) * 0.08))
    cx = (bx0 + bx1) / 2
    cy = (by0 + by1) / 2
    side = int(round(max(bx1 - bx0, by1 - by0) + pad * 2))
    side = min(side, min(cell.width, cell.height))
    sx0 = max(0, int(round(cx - side / 2)))
    sy0 = max(0, int(round(cy - side / 2)))
    sx1 = min(cell.width, sx0 + side)
    sy1 = min(cell.height, sy0 + side)
    sx0 = max(0, sx1 - side)
    sy0 = max(0, sy1 - side)

    icon = key_to_alpha(cell.crop((sx0, sy0, sx1, sy1)))
    return icon.resize((ICON_SIZE, ICON_SIZE), Image.Resampling.LANCZOS)


def slice_single_art(source: Image.Image) -> Image.Image:
    bx0, by0, bx1, by1 = non_key_bbox(source)
    pad = int(round(max(bx1 - bx0, by1 - by0) * 0.05))
    crop = source.crop((max(0, bx0 - pad), max(0, by0 - pad), min(source.width, bx1 + pad), min(source.height, by1 + pad)))
    side = max(crop.width, crop.height)
    square = Image.new("RGB", (side, side), (0, 255, 0))
    square.paste(crop, ((side - crop.width) // 2, (side - crop.height) // 2))
    return key_to_alpha(square).resize((ICON_SIZE, ICON_SIZE), Image.Resampling.LANCZOS)


def scaled_box(values: tuple[int, int, int, int], scale: int) -> tuple[int, int, int, int]:
    return tuple(v * scale for v in values)


def scaled_points(values: tuple[tuple[int, int], ...], scale: int) -> list[tuple[int, int]]:
    return [(x * scale, y * scale) for x, y in values]


def draw_reward_item_icon(asset_id: str) -> Image.Image:
    scale = 4
    image = Image.new("RGBA", (ICON_SIZE * scale, ICON_SIZE * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image, "RGBA")

    def box(values: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
        return scaled_box(values, scale)

    def points(values: tuple[tuple[int, int], ...]) -> list[tuple[int, int]]:
        return scaled_points(values, scale)

    def line(values: tuple[tuple[int, int], ...], fill: tuple[int, int, int, int], width: int) -> None:
        draw.line(points(values), fill=fill, width=width * scale, joint="curve")

    draw.ellipse(box((13, 48, 51, 58)), fill=(0, 0, 0, 70))

    if asset_id == "asset_icon_seeker_token":
        draw.ellipse(box((13, 8, 51, 52)), fill=(92, 56, 28, 255), outline=(226, 172, 88, 255), width=3 * scale)
        draw.ellipse(box((19, 14, 45, 45)), fill=(158, 97, 42, 255), outline=(246, 204, 124, 255), width=2 * scale)
        draw.polygon(points(((32, 19), (39, 31), (32, 43), (25, 31))), fill=(51, 83, 99, 255), outline=(235, 211, 152, 255))
        draw.line(points(((32, 22), (32, 40))), fill=(247, 220, 148, 210), width=2)
    elif asset_id == "asset_reward_xp":
        draw.polygon(
            points(((32, 6), (40, 22), (57, 25), (45, 37), (48, 55), (32, 46), (16, 55), (19, 37), (7, 25), (24, 22))),
            fill=(49, 93, 122, 255),
            outline=(211, 182, 105, 255),
        )
        draw.polygon(points(((32, 13), (39, 28), (32, 43), (25, 28))), fill=(99, 167, 196, 255), outline=(232, 216, 154, 235))
        line(((18, 25), (46, 25)), (232, 216, 154, 180), 2)
        line(((23, 38), (41, 38)), (232, 216, 154, 150), 2)
    elif asset_id == "asset_icon_grain_sacks":
        draw.ellipse(box((10, 47, 54, 58)), fill=(0, 0, 0, 75))
        draw.rounded_rectangle(box((13, 20, 35, 52)), radius=7 * scale, fill=(142, 98, 54, 255), outline=(226, 177, 103, 255), width=2 * scale)
        draw.rounded_rectangle(box((29, 17, 51, 53)), radius=7 * scale, fill=(117, 80, 49, 255), outline=(209, 158, 86, 255), width=2 * scale)
        draw.arc(box((15, 15, 36, 30)), 180, 350, fill=(245, 204, 118, 220), width=2 * scale)
        draw.arc(box((31, 12, 52, 28)), 180, 350, fill=(229, 185, 101, 220), width=2 * scale)
        for x in (22, 28, 39, 45):
            line(((x, 27), (x - 2, 45)), (80, 53, 34, 115), 1)
    elif asset_id == "asset_icon_contract_progress":
        draw.rounded_rectangle(box((13, 9, 50, 54)), radius=4 * scale, fill=(191, 157, 104, 255), outline=(98, 62, 37, 255), width=2 * scale)
        draw.rectangle(box((18, 15, 45, 49)), fill=(226, 194, 135, 255))
        for y in (22, 29, 36):
            line(((22, y), (41, y)), (105, 72, 45, 180), 1)
        line(((22, 43), (28, 48), (42, 34)), (58, 117, 77, 255), 3)
    elif asset_id == "asset_icon_clue_fragment":
        draw.polygon(
            points(((18, 10), (47, 15), (43, 29), (50, 42), (33, 55), (14, 45), (20, 31), (12, 21))),
            fill=(213, 184, 132, 255),
            outline=(91, 60, 40, 255),
        )
        draw.polygon(points(((37, 15), (47, 15), (43, 27))), fill=(158, 118, 78, 255))
        line(((22, 24), (38, 27)), (85, 58, 43, 170), 1)
        line(((20, 34), (42, 38)), (85, 58, 43, 160), 1)
        draw.ellipse(box((29, 28, 37, 36)), outline=(43, 81, 91, 220), width=2 * scale)
    elif asset_id == "asset_icon_burned_chain_bracket":
        for bounds in ((13, 18, 35, 39), (29, 25, 51, 46)):
            draw.ellipse(box(bounds), outline=(48, 45, 43, 255), width=7 * scale)
            draw.ellipse(box((bounds[0] + 4, bounds[1] + 4, bounds[2] - 4, bounds[3] - 4)), outline=(137, 90, 53, 255), width=2 * scale)
        line(((16, 47), (48, 13)), (214, 88, 45, 210), 3)
        draw.polygon(points(((12, 48), (18, 43), (16, 54))), fill=(239, 133, 57, 210))
    elif asset_id == "asset_icon_order_scrap":
        draw.polygon(
            points(((16, 8), (47, 10), (51, 48), (38, 55), (30, 50), (16, 54), (11, 23))),
            fill=(219, 188, 130, 255),
            outline=(99, 63, 39, 255),
        )
        draw.rectangle(box((20, 16, 43, 20)), fill=(108, 70, 43, 170))
        for y in (27, 34):
            line(((20, y), (42, y)), (108, 70, 43, 150), 1)
        draw.ellipse(box((33, 39, 48, 53)), fill=(127, 31, 29, 235), outline=(237, 155, 90, 210), width=1 * scale)
        line(((37, 45), (44, 47)), (239, 183, 107, 180), 1)
    else:
        draw.rounded_rectangle(box((12, 12, 52, 52)), radius=6 * scale, fill=(84, 68, 52, 255), outline=(218, 170, 92, 255), width=2 * scale)
        draw.polygon(points(((32, 18), (44, 32), (32, 46), (20, 32))), fill=(44, 73, 86, 255), outline=(239, 206, 130, 255))

    return image.resize((ICON_SIZE, ICON_SIZE), Image.Resampling.LANCZOS)


def entry_for_spec(spec: IconSpec, out_dir: Path) -> dict:
    path = out_dir / spec.file
    entry = {
        "asset_id": spec.asset_id,
        "file": spec.file.replace("\\", "/"),
        "role": spec.role,
        "sheet_index": spec.sheet_index,
        "size": {"w": ICON_SIZE, "h": ICON_SIZE},
        "origin": "procedural" if spec.role in ("reward_item", "reward_token") else "ai",
        "license": "project-internal generated asset",
        "sha256": sha256_file(path),
    }
    if spec.slot:
        entry["slot"] = spec.slot
    if spec.item_id:
        entry["item_id"] = spec.item_id
    return entry


def write_contact_sheet(entries: list[dict], out_dir: Path, cell_file: str) -> dict:
    cols = 9
    cell = ICON_SIZE + 8
    rows = (len(entries) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cell + 8, rows * cell + 8), (13, 9, 7, 255))
    cell_image = Image.open(out_dir / cell_file).convert("RGBA")
    for index, entry in enumerate(entries):
        icon = Image.open(out_dir / entry["file"]).convert("RGBA")
        x = 8 + (index % cols) * cell
        y = 8 + (index // cols) * cell
        sheet.alpha_composite(cell_image, (x, y))
        sheet.alpha_composite(icon, (x, y))
    path = out_dir / "contact_sheet.png"
    sheet.save(path)
    return {
        "file": path.name,
        "shows": "cell_plus_overlay_composition",
        "origin": "derived",
        "license": "project-internal generated asset",
        "sha256": sha256_file(path),
        "size": {"w": sheet.width, "h": sheet.height},
    }


def write_icon_pack(specs: IconSpecs, out_dir: Path, cell_source_path: Path, slots_source_path: Path, gear_source_path: Path) -> dict:
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "cell").mkdir(exist_ok=True)
    (out_dir / "slots").mkdir(exist_ok=True)
    (out_dir / "gear").mkdir(exist_ok=True)
    (out_dir / "rewards").mkdir(exist_ok=True)
    (out_dir / "items").mkdir(exist_ok=True)

    cell_source = Image.open(cell_source_path).convert("RGB")
    slots_source = Image.open(slots_source_path).convert("RGB")
    gear_source = Image.open(gear_source_path).convert("RGB")
    slice_single_art(cell_source).save(out_dir / specs.cell.file)
    for spec in specs.slots:
        slice_icon(slots_source, 4, 3, spec.sheet_index).save(out_dir / spec.file)
    for spec in specs.gear:
        slice_icon(gear_source, 5, 3, spec.sheet_index).save(out_dir / spec.file)
    for spec in specs.reward_tokens:
        draw_reward_item_icon(spec.asset_id).save(out_dir / spec.file)
    for spec in specs.reward_items:
        draw_reward_item_icon(spec.asset_id).save(out_dir / spec.file)

    cell_entry = entry_for_spec(specs.cell, out_dir)
    slot_entries = [entry_for_spec(spec, out_dir) for spec in specs.slots]
    gear_entries = [entry_for_spec(spec, out_dir) for spec in specs.gear]
    reward_token_entries = [entry_for_spec(spec, out_dir) for spec in specs.reward_tokens]
    reward_item_entries = [entry_for_spec(spec, out_dir) for spec in specs.reward_items]
    contact_sheet = write_contact_sheet([*slot_entries, *gear_entries, *reward_token_entries, *reward_item_entries], out_dir, specs.cell.file)
    manifest = {
        "schema": "rb-dark-rpg.equipment_icons.v1",
        "game_id": "rb-dark-rpg",
        "status": "ai_generated_placeholder_ready_for_pack",
        "generated": "2026-07-05",
        "generator": "games/rb-dark-rpg/tools/generate_equipment_icons.py",
        "source_items": "games/rb-dark-rpg/design/data/items.json",
        "icon_size": {"w": ICON_SIZE, "h": ICON_SIZE},
        "source_sheets": [
            {
                "role": "cell",
                "file": cell_source_path.name,
                "layout": {"columns": 1, "rows": 1},
                "sha256": sha256_file(cell_source_path),
                "origin": "ai",
                "license": "project-internal generated asset",
                "prompt": "prompt_cell.txt",
            },
            {
                "role": "slots",
                "file": slots_source_path.name,
                "layout": {"columns": 4, "rows": 3},
                "sha256": sha256_file(slots_source_path),
                "origin": "ai",
                "license": "project-internal generated asset",
                "prompt": "prompt_slots.txt",
            },
            {
                "role": "gear",
                "file": gear_source_path.name,
                "layout": {"columns": 5, "rows": 3},
                "sha256": sha256_file(gear_source_path),
                "origin": "ai",
                "license": "project-internal generated asset",
                "prompt": "prompt_gear.txt",
            },
        ],
        "source_first": {
            "local_library": [
                'node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg equipment icons weapon armor boots ring relic" --kind item_icon,ui_icon --limit 12 --json',
                'node ai_studio/assets/backlog/storage/search.mjs --query "inventory slot icons rpg gear" --kind ui,item_icon,ui_icon --limit 12 --json',
                'node ai_studio/assets/backlog/storage/search.mjs --query "Kenney UI icons RPG equipment" --limit 12 --json',
            ],
            "local_library_result": "0 matches for each query",
            "free_source_result": "No exact CC0/OFL set imported; external candidates were mixed-license or did not cover the required slot/item set.",
        },
        "cell": cell_entry,
        "slots": slot_entries,
        "gear_items": gear_entries,
        "reward_tokens": reward_token_entries,
        "reward_items": reward_item_entries,
        "contact_sheet": contact_sheet,
    }
    manifest_path = out_dir / "manifest.generated.json"
    with manifest_path.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    return manifest


def write_provenance(out_dir: Path, manifest: dict) -> None:
    lines = [
        "# Equipment Icons 01",
        "",
        "- Asset pack: `games/rb-dark-rpg/assets/ui/generated/equipment_icons_01/`",
        "- Game: `rb-dark-rpg`",
        "- Role: AI-generated source sheets sliced into one reusable cell plus transparent slot, gear, and reward item overlay icons.",
        "- Status: placeholder-ready for runtime atlas packing.",
        "- Origin: AI-generated raster source sheets with deterministic crop/key cleanup; XP and quest/clue reward icons are deterministic procedural overlays from reward/item semantics.",
        "- Generator: `games/rb-dark-rpg/tools/generate_equipment_icons.py`.",
        "- Source data: `games/rb-dark-rpg/design/data/items.json`.",
        "- Date: 2026-07-05.",
        "- Final icon size: 64x64 RGBA PNG.",
        "- License: project-internal generated asset.",
        "",
        "## Source-First Check",
        "",
        "The local shared asset library was searched before generation:",
        "",
        "- `node ai_studio/assets/backlog/storage/search.mjs --query \"dark rpg equipment icons weapon armor boots ring relic\" --kind item_icon,ui_icon --limit 12 --json` -> 0 matches.",
        "- `node ai_studio/assets/backlog/storage/search.mjs --query \"inventory slot icons rpg gear\" --kind ui,item_icon,ui_icon --limit 12 --json` -> 0 matches.",
        "- `node ai_studio/assets/backlog/storage/search.mjs --query \"Kenney UI icons RPG equipment\" --limit 12 --json` -> 0 matches.",
        "",
        "A quick free-source check did not find one reliable CC0/OFL source set that covers all current slots and gear items without mixed-license or visual-style mismatch. The accepted output is therefore generated project art, not sourced third-party art.",
        "",
        "## AI Source Sheets",
        "",
    ]
    for sheet in manifest["source_sheets"]:
        lines.extend(
            [
                f"- `{sheet['file']}`: role `{sheet['role']}`, SHA-256 `{sheet['sha256']}`, prompt `{sheet['prompt']}`.",
            ]
        )
    lines.extend(
        [
            "",
            "Generation path: `.codex/skills/nt-asset-image-generation/scripts/generate_image.py` via Codex OAuth image generation. Path A wrapper was unavailable on this Windows host because WSL bash was missing and Git Bash could not execute the WindowsApps `codex.exe`; Path C was used after that failure.",
            "",
        "Layering contract: `asset_equipment_slot_cell` is the reusable cell/socket. Slot placeholder, gear item, XP token, and reward item icons are transparent overlays rendered above that cell; they are not baked into a rectangle.",
        "",
        "Cleanup: source sheets use flat green chroma-key background. The prep script crops each row-major cell, removes the green key to alpha, dampens green edge spill, and downsamples to 64x64.",
            "",
            "## Regeneration",
            "",
            "```powershell",
            "py -3.12 games/rb-dark-rpg/tools/generate_equipment_icons.py",
            "```",
            "",
            "## Integrity",
            "",
            "Per-icon SHA-256 values are recorded in `manifest.generated.json` and mirrored in `design/data/asset_manifest.json` for runtime asset ids.",
            "",
            f"- Contact sheet SHA-256: `{manifest['contact_sheet']['sha256']}`",
        ]
    )
    path = out_dir / "provenance.md"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--items", type=Path, default=DEFAULT_ITEMS_PATH)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--cell-source", type=Path, default=DEFAULT_CELL_SOURCE)
    parser.add_argument("--slots-source", type=Path, default=DEFAULT_SLOTS_SOURCE)
    parser.add_argument("--gear-source", type=Path, default=DEFAULT_GEAR_SOURCE)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    items_doc = load_items_doc(args.items)
    specs = collect_icon_specs(items_doc)
    manifest = write_icon_pack(specs, args.out, args.cell_source, args.slots_source, args.gear_source)
    write_provenance(args.out, manifest)
    print(
        f"wrote {len(manifest['slots'])} slot icons, {len(manifest['gear_items'])} gear icons, "
        f"{len(manifest['reward_tokens'])} reward token icons, and {len(manifest['reward_items'])} reward item icons -> {args.out}"
    )


if __name__ == "__main__":
    main()
