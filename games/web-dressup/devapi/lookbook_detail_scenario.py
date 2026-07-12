"""Open one exact saved-look card for responsive visual proof."""

from __future__ import annotations

from typing import Any

import smoke_bot


def prepare_lookbook_detail(game: Any, _viewport: Any) -> dict[str, Any]:
    methods = game.endpoint_methods()
    smoke_bot.wait_for_ui_id(game, smoke_bot.MOON_MAIN_ID, methods=methods)
    smoke_bot.click_and_advance(game, smoke_bot.MOON_MAIN_ID, methods)
    smoke_bot.click_and_advance(game, smoke_bot.ACCENT_CATEGORY_ID, methods)
    smoke_bot.click_and_advance(game, smoke_bot.BLOOM_ACCENT_ID, methods)
    smoke_bot.confirm_support_decisions(game, methods)
    smoke_bot.awaken_to_card(game, methods)
    smoke_bot.click_and_advance(game, smoke_bot.RESTYLE_ID, methods)
    smoke_bot.click_and_advance(game, f"{smoke_bot.LOOKBOOK_OPEN_ID}/control", methods)
    smoke_bot.click_and_advance(game, "lookbook/recipe/3/control", methods)
    tree = smoke_bot.wait_for_ui_id(game, smoke_bot.LOOKBOOK_WEAR_ID, methods=methods)
    ids = {node.get("id_string") for node in smoke_bot.ui_nodes(tree)}
    required = {"lookbook/back", smoke_bot.LOOKBOOK_WEAR_ID, smoke_bot.LOOKBOOK_CREATE_ANOTHER_ID}
    missing = sorted(required - ids)
    if missing:
        raise smoke_bot.DevApiError(f"saved-look detail missing controls: {missing}")
    return {"saved_recipe": 3, "required_controls": sorted(required)}
