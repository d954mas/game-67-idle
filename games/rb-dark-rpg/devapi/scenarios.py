"""Game-owned DevAPI scenario hooks for rb-dark-rpg runtime evidence."""

from __future__ import annotations

from typing import Any


def _ui_nodes(tree: Any) -> list[dict[str, Any]]:
    if not isinstance(tree, dict):
        return []
    nodes = tree.get("nodes")
    return [node for node in nodes if isinstance(node, dict)] if isinstance(nodes, list) else []


def _viewport_size(viewport: Any, tree_viewport: dict[str, Any]) -> tuple[float, float]:
    width = getattr(viewport, "width", None)
    height = getattr(viewport, "height", None)
    if isinstance(width, (int, float)) and isinstance(height, (int, float)):
        return float(width), float(height)
    raw = getattr(viewport, "window_size", "")
    if isinstance(raw, str) and "x" in raw.lower():
        left, right = raw.lower().split("x", 1)
        try:
            return float(left), float(right)
        except ValueError:
            pass
    return float(tree_viewport.get("w", 0.0)), float(tree_viewport.get("h", 0.0))


def _tap_node(game: Any, tree: dict[str, Any], node: dict[str, Any], viewport: Any) -> None:
    id_string = node.get("id_string")
    if isinstance(id_string, str) and id_string:
        game.wait_frames(2)
        game.result("ui.click", {"id": id_string, "button": "left"})
        game.wait_frames(6)
        return

    bounds = node.get("bounds")
    tree_viewport = tree.get("viewport")
    if not isinstance(bounds, dict) or not isinstance(tree_viewport, dict):
        raise RuntimeError("ui.tree node has no bounds")

    x = float(bounds["x"]) + float(bounds["w"]) * 0.5
    y = float(bounds["y"]) + float(bounds["h"]) * 0.5
    try:
        game.wait_frames(2)
        game.result("ui.click", {"id": {"x": x, "y": y}, "hold": 6})
        game.wait_frames(10)
        return
    except Exception:
        pass

    fb_w, fb_h = _viewport_size(viewport, tree_viewport)
    ui_w = float(tree_viewport.get("w", fb_w) or fb_w)
    ui_h = float(tree_viewport.get("h", fb_h) or fb_h)
    ui_x = float(tree_viewport.get("x", 0.0))
    ui_y = float(tree_viewport.get("y", 0.0))
    x_fb = (x - ui_x) * fb_w / ui_w
    y_fb = (y - ui_y) * fb_h / ui_h
    try:
        game.result("input.click", {"x": x_fb, "y": y_fb, "button": 1, "hold": 1})
    except Exception:
        try:
            game.result("input.gesture", {"id": 1, "type": "mouse", "points": [[x_fb, y_fb]], "frame_stride": 0})
        except Exception:
            id_string = node.get("id_string")
            if isinstance(id_string, str) and id_string:
                game.result("ui.click", {"id": id_string, "button": "left"})
            else:
                game.result("ui.click", {"id": {"x": x, "y": y}, "button": "left"})
    game.wait_frames(4)


def _tap_node_by_bounds(game: Any, tree: dict[str, Any], node: dict[str, Any], viewport: Any) -> None:
    bounds = node.get("bounds")
    tree_viewport = tree.get("viewport")
    if not isinstance(bounds, dict) or not isinstance(tree_viewport, dict):
        raise RuntimeError("ui.tree node has no bounds")
    x = float(bounds["x"]) + float(bounds["w"]) * 0.5
    y = float(bounds["y"]) + float(bounds["h"]) * 0.5
    fb_w, fb_h = _viewport_size(viewport, tree_viewport)
    ui_w = float(tree_viewport.get("w", fb_w) or fb_w)
    ui_h = float(tree_viewport.get("h", fb_h) or fb_h)
    ui_x = float(tree_viewport.get("x", 0.0))
    ui_y = float(tree_viewport.get("y", 0.0))
    x_fb = (x - ui_x) * fb_w / ui_w
    y_fb = (y - ui_y) * fb_h / ui_h
    try:
        game.result("input.click", {"x": x_fb, "y": y_fb, "button": 1, "hold": 2})
        game.wait_frames(8)
        return
    except Exception:
        pass
    game.wait_frames(2)
    game.result("ui.click", {"id": {"x": x, "y": y}, "button": "left", "hold": 6})
    game.wait_frames(10)


def _open_equipment_nav(game: Any, viewport: Any) -> None:
    _open_bottom_nav_slot(game, viewport, 0)


def _ensure_equipment_open(game: Any, viewport: Any) -> None:
    last_error: RuntimeError | None = None
    for _ in range(4):
        try:
            _wait_for_node(game, "equipment/modal_frame", max_frames=24, stride=3)
            return
        except RuntimeError as exc:
            last_error = exc
        try:
            _tap_by_id(game, viewport, "bottom_nav/slot/equipment", max_frames=12)
        except RuntimeError:
            tree, node = _wait_for_node(game, "bottom_nav/slot/equipment", max_frames=30)
            _tap_node_by_bounds(game, tree, node, viewport)
        game.wait_frames(12)
    try:
        _wait_for_node(game, "equipment/modal_frame", max_frames=36, stride=3)
        return
    except RuntimeError as exc:
        raise last_error or exc


