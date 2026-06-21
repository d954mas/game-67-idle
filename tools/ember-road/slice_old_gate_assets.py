from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "assets"))
from atomic_io import save_image_atomic, write_json_atomic  # noqa: E402

JOB = "gamedesign/projects/ember-road/art_requests/ember-road-old-gate-fakeshot-v001.json"
CROP_MANIFEST = ROOT / "gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-crop_manifest.json"
ASSET_MANIFEST = ROOT / "gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-asset_manifest.json"
OUT_DIR = ROOT / "assets/runtime/ember-road-old-gate-fakeshot-v001"

SOURCE_DIR = ROOT / "gamedesign/projects/ember-road/art"


SOURCES = {
    "background": {
        "path": SOURCE_DIR / "ember-road-old-gate-background-layer-sheet-v001.png",
        "family": "old gate background layer sheet",
        "kind": "background",
        "assets": [
            ("old_gate_backdrop", "painted Old Gate backdrop", (18, 51, 466, 369), "center"),
            ("north_road_backdrop", "forest road continuation backdrop", (485, 22, 854, 369), "center"),
            ("route_strip_base", "route strip base", (883, 35, 1142, 356), "center"),
            ("destination_plaque_a", "destination plaque base", (1179, 56, 1508, 331), "center"),
            ("destination_plaque_b", "destination plaque base", (34, 403, 368, 669), "center"),
            ("destination_plaque_c", "destination plaque base", (424, 404, 759, 670), "center"),
            ("destination_plaque_d", "destination plaque base", (815, 405, 1152, 671), "center"),
            ("lock_overlay", "locked route overlay", (1215, 439, 1497, 647), "center"),
            ("quest_highlight_ring", "quest highlight ring overlay", (61, 732, 397, 946), "center"),
            ("torch_wall", "wall torch overlay", (523, 744, 679, 983), "bottom"),
            ("hanging_brazier", "hanging brazier overlay", (909, 710, 1067, 981), "bottom"),
            ("ember_particles", "ember particles overlay", (1250, 808, 1375, 950), "center"),
        ],
    },
    "sprites": {
        "path": SOURCE_DIR / "ember-road-hero-npc-enemy-sprite-sheet-v001.png",
        "family": "hero npc enemy sprite sheet",
        "kind": "sprite",
        "assets": [
            ("hero_back", "hero back three-quarter pose", (38, 94, 391, 513), "bottom"),
            ("hero_combat", "hero combat pose", (429, 195, 750, 514), "bottom"),
            ("gate_warden_portrait", "Gate Warden portrait", (787, 140, 1127, 498), "center"),
            ("gate_warden_standing", "Gate Warden standing marker", (1225, 62, 1449, 525), "bottom"),
            ("road_wolf_side", "Road Wolf side pose", (55, 636, 463, 883), "bottom"),
            ("road_wolf_combat", "Road Wolf combat pose", (515, 624, 961, 899), "bottom"),
            ("ember_slash", "ember slash hit effect", (1048, 614, 1448, 907), "center"),
        ],
    },
    "icons": {
        "path": SOURCE_DIR / "ember-road-quest-reward-route-icon-sheet-v001.png",
        "family": "quest reward route icon sheet",
        "kind": "icon",
        "assets": [
            ("quest_marker", "quest marker icon", (129, 78, 318, 361), "center"),
            ("route_arrow", "route arrow icon", (447, 139, 683, 314), "center"),
            ("locked_mine", "locked mine icon", (770, 99, 1047, 338), "center"),
            ("wolf_marker", "wolf encounter marker", (1153, 82, 1419, 361), "center"),
            ("ring_reward", "ring reward icon", (115, 435, 319, 658), "center"),
            ("xp_spark", "XP spark icon", (425, 420, 662, 662), "center"),
            ("gold_coin", "gold coin icon", (802, 446, 1004, 658), "center"),
            ("sword_auto_battle", "sword auto-battle icon", (1162, 438, 1397, 675), "center"),
            ("claim_check", "claim/completed icon", (623, 708, 860, 949), "center"),
        ],
    },
    "ui": {
        "path": SOURCE_DIR / "ember-road-fantasy-browser-rpg-ui-frame-sheet-v001.png",
        "family": "fantasy browser rpg ui frame sheet",
        "kind": "ui",
        "assets": [
            ("top_status_frame", "top hero/status frame strip", (32, 20, 953, 218), "center"),
            ("portrait_round_frame", "round portrait frame", (955, 40, 1047, 144), "center"),
            ("status_meter_stack", "status meter stack", (1058, 35, 1225, 146), "center"),
            ("quest_rail_panel", "right quest rail panel base", (1195, 110, 1504, 541), "center"),
            ("small_panel", "small framed panel", (30, 214, 273, 419), "center"),
            ("wide_panel", "wide framed panel", (291, 178, 1148, 419), "center"),
            ("reward_slot_a", "reward slot base", (33, 437, 245, 561), "center"),
            ("reward_slot_b", "reward slot base", (257, 437, 468, 561), "center"),
            ("reward_slot_c", "reward slot base", (480, 437, 689, 561), "center"),
            ("reward_slot_d", "reward slot base", (698, 437, 906, 561), "center"),
            ("reward_slot_e", "reward slot base", (919, 437, 1128, 561), "center"),
            ("primary_button_default", "primary button default", (34, 585, 325, 666), "center"),
            ("primary_button_pressed", "primary button pressed", (357, 586, 627, 664), "center"),
            ("primary_button_disabled", "primary button disabled", (650, 586, 907, 664), "center"),
            ("primary_button_selected", "primary button selected", (928, 586, 1174, 664), "center"),
            ("bottom_log_belt", "bottom log/action belt", (30, 680, 1149, 837), "center"),
            ("route_plaque_frame", "route plaque frame", (22, 858, 390, 989), "center"),
            ("corner_cap_a", "corner/cap overlay", (413, 881, 545, 979), "center"),
            ("corner_cap_b", "corner/cap overlay", (566, 891, 652, 976), "center"),
            ("corner_cap_c", "corner/cap overlay", (681, 892, 742, 972), "center"),
            ("medallion_red", "red medallion overlay", (765, 879, 863, 984), "center"),
            ("medallion_gold", "gold medallion overlay", (873, 876, 972, 981), "center"),
            ("gem_red", "red gem overlay", (993, 888, 1059, 973), "center"),
            ("gem_gold", "gold gem overlay", (1079, 890, 1128, 980), "center"),
            ("gem_small", "small gem overlay", (1150, 916, 1207, 973), "center"),
            ("rail_arc_a", "rail arc overlay", (1198, 552, 1506, 669), "center"),
            ("rail_arc_b", "rail arc overlay", (1251, 643, 1447, 717), "center"),
            ("rail_arc_c", "rail arc overlay", (1219, 783, 1486, 854), "center"),
            ("rail_arc_d", "rail arc overlay", (1239, 846, 1465, 923), "center"),
        ],
    },
}

