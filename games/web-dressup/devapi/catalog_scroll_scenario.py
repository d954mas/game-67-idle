"""Responsive runtime proof for six-item horizontal dress catalogs."""

from __future__ import annotations

import contextlib
from typing import Any

import smoke_bot


BOTTOM_CATEGORY = "dress/category/2"
SHOES_CATEGORY = "dress/category/3"
TOP_CATEGORY = "dress/category/1"
CATALOG_ID = "dress/catalog"
TOP_IDS = ["top_tee", "top_hoodie", "top_blazer", "top_crop"]
BOTTOM_IDS = [
    "bot_jeans",
    "bot_skirt",
    "bot_shorts",
    "bot_cargo",
    "bot_moonveil",
    "bot_phoenix",
]
SHOES_IDS = [
    "shoe_sneak",
    "shoe_boot",
    "shoe_heel",
    "shoe_sandal",
    "shoe_eclipse",
    "shoe_phoenix",
]


def _node(tree: dict[str, Any], element_id: str) -> dict[str, Any]:
    found = smoke_bot.find_ui_node(tree, element_id)
    if found is None:
        raise smoke_bot.DevApiError(f"missing semantic UI id: {element_id}")
    return found


def _all_item_ids(tree: dict[str, Any], item_ids: list[str]) -> list[str]:
    expected = [f"dress/item/{item_id}" for item_id in item_ids]
    missing = [element_id for element_id in expected if smoke_bot.find_ui_node(tree, element_id) is None]
    if missing:
        raise smoke_bot.DevApiError(f"catalog omitted semantic item ids: {missing}")
    return expected


def _intersects(a: dict[str, Any], b: dict[str, Any], *, inset: float = 3.0) -> bool:
    ax0 = float(a["x"])
    ay0 = float(a["y"])
    ax1 = ax0 + float(a["w"])
    ay1 = ay0 + float(a["h"])
    bx0 = float(b["x"]) + inset
    by0 = float(b["y"]) + inset
    bx1 = float(b["x"]) + float(b["w"]) - inset
    by1 = float(b["y"]) + float(b["h"]) - inset
    return min(ax1, bx1) > max(ax0, bx0) and min(ay1, by1) > max(ay0, by0)


def _contained(a: dict[str, Any], b: dict[str, Any], *, inset: float = 3.0) -> bool:
    return (
        float(a["x"]) >= float(b["x"]) + inset
        and float(a["y"]) >= float(b["y"]) + inset
        and float(a["x"]) + float(a["w"]) <= float(b["x"]) + float(b["w"]) - inset
        and float(a["y"]) + float(a["h"]) <= float(b["y"]) + float(b["h"]) - inset
    )


def _select_category(game: Any, element_id: str, methods: set[str]) -> dict[str, Any]:
    smoke_bot.wait_for_ui_id(game, element_id, methods=methods)
    smoke_bot.click_and_advance(game, element_id, methods, frames=3)
    return smoke_bot.wait_for_ui_id(game, CATALOG_ID, methods=methods)


def _drag_to_last_item(game: Any, methods: set[str]) -> dict[str, Any]:
    tree = smoke_bot.wait_for_ui_id(game, CATALOG_ID, methods=methods)
    catalog = _node(tree, CATALOG_ID)["bounds"]
    y = float(catalog["y"]) + float(catalog["h"]) * 0.5
    start_x = float(catalog["x"]) + float(catalog["w"]) - 16.0
    end_x = float(catalog["x"]) + 16.0
    gate = (
        game.player_gated()
        if "input.set_player_enabled" in methods and hasattr(game, "player_gated")
        else contextlib.nullcontext(game)
    )
    with gate:
        game.result(
            "ui.drag",
            {
                "from": {"x": start_x, "y": y},
                "to": {"x": end_x, "y": y},
                "frames": 10,
            },
        )
        smoke_bot.advance_safely(game, 16, methods)
    return smoke_bot.wait_for_ui_id(game, "dress/item/bot_phoenix", methods=methods)


def prepare_catalog_scroll(game: Any, viewport: Any) -> dict[str, Any]:
    """Assert six-item semantics everywhere and swipe/click reachability at 640x360."""

    methods = game.endpoint_methods()
    bottom_tree = _select_category(game, BOTTOM_CATEGORY, methods)
    bottom_semantic_ids = _all_item_ids(bottom_tree, BOTTOM_IDS)

    shoes_tree = _select_category(game, SHOES_CATEGORY, methods)
    shoes_semantic_ids = _all_item_ids(shoes_tree, SHOES_IDS)

    top_tree = _select_category(game, TOP_CATEGORY, methods)
    top_semantic_ids = _all_item_ids(top_tree, TOP_IDS)
    top_catalog_bounds = _node(top_tree, CATALOG_ID)["bounds"]
    clipped_top_ids = [
        element_id
        for element_id in top_semantic_ids
        if not _contained(_node(top_tree, element_id)["bounds"], top_catalog_bounds)
    ]
    if clipped_top_ids:
        raise smoke_bot.DevApiError(f"four-item Top catalog regressed into clipping: {clipped_top_ids}")

    bottom_tree = _select_category(game, BOTTOM_CATEGORY, methods)
    result: dict[str, Any] = {
        "bottom_semantic_ids": bottom_semantic_ids,
        "shoes_semantic_ids": shoes_semantic_ids,
        "four_item_top_fit": "pass",
        "touch_targets_min": min(
            min(float(_node(bottom_tree, f"dress/item/{item_id}")["bounds"][axis]) for axis in ("w", "h"))
            for item_id in BOTTOM_IDS
        ),
    }

    if viewport.width == 640 and viewport.height == 360:
        tree = _drag_to_last_item(game, methods)
        catalog_bounds = _node(tree, CATALOG_ID)["bounds"]
        last_bounds = _node(tree, "dress/item/bot_phoenix")["bounds"]
        if not _intersects(last_bounds, catalog_bounds):
            raise smoke_bot.DevApiError(
                f"sixth Bottom item remained outside catalog after swipe: item={last_bounds}, catalog={catalog_bounds}"
            )
        smoke_bot.click_and_advance(game, "dress/item/bot_phoenix", methods, frames=3)
        state = smoke_bot.validate_game_state(game.result("game.state.get", {"path": ""}))
        equipped = state["value"]["game"].get("outfit_bottom_id")
        if equipped != "bot_phoenix":
            raise smoke_bot.DevApiError(f"sixth Bottom item was visible but not clickable: equipped={equipped!r}")
        result.update(
            {
                "compact_semantic_reachability": "pass",
                "equipped_after_swipe": equipped,
                "last_item_bounds": last_bounds,
                "catalog_bounds": catalog_bounds,
            }
        )

    if result["touch_targets_min"] < 44.0:
        raise smoke_bot.DevApiError(
            f"catalog touch target below 44 logical pixels: {result['touch_targets_min']}"
        )
    return result