def _open_bottom_nav_slot(game: Any, viewport: Any, slot: int) -> None:
    if slot == 2:
        try:
            _wait_for_node(game, "world_map/atlas_canvas", max_frames=24)
            return
        except RuntimeError:
            pass

    def _wait_for_slot_surface() -> bool:
        expected = {
            0: "equipment/modal_frame",
            2: "world_map/atlas_canvas",
            3: "world_place/tabs",
        }.get(slot)
        if not expected:
            return True
        try:
            _wait_for_node(game, expected, max_frames=36, stride=3)
            return True
        except RuntimeError:
            return False

    if _wait_for_slot_surface():
        return

    slot_ids = [
        "bottom_nav/slot/equipment",
        "bottom_nav/slot/journal",
        "bottom_nav/slot/map",
        "bottom_nav/slot/place",
        "bottom_nav/slot/more",
    ]
    if 0 <= slot < len(slot_ids):
        try:
            _tap_by_id(game, viewport, slot_ids[slot], max_frames=30)
            if _wait_for_slot_surface():
                return
        except RuntimeError:
            pass

    button_ids = [
        "ui/nav_v11_equipment",
        "ui/nav_v11_journal",
        "ui/nav_v11_map",
        "ui/nav_v11_place",
        "ui/nav_v11_more",
    ]
    if 0 <= slot < len(button_ids):
        try:
            _tap_by_id(game, viewport, button_ids[slot], max_frames=30)
            if _wait_for_slot_surface():
                return
        except RuntimeError:
            pass

    tree = game.result("ui.tree")
    nodes = _ui_nodes(tree)
    root = next((node for node in nodes if node.get("id_string") == "bottom_nav/root"), None)
    if root is None:
        raise RuntimeError("bottom_nav/root not found")
    buttons = [
        node
        for node in nodes
        if node.get("parent") == root.get("id") and isinstance(node.get("bounds"), dict)
    ]
    if not buttons:
        raise RuntimeError("bottom nav buttons not found")
    buttons.sort(key=lambda node: float(node["bounds"].get("x", 0.0)))
    if slot < 0 or slot >= len(buttons):
        raise RuntimeError(f"bottom nav slot {slot} not found")
    _tap_node(game, tree, buttons[slot], viewport)
    if not _wait_for_slot_surface():
        raise RuntimeError(f"bottom nav slot {slot} did not open expected surface")


def _wait_for_node(game: Any, element_id: str, *, max_frames: int = 180, stride: int = 4) -> tuple[dict[str, Any], dict[str, Any]]:
    last_ids: list[str] = []
    for _ in range(max(1, max_frames // max(1, stride))):
        tree = game.result("ui.tree")
        last_ids = sorted(
            node.get("id_string")
            for node in _ui_nodes(tree)
            if isinstance(node.get("id_string"), str) and node.get("id_string")
        )
        node = next((item for item in _ui_nodes(tree) if item.get("id_string") == element_id), None)
        if node is not None:
            return tree, node
        game.wait_frames(stride)
    preview = ", ".join(last_ids[:80])
    suffix = "..." if len(last_ids) > 80 else ""
    raise RuntimeError(f"ui node {element_id!r} not found; visible ids: {preview}{suffix}")


def _tap_by_id(game: Any, viewport: Any, element_id: str, *, max_frames: int = 180) -> None:
    _wait_for_node(game, element_id, max_frames=max_frames)
    game.wait_frames(2)
    game.result("ui.click", {"id": element_id, "button": "left"})
    game.wait_frames(6)


def _tap_by_id_bounds(game: Any, viewport: Any, element_id: str, *, max_frames: int = 180) -> None:
    tree, node = _wait_for_node(game, element_id, max_frames=max_frames)
    _tap_node_by_bounds(game, tree, node, viewport)


def _open_place_tab(game: Any, viewport: Any, tab: str) -> None:
    _tap_by_id(game, viewport, f"world_place/tab/{tab}", max_frames=60)


def _prepare_gate_combat_state(game: Any) -> None:
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "hero.hp": 30,
                "inventory.gear_instances": {
                    "gear_old_sword_001": {
                        "def_id": "old_sword",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_padded_jacket_001": {
                        "def_id": "padded_jacket",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_leather_greaves_001": {
                        "def_id": "leather_greaves",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                },
                "inventory.bag_order": [
                    "gear_old_sword_001",
                    "gear_padded_jacket_001",
                    "gear_leather_greaves_001",
                ],
                "equipment.weapon_instance_id": "gear_old_sword_001",
                "equipment.armour_instance_id": "gear_padded_jacket_001",
                "equipment.legs_instance_id": "gear_leather_greaves_001",
                "world.current_location_id": "hub_last_post",
                "world.visited_location_ids": ["hub_last_post"],
                "quests.tracked_quest_id": "q001_gate_pass",
                "quests.quest_states": {
                    "q001_gate_pass": {
                        "status": "active",
                        "current_step_id": "clear_gate_scavenger",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_combat_fixture",
                    }
                },
                "quests.completed_step_ids": ["talk_gate_guard", "equip_old_sword", "equip_padded_jacket", "equip_leather_greaves"],
                "quests.claimed_reward_ids": [],
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "dev_world_place_open",
                ],
            },
        },
    )
    game.wait_frames(3)


def _prepare_gate_travel_state(game: Any) -> None:
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "hero.hp": 30,
                "inventory.gear_instances": {
                    "gear_old_sword_001": {
                        "def_id": "old_sword",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_padded_jacket_001": {
                        "def_id": "padded_jacket",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_leather_greaves_001": {
                        "def_id": "leather_greaves",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                },
                "inventory.bag_order": [
                    "gear_old_sword_001",
                    "gear_padded_jacket_001",
                    "gear_leather_greaves_001",
                ],
                "equipment.weapon_instance_id": "gear_old_sword_001",
                "equipment.armour_instance_id": "gear_padded_jacket_001",
                "equipment.legs_instance_id": "gear_leather_greaves_001",
                "world.current_location_id": "hub_last_post",
                "world.visited_location_ids": ["hub_last_post"],
                "quests.tracked_quest_id": "q001_gate_pass",
                "quests.quest_states": {
                    "q001_gate_pass": {
                        "status": "active",
                        "current_step_id": "clear_gate_scavenger",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_world_map_fixture",
                    }
                },
                "quests.completed_step_ids": [
                    "talk_gate_guard",
                    "equip_old_sword",
                    "equip_padded_jacket",
                    "equip_leather_greaves",
                ],
                "quests.claimed_reward_ids": [],
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "map_gate_unlocked",
                    "dev_world_map_open",
                ],
            },
        },
    )
    game.wait_frames(3)