SLICE9 = {
    "bottom_log_belt": (18, 18, 16, 16, "status_strip_only"),
    "destination_plaque_a": (14, 14, 12, 12, "compact_only"),
    "destination_plaque_b": (14, 14, 12, 12, "compact_only"),
    "destination_plaque_c": (14, 14, 12, 12, "compact_only"),
    "destination_plaque_d": (14, 14, 12, 12, "compact_only"),
    "primary_button_default": (20, 20, 16, 16, "flexible"),
    "primary_button_disabled": (20, 20, 16, 16, "flexible"),
    "primary_button_pressed": (20, 20, 16, 16, "flexible"),
    "primary_button_selected": (20, 20, 16, 16, "flexible"),
    "quest_rail_panel": (20, 20, 20, 20, "large_only"),
    "reward_slot_a": (12, 12, 12, 12, "icon_slot_only"),
    "reward_slot_b": (12, 12, 12, 12, "icon_slot_only"),
    "reward_slot_c": (12, 12, 12, 12, "icon_slot_only"),
    "reward_slot_d": (12, 12, 12, 12, "icon_slot_only"),
    "reward_slot_e": (12, 12, 12, 12, "icon_slot_only"),
    "route_plaque_frame": (14, 14, 14, 14, "compact_only"),
    "route_strip_base": (18, 18, 14, 14, "large_only"),
    "small_panel": (12, 12, 12, 12, "compact_only"),
    "status_meter_stack": (10, 10, 8, 8, "status_strip_only"),
    "top_status_frame": (18, 18, 16, 16, "status_strip_only"),
    "wide_panel": (18, 18, 16, 16, "large_only"),
}


