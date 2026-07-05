#!/usr/bin/env python3
"""Slice AI-generated equipment icon source sheets into runtime icons."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image


GAME_DIR = Path(__file__).resolve().parents[1]
DEFAULT_ITEMS_PATH = GAME_DIR / "design" / "data" / "items.json"
DEFAULT_OUT_DIR = GAME_DIR / "assets" / "ui" / "generated" / "equipment_icons_01"
DEFAULT_CELL_SOURCE = DEFAULT_OUT_DIR / "cell_source.png"
DEFAULT_SLOTS_SOURCE = DEFAULT_OUT_DIR / "slots_source_sheet.png"
DEFAULT_GEAR_SOURCE = DEFAULT_OUT_DIR / "gear_source_sheet.png"
DEFAULT_REWARD_SOURCE = DEFAULT_OUT_DIR / "reward_source_sheet.png"
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


def entry_for_spec(spec: IconSpec, out_dir: Path) -> dict:
    path = out_dir / spec.file
    entry = {
        "asset_id": spec.asset_id,
        "file": spec.file.replace("\\", "/"),
        "role": spec.role,
        "sheet_index": spec.sheet_index,
        "size": {"w": ICON_SIZE, "h": ICON_SIZE},
        "origin": "ai" if spec.role in ("gear_item", "reward_item", "reward_token") else "ai",
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


def write_icon_pack(
    specs: IconSpecs,
    out_dir: Path,
    cell_source_path: Path,
    slots_source_path: Path,
    gear_source_path: Path,
    reward_source_path: Path,
) -> dict:
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "cell").mkdir(exist_ok=True)
    (out_dir / "slots").mkdir(exist_ok=True)
    (out_dir / "gear").mkdir(exist_ok=True)
    (out_dir / "rewards").mkdir(exist_ok=True)
    (out_dir / "items").mkdir(exist_ok=True)

    cell_source = Image.open(cell_source_path).convert("RGB")
    slots_source = Image.open(slots_source_path).convert("RGB")
    gear_source = Image.open(gear_source_path).convert("RGB")
    reward_source = Image.open(reward_source_path).convert("RGB")
    slice_single_art(cell_source).save(out_dir / specs.cell.file)
    for spec in specs.slots:
        slice_icon(slots_source, 4, 3, spec.sheet_index).save(out_dir / spec.file)
    for spec in specs.gear:
        slice_icon(gear_source, 5, 3, spec.sheet_index).save(out_dir / spec.file)
    for spec in specs.reward_tokens:
        slice_icon(reward_source, 7, 1, spec.sheet_index).save(out_dir / spec.file)
    for spec in specs.reward_items:
        slice_icon(reward_source, 7, 1, spec.sheet_index + len(specs.reward_tokens)).save(out_dir / spec.file)

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
            {
                "role": "reward",
                "file": reward_source_path.name,
                "layout": {"columns": 7, "rows": 1},
                "sha256": sha256_file(reward_source_path),
                "origin": "ai",
                "license": "project-internal generated asset",
                "prompt": "prompt_reward_source_sheet.txt",
                "canvas_project_id": "rb-dark-rpg-reward-icon-source-sheet-6d8622",
                "canvas_element_id": "el_7a738c3b",
            },
        ],
        "source_first": {
            "local_library": [
                'node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg equipment icons weapon armor boots ring relic" --kind item_icon,ui_icon --limit 12 --json',
                'node ai_studio/assets/backlog/storage/search.mjs --query "inventory slot icons rpg gear" --kind ui,item_icon,ui_icon --limit 12 --json',
                'node ai_studio/assets/backlog/storage/search.mjs --query "Kenney UI icons RPG equipment" --limit 12 --json',
                'node ai_studio/assets/backlog/storage/search.mjs --query "dark rpg reward icons xp gold quest clue item tokens" --kind item_icon,ui_icon --limit 12 --json',
                'node ai_studio/assets/backlog/storage/search.mjs --query "xp reward token parchment clue sacks dark fantasy inventory icon" --kind item_icon,ui_icon --limit 12 --json',
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
        "- Origin: AI-generated raster source sheets with deterministic crop/key cleanup.",
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
        "- `node ai_studio/assets/backlog/storage/search.mjs --query \"dark rpg reward icons xp gold quest clue item tokens\" --kind item_icon,ui_icon --limit 12 --json` -> 0 matches.",
        "- `node ai_studio/assets/backlog/storage/search.mjs --query \"xp reward token parchment clue sacks dark fantasy inventory icon\" --kind item_icon,ui_icon --limit 12 --json` -> 0 matches.",
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
            "Generation path for `reward_source_sheet.png`: built-in `image_gen` tool after Path A was unavailable on this Windows host (`bash`/WSL missing; `codex.exe` access denied from PowerShell).",
            "",
            "Canvas handoff for `reward_source_sheet.png`: project `rb-dark-rpg-reward-icon-source-sheet-6d8622`, image element `el_7a738c3b`; prompt/provenance note `el_957e3538`.",
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
    parser.add_argument("--reward-source", type=Path, default=DEFAULT_REWARD_SOURCE)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    items_doc = load_items_doc(args.items)
    specs = collect_icon_specs(items_doc)
    manifest = write_icon_pack(specs, args.out, args.cell_source, args.slots_source, args.gear_source, args.reward_source)
    write_provenance(args.out, manifest)
    print(
        f"wrote {len(manifest['slots'])} slot icons, {len(manifest['gear_items'])} gear icons, "
        f"{len(manifest['reward_tokens'])} reward token icons, and {len(manifest['reward_items'])} reward item icons -> {args.out}"
    )


if __name__ == "__main__":
    main()