def _prepare_gate_turn_in_state(game: Any) -> None:
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "hero.hp": 30,
                "hero.xp": 0,
                "wallet.gold": 0,
                "world.current_location_id": "hub_last_post",
                "world.visited_location_ids": ["hub_last_post", "hub_gate_outskirts"],
                "quests.tracked_quest_id": "q001_gate_pass",
                "quests.quest_states": {
                    "q001_gate_pass": {
                        "status": "ready_to_turn_in",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_guard_turn_in_fixture",
                    }
                },
                "quests.completed_step_ids": [
                    "talk_gate_guard",
                    "equip_old_sword",
                    "equip_padded_jacket",
                    "equip_leather_greaves",
                    "clear_gate_scavenger",
                    "report_to_gate_guard",
                ],
                "quests.claimed_reward_ids": [],
                "inventory.stack_instances": {},
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "gate_scavenger_defeated",
                ],
            },
        },
    )
    game.wait_frames(3)


def _wait_for_state_value(game: Any, path: str, expected: Any, *, max_frames: int = 120, stride: int = 4) -> Any:
    last_value = None
    for _ in range(max(1, max_frames // max(1, stride))):
        state = game.result("game.state.get", {"path": path})
        last_value = state.get("value") if isinstance(state, dict) else None
        if last_value == expected:
            return last_value
        game.wait_frames(stride)
    raise RuntimeError(f"state {path!r} expected {expected!r}, got {last_value!r}")


def _clear_dev_world_place_flag(game: Any) -> None:
    state = game.result("game.state.get", {"path": "flags.ids"})
    flags = state.get("value") if isinstance(state, dict) else None
    if not isinstance(flags, list) or "dev_world_place_open" not in flags:
        return
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {"flags.ids": [flag for flag in flags if flag != "dev_world_place_open"]},
        },
    )
    game.wait_frames(2)


def _clear_dev_world_map_flag(game: Any) -> None:
    state = game.result("game.state.get", {"path": "flags.ids"})
    flags = state.get("value") if isinstance(state, dict) else None
    if not isinstance(flags, list) or "dev_world_map_open" not in flags:
        return
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {"flags.ids": [flag for flag in flags if flag != "dev_world_map_open"]},
        },
    )
    game.wait_frames(2)


def _wait_for_quest_status(game: Any, quest_id: str, expected: str, *, max_frames: int = 120, stride: int = 4) -> dict[str, Any]:
    last_quest = None
    for _ in range(max(1, max_frames // max(1, stride))):
        state = game.result("game.state.get", {"path": "quests.quest_states"})
        value = state.get("value") if isinstance(state, dict) else None
        last_quest = value.get(quest_id) if isinstance(value, dict) else None
        if isinstance(last_quest, dict) and last_quest.get("status") == expected:
            return last_quest
        game.wait_frames(stride)
    raise RuntimeError(f"quest {quest_id!r} expected status {expected!r}, got {last_quest!r}")


def _wait_for_stack_count(game: Any, item_id: str, expected: int, *, max_frames: int = 120, stride: int = 4) -> int:
    last_count = None
    for _ in range(max(1, max_frames // max(1, stride))):
        state = game.result("game.state.get", {"path": "inventory.stack_instances"})
        stacks = state.get("value") if isinstance(state, dict) else None
        stack = stacks.get(item_id) if isinstance(stacks, dict) else None
        last_count = stack.get("count", 0) if isinstance(stack, dict) else 0
        if last_count == expected:
            return last_count
        game.wait_frames(stride)
    raise RuntimeError(f"stack {item_id!r} expected {expected!r}, got {last_count!r}")


def _wait_for_gear_def_count(game: Any, def_id: str, expected: int, *, max_frames: int = 120, stride: int = 4) -> int:
    last_count = None
    for _ in range(max(1, max_frames // max(1, stride))):
        state = game.result("game.state.get", {"path": "inventory.gear_instances"})
        gear_instances = state.get("value") if isinstance(state, dict) else None
        last_count = 0
        if isinstance(gear_instances, dict):
            for gear in gear_instances.values():
                if isinstance(gear, dict) and gear.get("def_id") == def_id:
                    last_count += 1
        if last_count == expected:
            return last_count
        game.wait_frames(stride)
    raise RuntimeError(f"gear def {def_id!r} expected {expected!r}, got {last_count!r}")


def _prepare_shop_fixture(game: Any) -> None:
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "hero.hp": 33,
                "hero.xp": 8,
                "wallet.gold": 20,
                "world.current_location_id": "hub_last_post",
                "world.visited_location_ids": ["hub_last_post"],
                "quests.tracked_quest_id": "q001_gate_pass",
                "quests.quest_states": {
                    "q001_gate_pass": {
                        "status": "completed",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_shop_fixture",
                    }
                },
                "quests.completed_step_ids": [
                    "talk_gate_guard",
                    "equip_old_sword",
                    "equip_padded_jacket",
                    "equip_leather_greaves",
                    "clear_gate_scavenger",
                    "report_to_gate_guard",
                ],
                "quests.claimed_reward_ids": ["encounter.gate_scavenger.win"],
                "inventory.gear_instances": {},
                "inventory.bag_order": [],
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "gate_scavenger_defeated",
                    "seeker_token_owned",
                    "map_gate_unlocked",
                    "dev_world_place_open",
                ],
            },
        },
    )
    game.wait_frames(3)


def prepare_post_trader_shop_purchase(game: Any, viewport: Any) -> dict[str, str]:
    """Open the Last Post trader shop and buy the first sword upgrade."""

    _prepare_shop_fixture(game)
    _open_bottom_nav_slot(game, viewport, 3)
    _clear_dev_world_place_flag(game)
    _open_place_tab(game, viewport, "environment")
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.town_trader")
    _tap_node(game, tree, node, viewport)
    _wait_for_node(game, "shop/modal_frame")
    _wait_for_node(game, "shop/item/iron_sword")
    _tap_by_id_bounds(game, viewport, "shop/buy/iron_sword", max_frames=60)
    _wait_for_state_value(game, "wallet.gold", 8)
    _wait_for_gear_def_count(game, "iron_sword", 1)
    _wait_for_node(game, "shop/feedback")
    return {"state": "post_trader_shop_purchase", "item": "iron_sword", "viewport": viewport.window_size}


def prepare_post_trader_shop_trade(game: Any, viewport: Any) -> dict[str, str]:
    """Open the Last Post trader shop, sell a bag item, then buy back the same instance."""

    trade_instance_id = "gear_runner_wraps_trade"
    _prepare_shop_fixture(game)
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "wallet.gold": 20,
                "inventory.gear_instances": {
                    trade_instance_id: {
                        "def_id": "runner_wraps",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    }
                },
                "inventory.bag_order": [trade_instance_id],
            },
        },
    )
    game.wait_frames(3)

    _open_bottom_nav_slot(game, viewport, 3)
    _clear_dev_world_place_flag(game)
    _open_place_tab(game, viewport, "environment")
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.town_trader")
    _tap_node(game, tree, node, viewport)
    _wait_for_node(game, "shop/modal_frame")
    _tap_by_id_bounds(game, viewport, "shop/mode/sell", max_frames=60)
    _wait_for_node(game, f"shop/sell/{trade_instance_id}")
    _tap_by_id_bounds(game, viewport, f"shop/sell/{trade_instance_id}", max_frames=60)
    _wait_for_state_value(game, "wallet.gold", 22)
    _wait_for_gear_def_count(game, "runner_wraps", 0)
    _wait_for_node(game, "shop/feedback")
    _tap_by_id_bounds(game, viewport, "shop/mode/buyback", max_frames=60)
    _wait_for_node(game, f"shop/buyback/{trade_instance_id}")
    _tap_by_id_bounds(game, viewport, f"shop/buyback/{trade_instance_id}", max_frames=60)
    _wait_for_state_value(game, "wallet.gold", 20)
    _wait_for_gear_def_count(game, "runner_wraps", 1)
    _wait_for_node(game, "shop/feedback")
    return {"state": "post_trader_shop_trade", "item": "runner_wraps", "viewport": viewport.window_size}