def project_path(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def rect_xywh(rect: tuple[int, int, int, int]) -> list[int]:
    x0, y0, x1, y1 = rect
    return [x0, y0, x1 - x0, y1 - y0]


def clamp_rect(rect: tuple[int, int, int, int], width: int, height: int, pad: int = 8) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = rect
    return max(0, x0 - pad), max(0, y0 - pad), min(width, x1 + pad), min(height, y1 + pad)


def is_chroma_like(r: int, g: int, b: int) -> bool:
    if g > 140 and r < 112 and b < 112:
        return True
    return g > 90 and r < 96 and b < 96 and g > r * 1.45 and g > b * 1.45


def touches_transparent(pixels, x: int, y: int, width: int, height: int) -> bool:
    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
        if nx < 0 or ny < 0 or nx >= width or ny >= height:
            return True
        if pixels[nx, ny][3] == 0:
            return True
    return False


def keyed_crop(image: Image.Image, rect: tuple[int, int, int, int]) -> Image.Image:
    crop = image.crop(rect).convert("RGBA")
    pixels = crop.load()
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b, a = pixels[x, y]
            if is_chroma_like(r, g, b):
                pixels[x, y] = (0, 0, 0, 0)
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b, a = pixels[x, y]
            on_outer_edge = x < 2 or y < 2 or x >= crop.width - 2 or y >= crop.height - 2
            green_edge = g > 95 and g > r + 32 and g > b + 32 and (on_outer_edge or touches_transparent(pixels, x, y, crop.width, crop.height))
            if a > 0 and green_edge:
                if r < 120 and b < 120:
                    pixels[x, y] = (0, 0, 0, 0)
                else:
                    neutral = max(r, b)
                    pixels[x, y] = (r, min(g, neutral + 8), b, a)
            elif a > 0 and g > r + 18 and g > b + 18 and touches_transparent(pixels, x, y, crop.width, crop.height):
                neutral = max(r, b)
                pixels[x, y] = (r, min(g, neutral + 6), b, a)
    return crop


def slice9_metadata(asset_id: str, rect: tuple[int, int, int, int]) -> dict | None:
    if asset_id not in SLICE9:
        return None
    left, right, top, bottom, size_class = SLICE9[asset_id]
    width, height = rect_xywh(rect)[2:]
    min_width = max(left + right + 16, min(width, 96))
    min_height = max(top + bottom + 12, min(height, 48))
    content = {
        "x": left + 4,
        "y": top + 4,
        "w": max(1, width - left - right - 8),
        "h": max(1, height - top - bottom - 8),
    }
    return {
        "kind": "slice9",
        "slice9": {"left": left, "top": top, "right": right, "bottom": bottom},
        "content": content,
        "target_preview_sizes": [[min_width, min_height], [max(width, min_width * 2), max(height, min_height * 2)]],
        "stretch_policy": {
            "center": "plain_texture",
            "horizontal_edges": "straight_frame",
            "vertical_edges": "straight_frame",
            "corners": "decorative_fixed",
            "non_stretch_ornaments": "none",
        },
        "usage_policy": {
            "size_class": size_class,
            "min_size": [min_width, min_height],
            "disallowed_uses": ["unique icon", "baked text", "character sprite"],
        },
    }


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    crop_sources = []
    runtime_assets = []

    for source_id, source in SOURCES.items():
        image = Image.open(source["path"]).convert("RGBA")
        width, height = image.size
        crops = []
        for asset_id, role, rect, pivot in source["assets"]:
            padded = clamp_rect(rect, width, height)
            crop = keyed_crop(image, padded)
            rel_output = Path("assets/runtime/ember-road-old-gate-fakeshot-v001") / f"{asset_id}.png"
            save_image_atomic(crop, ROOT / rel_output, format="PNG")
            metadata = slice9_metadata(asset_id, rect)
            asset = {
                "id": asset_id,
                "kind": metadata["kind"] if metadata else source["kind"],
                "source_family": source["family"],
                "role": role,
                "path": rel_output.as_posix(),
                "output": rel_output.as_posix(),
                "source": project_path(source["path"]),
                "rect": rect_xywh(rect),
                "crop_rect": list(rect),
                "padded_rect": rect_xywh(padded),
                "pivot": pivot,
                "alpha": "chroma_key_removed",
            }
            if metadata:
                asset.update(metadata)
            if source["kind"] == "icon":
                asset["semantic_role"] = role
                asset["size_class"] = "64px source, runtime may scale down"
                asset["trim_padding"] = 8
                asset["isolate_component"] = "one centered icon crop from chroma-key source sheet"
            runtime_assets.append(asset)
            crops.append(asset)

        crop_sources.append(
            {
                "id": source_id,
                "source_family": source["family"],
                "path": project_path(source["path"]),
                "size": [width, height],
                "crops": crops,
            }
        )

    crop_manifest = {
        "schema": "game.art_crop_manifest",
        "version": 1,
        "art_job": JOB,
        "output_dir": "assets/runtime/ember-road-old-gate-fakeshot-v001",
        "green_screen": {
            "mode": "chroma_key",
            "key": "#00ff00",
            "removal": "bright chroma threshold plus edge-only green decontamination; source sheets use flat chroma with gutters",
        },
        "sources": crop_sources,
    }
    write_json_atomic(CROP_MANIFEST, crop_manifest)

    asset_manifest = {
        "schema": "game.asset_manifest",
        "version": 1,
        "art_job": JOB,
        "crop_manifest": project_path(CROP_MANIFEST),
        "runtime_dir": "assets/runtime/ember-road-old-gate-fakeshot-v001",
        "commands": {
            "slice_assets": "python tools/ember-road/slice_old_gate_assets.py",
            "slice9_design_audit": "node tools/assets/job/audit_slice9_design_policy.mjs --crop-manifest gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-crop_manifest.json --runtime-manifest gamedesign/projects/ember-road/data/ember-road-old-gate-fakeshot-v001-asset_manifest.json --json-output gamedesign/projects/ember-road/reviews/ember-road-old-gate-fakeshot-v001-slice9_design_policy_audit.json --report gamedesign/projects/ember-road/reviews/ember-road-old-gate-fakeshot-v001-slice9_design_policy_audit.md",
            "build_pack": "cmake --build --preset native-debug --target ember_road_runtime_assets",
            "native_evidence": "py -3.12 tools/devapi/smoke.py",
        },
        "assets": runtime_assets,
    }
    write_json_atomic(ASSET_MANIFEST, asset_manifest)

    print(f"wrote {len(runtime_assets)} runtime assets to {project_path(OUT_DIR)}")
    print(f"wrote {project_path(CROP_MANIFEST)}")
    print(f"wrote {project_path(ASSET_MANIFEST)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