def prepare_world_map_move_gate(game: Any, viewport: Any) -> dict[str, str]:
    """Open the world map, click the gate-outskirts node, and stop on the Place window."""

    _prepare_gate_travel_state(game)
    _clear_dev_world_map_flag(game)
    _clear_dev_world_place_flag(game)
    _open_bottom_nav_slot(game, viewport, 2)
    tree, node = _wait_for_node(game, "world_map/region/hub_gate_outskirts")
    bounds = node.get("bounds") if isinstance(node, dict) else None
    if isinstance(bounds, dict):
        node = {
            **node,
            "bounds": {
                "x": float(bounds["x"]) + float(bounds["w"]) * 0.24,
                "y": float(bounds["y"]) + float(bounds["h"]) * 0.55,
                "w": 8.0,
                "h": 8.0,
            },
        }
    _tap_node_by_bounds(game, tree, node, viewport)
    try:
        _wait_for_node(game, "world_map/travel_timer", max_frames=60, stride=4)
    except RuntimeError:
        pass
    _wait_for_state_value(game, "world.current_location_id", "hub_gate_outskirts", max_frames=420, stride=4)
    _wait_for_node(game, "world_place/tabs", max_frames=120, stride=4)
    return {"state": "world_map_move_gate", "location": "hub_gate_outskirts", "viewport": viewport.window_size}


def prepare_world_map_auto_travel_old_mill(game: Any, viewport: Any) -> dict[str, str]:
    """Click the distant mill node and wait for the auto-route to finish."""

    _prepare_gate_travel_state(game)
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "map_gate_unlocked",
                    "old_mill_unlocked",
                    "dev_world_map_auto_travel_old_mill",
                ],
            },
        },
    )
    _clear_dev_world_map_flag(game)
    _clear_dev_world_place_flag(game)
    _open_bottom_nav_slot(game, viewport, 2)
    _wait_for_node(game, "world_map/travel_timer", max_frames=90)
    _wait_for_state_value(game, "world.current_location_id", "old_mill", max_frames=420)
    _wait_for_node(game, "world_place/tabs", max_frames=90)
    return {"state": "world_map_auto_travel_old_mill", "location": "old_mill", "viewport": viewport.window_size}


def prepare_world_map_auto_travel_old_mill_midway(game: Any, viewport: Any) -> dict[str, str]:
    """Click the distant mill node and stop while the map travel animation is visible."""

    _prepare_gate_travel_state(game)
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "map_gate_unlocked",
                    "old_mill_unlocked",
                    "dev_world_map_auto_travel_old_mill",
                ],
            },
        },
    )
    _clear_dev_world_map_flag(game)
    _clear_dev_world_place_flag(game)
    _open_bottom_nav_slot(game, viewport, 2)
    _wait_for_node(game, "world_map/travel_timer", max_frames=90)
    game.wait_frames(60)
    return {"state": "world_map_auto_travel_old_mill_midway", "location": "traveling", "viewport": viewport.window_size}


def prepare_world_map_open(game: Any, viewport: Any) -> dict[str, str]:
    """Open the authored Ash Border atlas map and stop before choosing a destination."""

    _prepare_gate_travel_state(game)
    _clear_dev_world_map_flag(game)
    _clear_dev_world_place_flag(game)
    _open_bottom_nav_slot(game, viewport, 2)
    _wait_for_node(game, "world_map/atlas_canvas")
    _wait_for_node(game, "world_map/art")
    _wait_for_node(game, "world_map/location/hub_last_post")
    _wait_for_node(game, "world_map/location/hub_gate_outskirts")
    _wait_for_node(game, "world_map/hero/ring")
    return {"state": "world_map_open", "location": "hub_last_post", "viewport": viewport.window_size}


def prepare_location_screen(game: Any, viewport: Any) -> dict[str, str]:
    """Open the standalone current-location screen without routing through Map."""

    _prepare_gate_combat_state(game)
    _wait_for_node(game, "world_place/tabs")
    return {"state": "location_screen", "location": "hub_last_post", "viewport": viewport.window_size}


def prepare_location_points_screen(game: Any, viewport: Any) -> dict[str, str]:
    """Open the standalone current-location screen and show people/points."""

    _prepare_gate_combat_state(game)
    _wait_for_node(game, "world_place/tabs")
    _open_place_tab(game, viewport, "environment")
    _wait_for_node(game, "world_place/object/hub_last_post.gate_guard")
    return {"state": "location_points_screen", "location": "hub_last_post", "viewport": viewport.window_size}


def _prepare_old_mill_inspect_state(game: Any) -> None:
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "world.current_location_id": "old_mill",
                "world.visited_location_ids": ["hub_last_post", "hub_gate_outskirts", "old_mill"],
                "quests.tracked_quest_id": "q002_bread_for_post",
                "quests.quest_states": {
                    "q002_bread_for_post": {
                        "status": "active",
                        "current_step_id": "inspect_old_mill",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_old_mill_fixture",
                    }
                },
                "quests.completed_step_ids": ["visit_old_mill"],
                "quests.claimed_reward_ids": [],
                "flags.ids": ["map_gate_unlocked", "old_mill_unlocked"],
            },
        },
    )
    game.wait_frames(3)


def prepare_old_mill_inspect_mark(game: Any, viewport: Any) -> dict[str, str]:
    """Open the current Place window and inspect the authored old-mill quest object."""

    _prepare_old_mill_inspect_state(game)
    _open_bottom_nav_slot(game, viewport, 3)
    _open_place_tab(game, viewport, "environment")
    tree, node = _wait_for_node(game, "world_place/object/old_mill.black_sun_mark")
    _tap_node(game, tree, node, viewport)
    _wait_for_state_value(game, "quests.completed_step_ids", ["visit_old_mill", "inspect_old_mill"])
    quest = _wait_for_quest_status(game, "q002_bread_for_post", "active")
    if quest.get("current_step_id") != "report_to_elder":
        raise RuntimeError(f"q002_bread_for_post current step mismatch after inspect: {quest!r}")
    return {"state": "old_mill_inspect_mark", "quest": "q002_bread_for_post", "viewport": viewport.window_size}


def prepare_gate_guard_turn_in_from_place(game: Any, viewport: Any) -> dict[str, str]:
    """Open Place, tap the guard, select the turn-in choice, and verify q001 completion."""

    _prepare_gate_turn_in_state(game)
    _open_bottom_nav_slot(game, viewport, 3)
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.gate_guard")
    _tap_node(game, tree, node, viewport)
    tree, node = _wait_for_node(game, "dialogue/primary_choice_inline")
    _tap_node(game, tree, node, viewport)
    _wait_for_quest_status(game, "q001_gate_pass", "completed")
    _wait_for_state_value(game, "hero.xp", 12)
    _wait_for_state_value(game, "quests.claimed_reward_ids", ["dlg_gate_guard_turn_in.take_token.completion"])
    _wait_for_state_value(
        game,
        "flags.ids",
        [
            "gate_guard_intro_seen",
            "starter_gear_received",
            "old_sword_equipped",
            "padded_jacket_equipped",
            "leather_greaves_equipped",
            "gate_scavenger_defeated",
            "seeker_token_owned",
            "map_gate_unlocked",
            "old_mill_unlocked",
        ],
    )
    return {"state": "gate_guard_turn_in_from_place", "quest": "q001_gate_pass", "viewport": viewport.window_size}


def prepare_q002_elder_contract_flow(game: Any, viewport: Any) -> dict[str, str]:
    """Play the first q002 story loop through Place, Map, Old Mill inspect, and Elder turn-in."""

    prepare_gate_guard_turn_in_from_place(game, viewport)

    _open_bottom_nav_slot(game, viewport, 3)
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.elder")
    _tap_node(game, tree, node, viewport)
    tree, node = _wait_for_node(game, "dialogue/primary_choice_inline")
    _tap_node(game, tree, node, viewport)
    quest = _wait_for_quest_status(game, "q002_bread_for_post", "active")
    if quest.get("current_step_id") != "visit_old_mill":
        raise RuntimeError(f"q002_bread_for_post did not start at visit_old_mill: {quest!r}")

    _open_bottom_nav_slot(game, viewport, 2)
    tree, node = _wait_for_node(game, "world_map/location/hub_gate_outskirts")
    _tap_node(game, tree, node, viewport)
    _wait_for_state_value(game, "world.current_location_id", "hub_gate_outskirts")
    _open_bottom_nav_slot(game, viewport, 2)
    tree, node = _wait_for_node(game, "world_map/location/old_mill")
    _tap_node(game, tree, node, viewport)
    _wait_for_state_value(game, "world.current_location_id", "old_mill")
    _open_place_tab(game, viewport, "environment")
    tree, node = _wait_for_node(game, "world_place/object/old_mill.black_sun_mark")
    _tap_node(game, tree, node, viewport)
    quest = _wait_for_quest_status(game, "q002_bread_for_post", "active")
    if quest.get("current_step_id") != "report_to_elder":
        raise RuntimeError(f"q002_bread_for_post did not advance to report_to_elder: {quest!r}")

    _open_bottom_nav_slot(game, viewport, 2)
    tree, node = _wait_for_node(game, "world_map/location/hub_gate_outskirts")
    _tap_node(game, tree, node, viewport)
    _wait_for_state_value(game, "world.current_location_id", "hub_gate_outskirts")
    _open_bottom_nav_slot(game, viewport, 2)
    tree, node = _wait_for_node(game, "world_map/location/hub_last_post")
    _tap_node(game, tree, node, viewport)
    _wait_for_state_value(game, "world.current_location_id", "hub_last_post")
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.elder")
    _tap_node(game, tree, node, viewport)
    tree, node = _wait_for_node(game, "dialogue/primary_choice_inline")
    _tap_node(game, tree, node, viewport)
    _wait_for_quest_status(game, "q002_bread_for_post", "completed")
    _wait_for_state_value(game, "hero.xp", 22)
    _wait_for_state_value(game, "wallet.gold", 6)
    return {"state": "q002_elder_contract_flow", "quest": "q002_bread_for_post", "viewport": viewport.window_size}


def prepare_combat_prefight(game: Any, viewport: Any) -> dict[str, str]:
    """Open the first combat pre-fight overlay from the Place sheet."""

    _prepare_gate_combat_state(game)
    _open_bottom_nav_slot(game, viewport, 3)
    _clear_dev_world_place_flag(game)
    _open_place_tab(game, viewport, "enemies")
    try:
        _wait_for_node(game, "combat/prefight", max_frames=12, stride=3)
        return {"state": "combat_prefight", "viewport": viewport.window_size}
    except RuntimeError:
        pass
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.caged_scavenger")
    _tap_node_by_bounds(game, tree, node, viewport)
    _wait_for_node(game, "combat/prefight")
    return {"state": "combat_prefight", "viewport": viewport.window_size}


def prepare_combat_result(game: Any, viewport: Any) -> dict[str, str]:
    """Resolve the first combat and stop on the result overlay."""

    prepare_combat_prefight(game, viewport)
    _tap_by_id_bounds(game, viewport, "combat/prefight_start", max_frames=60)
    _wait_for_node(game, "combat/result", max_frames=720, stride=8)
    _wait_for_state_value(game, "hero.xp", 8)
    _wait_for_state_value(game, "wallet.gold", 5)
    _wait_for_state_value(game, "quests.claimed_reward_ids", ["encounter.gate_scavenger.win"])
    _wait_for_stack_count(game, "seeker_token_unlock", 1)
    _wait_for_state_value(
        game,
        "flags.ids",
        [
            "gate_guard_intro_seen",
            "starter_gear_received",
            "old_sword_equipped",
            "padded_jacket_equipped",
            "leather_greaves_equipped",
            "gate_scavenger_defeated",
        ],
    )
    quest = _wait_for_quest_status(game, "q001_gate_pass", "active")
    if quest.get("current_step_id") != "report_to_gate_guard":
        raise RuntimeError(f"q001_gate_pass current step after win is wrong: {quest!r}")
    return {"state": "combat_result", "viewport": viewport.window_size}


def prepare_combat_result_closed(game: Any, viewport: Any) -> dict[str, str]:
    """Resolve the first combat, close the result, and stop on the updated hub UI."""

    prepare_combat_result(game, viewport)
    _tap_by_id(game, viewport, "combat/result_close", max_frames=60)
    _wait_for_state_value(game, "hero.xp", 8)
    _wait_for_state_value(game, "wallet.gold", 5)
    _wait_for_node(game, "first_screen/top_player_cluster")
    return {"state": "combat_result_closed", "viewport": viewport.window_size}


def prepare_combat_loss_result(game: Any, viewport: Any) -> dict[str, str]:
    """Resolve the first combat from low HP and stop on the loss result overlay."""

    _prepare_gate_combat_state(game)
    game.result("game.state.patch", {"doc": "player", "values": {"hero.hp": 1}})
    game.wait_frames(3)
    _open_bottom_nav_slot(game, viewport, 3)
    _clear_dev_world_place_flag(game)
    _open_place_tab(game, viewport, "enemies")
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.caged_scavenger")
    _tap_node_by_bounds(game, tree, node, viewport)
    _wait_for_node(game, "combat/prefight")
    _tap_by_id_bounds(game, viewport, "combat/prefight_start", max_frames=60)
    _wait_for_node(game, "combat/result", max_frames=720, stride=8)
    _wait_for_state_value(game, "hero.hp", 1)
    _wait_for_state_value(game, "hero.xp", 0)
    _wait_for_state_value(game, "wallet.gold", 0)
    _wait_for_state_value(game, "world.current_location_id", "hub_last_post")
    _wait_for_state_value(game, "quests.claimed_reward_ids", [])
    _wait_for_state_value(
        game,
        "quests.completed_step_ids",
        ["talk_gate_guard", "equip_old_sword", "equip_padded_jacket", "equip_leather_greaves"],
    )
    _wait_for_state_value(
        game,
        "flags.ids",
        [
            "gate_guard_intro_seen",
            "starter_gear_received",
            "old_sword_equipped",
            "padded_jacket_equipped",
            "leather_greaves_equipped",
        ],
    )
    quest = _wait_for_quest_status(game, "q001_gate_pass", "active")
    if quest.get("current_step_id") != "clear_gate_scavenger":
        raise RuntimeError(f"q001_gate_pass current step changed after loss: {quest!r}")
    return {"state": "combat_loss_result", "viewport": viewport.window_size}


def prepare_combat_loss_recovered_hub(game: Any, viewport: Any) -> dict[str, str]:
    """Lose the first combat, return to the post, heal, and stop on the hub Place sheet."""

    prepare_combat_loss_result(game, viewport)
    _tap_by_id(game, viewport, "combat/result_close", max_frames=60)
    _open_bottom_nav_slot(game, viewport, 3)
    _open_place_tab(game, viewport, "environment")
    tree, node = _wait_for_node(game, "world_place/object/hub_last_post.healer")
    _tap_node(game, tree, node, viewport)
    _wait_for_state_value(game, "hero.hp", 33)
    _wait_for_state_value(game, "hero.xp", 0)
    _wait_for_state_value(game, "wallet.gold", 0)
    _wait_for_state_value(game, "quests.claimed_reward_ids", [])
    quest = _wait_for_quest_status(game, "q001_gate_pass", "active")
    if quest.get("current_step_id") != "clear_gate_scavenger":
        raise RuntimeError(f"q001_gate_pass current step changed after recovery: {quest!r}")
    _wait_for_node(game, "world_place/object/hub_last_post.healer")
    return {"state": "combat_loss_recovered_hub", "viewport": viewport.window_size}


def prepare_combat_running(game: Any, viewport: Any) -> dict[str, str]:
    """Start the first combat and stop mid-autobattle for visual audit."""

    prepare_combat_prefight(game, viewport)
    _tap_by_id_bounds(game, viewport, "combat/prefight_start", max_frames=60)
    _wait_for_node(game, "combat/running", max_frames=180, stride=4)
    _wait_for_node(game, "combat/stage", max_frames=180, stride=4)
    game.wait_frames(8)
    _wait_for_node(game, "combat/stage", max_frames=1, stride=1)
    return {"state": "combat_running", "viewport": viewport.window_size}


def prepare_mill_combat_result(game: Any, viewport: Any) -> dict[str, str]:
    """Resolve a later multi-reward combat and stop on the result overlay."""

    _prepare_gate_combat_state(game)
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "hero.hp": 48,
                "inventory.gear_instances": {
                    "gear_iron_sword_001": {
                        "def_id": "iron_sword",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_patched_mail_001": {
                        "def_id": "patched_mail",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_iron_greaves_001": {
                        "def_id": "iron_greaves",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                },
                "inventory.bag_order": [
                    "gear_old_sword_001",
                    "gear_padded_jacket_001",
                    "gear_leather_greaves_001",
                    "gear_iron_sword_001",
                    "gear_patched_mail_001",
                    "gear_iron_greaves_001",
                ],
                "equipment.weapon_instance_id": "gear_iron_sword_001",
                "equipment.armour_instance_id": "gear_patched_mail_001",
                "equipment.legs_instance_id": "gear_iron_greaves_001",
                "world.current_location_id": "old_mill",
                "world.visited_location_ids": ["hub_last_post", "hub_gate_outskirts", "old_mill"],
                "quests.tracked_quest_id": "q002_bread_for_post",
                "quests.quest_states": {
                    "q002_bread_for_post": {
                        "status": "active",
                        "current_step_id": "report_to_elder",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_mill_fixture",
                    }
                },
                "quests.completed_step_ids": ["visit_old_mill", "inspect_old_mill"],
                "quests.claimed_reward_ids": [],
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "gate_scavenger_defeated",
                    "seeker_token_owned",
                    "map_gate_unlocked",
                    "old_mill_unlocked",
                    "dev_world_place_open",
                ],
            },
        },
    )
    game.wait_frames(3)
    _open_bottom_nav_slot(game, viewport, 3)
    _clear_dev_world_place_flag(game)
    tree, node = _wait_for_node(game, "world_place/object/old_mill.main_yard")
    _tap_node_by_bounds(game, tree, node, viewport)
    _wait_for_node(game, "combat/prefight")
    _tap_by_id_bounds(game, viewport, "combat/prefight_start", max_frames=60)
    _wait_for_node(game, "combat/result", max_frames=720, stride=8)
    _wait_for_state_value(game, "hero.xp", 10)
    _wait_for_state_value(game, "wallet.gold", 7)
    _wait_for_state_value(game, "quests.claimed_reward_ids", ["encounter.mill_scavenger.win"])
    _wait_for_stack_count(game, "contract_progress", 1)
    _wait_for_gear_def_count(game, "scavenger_knee_plates", 1)
    quest = _wait_for_quest_status(game, "q002_bread_for_post", "active")
    if quest.get("current_step_id") != "report_to_elder":
        raise RuntimeError(f"q002_bread_for_post current step changed after mill win: {quest!r}")
    return {"state": "mill_combat_result", "viewport": viewport.window_size}


def prepare_mill_combat_running(game: Any, viewport: Any) -> dict[str, str]:
    """Start the later mill combat and stop on the animated clash stage."""

    _prepare_gate_combat_state(game)
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "hero.hp": 48,
                "inventory.gear_instances": {
                    "gear_iron_sword_001": {
                        "def_id": "iron_sword",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_patched_mail_001": {
                        "def_id": "patched_mail",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_iron_greaves_001": {
                        "def_id": "iron_greaves",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                },
                "inventory.bag_order": [
                    "gear_old_sword_001",
                    "gear_padded_jacket_001",
                    "gear_leather_greaves_001",
                    "gear_iron_sword_001",
                    "gear_patched_mail_001",
                    "gear_iron_greaves_001",
                ],
                "equipment.weapon_instance_id": "gear_iron_sword_001",
                "equipment.armour_instance_id": "gear_patched_mail_001",
                "equipment.legs_instance_id": "gear_iron_greaves_001",
                "world.current_location_id": "old_mill",
                "world.visited_location_ids": ["hub_last_post", "hub_gate_outskirts", "old_mill"],
                "quests.tracked_quest_id": "q002_bread_for_post",
                "quests.quest_states": {
                    "q002_bread_for_post": {
                        "status": "active",
                        "current_step_id": "report_to_elder",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_mill_fixture",
                    }
                },
                "quests.completed_step_ids": ["visit_old_mill", "inspect_old_mill"],
                "quests.claimed_reward_ids": [],
                "flags.ids": [
                    "gate_guard_intro_seen",
                    "starter_gear_received",
                    "old_sword_equipped",
                    "padded_jacket_equipped",
                    "leather_greaves_equipped",
                    "gate_scavenger_defeated",
                    "seeker_token_owned",
                    "map_gate_unlocked",
                    "old_mill_unlocked",
                    "dev_world_place_open",
                ],
            },
        },
    )
    game.wait_frames(3)
    _open_bottom_nav_slot(game, viewport, 3)
    _clear_dev_world_place_flag(game)
    tree, node = _wait_for_node(game, "world_place/object/old_mill.main_yard")
    _tap_node_by_bounds(game, tree, node, viewport)
    _wait_for_node(game, "combat/prefight")
    _tap_by_id_bounds(game, viewport, "combat/prefight_start", max_frames=60)
    _wait_for_node(game, "combat/running", max_frames=180, stride=4)
    _wait_for_node(game, "combat/stage", max_frames=180, stride=4)
    _wait_for_node(game, "combat/stage", max_frames=1, stride=1)
    return {"state": "mill_combat_running", "viewport": viewport.window_size}


def prepare_mill_combat_result_closed(game: Any, viewport: Any) -> dict[str, str]:
    """Resolve the later multi-reward combat, close the result, and stop on the updated UI."""

    prepare_mill_combat_result(game, viewport)
    _tap_by_id(game, viewport, "combat/result_close", max_frames=60)
    _wait_for_state_value(game, "hero.xp", 10)
    _wait_for_state_value(game, "wallet.gold", 7)
    _wait_for_node(game, "first_screen/top_player_cluster")
    return {"state": "mill_combat_result_closed", "viewport": viewport.window_size}


def _prepare_equipment_fixture(game: Any, viewport: Any, *, item_modal: bool) -> dict[str, str]:
    """Open equipment with starter guard rewards in the backpack."""

    flags = [
        "gate_guard_intro_seen",
        "starter_gear_received",
        "dev_equipment_open",
    ]
    if item_modal:
        flags.append("dev_equipment_item_modal")

    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "inventory.gear_instances": {
                    "gear_old_sword_001": {
                        "def_id": "old_sword",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_padded_jacket_001": {
                        "def_id": "padded_jacket",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                    "gear_leather_greaves_001": {
                        "def_id": "leather_greaves",
                        "durability": 1,
                        "level": 1,
                        "bind_state": "none",
                    },
                },
                "inventory.bag_order": [
                    "gear_old_sword_001",
                    "gear_padded_jacket_001",
                    "gear_leather_greaves_001",
                ],
                "quests.tracked_quest_id": "q001_gate_pass",
                "quests.quest_states": {
                    "q001_gate_pass": {
                        "status": "active",
                        "current_step_id": "equip_old_sword",
                        "objective_progress": 0,
                        "last_update_reason": "runtime_equipment_fixture",
                    }
                },
                "quests.completed_step_ids": ["talk_gate_guard"],
                "flags.ids": flags,
            },
        },
    )
    game.wait_frames(2)
    _ensure_equipment_open(game, viewport)
    _wait_for_node(game, "gear_screen/doll")
    _wait_for_node(game, "gear_screen/backpack_grid")
    _wait_for_node(game, "gear_screen/slot")
    _wait_for_node(game, "gear_screen/bag_cell")
    if item_modal:
        _wait_for_node(game, "gear_screen/item_modal")
        _wait_for_node(game, "gear_screen/equip_button")
    return {"state": "equipment_screen_item_modal" if item_modal else "equipment_screen", "viewport": viewport.window_size}


def prepare_equipment_screen(game: Any, viewport: Any) -> dict[str, str]:
    return _prepare_equipment_fixture(game, viewport, item_modal=True)


def prepare_equipment_screen_grid(game: Any, viewport: Any) -> dict[str, str]:
    return _prepare_equipment_fixture(game, viewport, item_modal=False)
    return {"state": "equipment_screen_item_modal", "viewport": viewport.window_size}


def prepare_equipment_quest_items(game: Any, viewport: Any) -> dict[str, str]:
    """Open the inventory quest-item tab and stop on a quest item detail modal."""

    _prepare_equipment_fixture(game, viewport, item_modal=False)
    game.result(
        "game.state.patch",
        {
            "doc": "player",
            "values": {
                "inventory.stack_instances": {
                    "seeker_token": {"def_id": "seeker_token", "count": 1},
                    "clue_fragment": {"def_id": "clue_fragment", "count": 2},
                }
            },
        },
    )
    game.wait_frames(3)
    _tap_by_id(game, viewport, "gear_screen/tab/quest", max_frames=60)
    _wait_for_node(game, "gear_screen/quest_cell", max_frames=60)
    _tap_by_id(game, viewport, "gear_screen/quest_cell", max_frames=60)
    _wait_for_node(game, "gear_screen/quest_item_modal", max_frames=60)
    return {"state": "equipment_quest_items", "viewport": viewport.window_size}
