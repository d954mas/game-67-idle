#!/usr/bin/env python3
"""Semantic eight-look DevAPI smoke for Runway Awakening.

The scenario proves the player path, not merely that the engine renders:
one authored discovery, an exact replay, all six recipes, and two support remixes.
"""

from __future__ import annotations

import argparse
import contextlib
import json
import os
import sys
from pathlib import Path
from typing import Any


SCRIPT_PATH = Path(__file__).resolve()
GAME_ROOT = SCRIPT_PATH.parents[1]


def find_repo_root(start: Path) -> Path:
    for candidate in (start, *start.parents):
        if (candidate / "ai_studio" / "runtime_automation" / "devapi_client.py").exists():
            return candidate
    raise RuntimeError("could not find repo root with ai_studio/runtime_automation/devapi_client.py")


REPO_ROOT = find_repo_root(GAME_ROOT)
RUNTIME_AUTOMATION = REPO_ROOT / "ai_studio" / "runtime_automation"
if str(RUNTIME_AUTOMATION) not in sys.path:
    sys.path.insert(0, str(RUNTIME_AUTOMATION))

from devapi_client import DEFAULT_DEVAPI_PORT, DevApiError, running_game  # noqa: E402


MOON_MAIN_ID = "dress/item/top_tee"
ACCENT_CATEGORY_ID = "dress/category/4"
BLOOM_ACCENT_ID = "dress/item/acc_hat"
AWAKEN_ID = "awakening/cta"
CARD_ID = "awakening/card"
RESTYLE_ID = "awakening/restyle"
SKIP_REPLAY_ID = "awakening/skip"
TRY_NEXT_ID = "dress/try-next"
LOOKBOOK_OPEN_ID = "lookbook/open"
LOOKBOOK_CLOSE_ID = "lookbook/close"
HAIR_CATEGORY_ID = "dress/category/0"
HAIR_LONG_ID = "dress/item/hair_long"
HAIR_PINK_ID = "dress/item/hair_pink"
BOTTOM_CATEGORY_ID = "dress/category/2"
SHOES_CATEGORY_ID = "dress/category/3"
LOOKBOOK_CREATE_ID = "lookbook/create"
LOOKBOOK_CREATE_ANOTHER_ID = "lookbook/create-another"
LOOKBOOK_WEAR_ID = "lookbook/wear"
MOON_BLOOM_RECIPE_BIT = 1 << 3
MOON_BLOOM_CROWN_LOOK_BIT = 1 << (3 * 3)
RECIPE_IDS = ("moon-moon", "bloom-bloom", "flame-flame", "moon-bloom", "moon-flame", "bloom-flame")

REQUIRED_METHODS = {
    "endpoints",
    "command.describe",
    "frame.wait",
    "frame.current",
    "ui.tree",
    "ui.click",
    "capture.frame",
    "game.state.schema",
    "game.state.get",
    "game.events.tail",
}


def exe_name() -> str:
    return "game.exe" if os.name == "nt" else "game"


def default_executable(game_root: Path = GAME_ROOT) -> Path:
    env_path = os.environ.get("AI_STUDIO_GAME_EXE")
    if env_path:
        return Path(env_path)
    candidates = [
        game_root / "build" / "devapi-debug" / "bin" / exe_name(),
        game_root / "build" / "bin" / exe_name(),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def extract_endpoint_methods(listing: Any) -> set[str]:
    methods: set[str] = set()
    if isinstance(listing, dict) and isinstance(listing.get("commands"), list):
        items = listing["commands"]
    elif isinstance(listing, list):
        items = listing
    else:
        return methods
    for item in items:
        method = item.get("method") if isinstance(item, dict) else item
        if isinstance(method, str):
            methods.add(method)
    return methods


def missing_required_methods(methods: set[str]) -> list[str]:
    return sorted(REQUIRED_METHODS - methods)


def ui_nodes(tree: Any) -> list[dict[str, Any]]:
    if isinstance(tree, dict) and isinstance(tree.get("nodes"), list):
        return [node for node in tree["nodes"] if isinstance(node, dict)]
    return []


def find_ui_node(tree: Any, element_id: str) -> dict[str, Any] | None:
    for node in ui_nodes(tree):
        if node.get("id_string") == element_id or node.get("id") == element_id:
            return node
    return None


def validate_game_state_schema(schema: Any) -> dict[str, Any]:
    if not isinstance(schema, dict):
        raise DevApiError(f"game.state.schema returned {type(schema).__name__}, expected object")
    game = schema.get("game")
    if not isinstance(game, dict):
        raise DevApiError("game.state.schema missing 'game' fragment")
    if game.get("schema") != "runway_awakening.state":
        raise DevApiError(f"unexpected game.state.schema id: {game.get('schema')!r}")
    if game.get("fragment") != "game" and game.get("document") != "game":
        raise DevApiError(f"unexpected game.state.schema fragment: {game.get('fragment')!r}")
    if not isinstance(game.get("fields"), list):
        raise DevApiError("game.state.schema 'game' missing fields array")
    settings = schema.get("settings")
    if not isinstance(settings, dict) or not isinstance(settings.get("fields"), list):
        raise DevApiError("game.state.schema missing 'settings' fragment fields")
    return schema


def validate_game_state(state: Any) -> dict[str, Any]:
    if not isinstance(state, dict):
        raise DevApiError(f"game.state.get returned {type(state).__name__}, expected object")
    if state.get("path") != "":
        raise DevApiError(f"unexpected game.state.get path: {state.get('path')!r}")
    value = state.get("value")
    if not isinstance(value, dict):
        raise DevApiError("game.state.get missing value object")
    game = value.get("game")
    if not isinstance(game, dict):
        raise DevApiError("game.state.get missing value.game fragment")
    for key in ("recipe_mask", "lookbook_mask", "rounds_completed"):
        if not isinstance(game.get(key), int) or isinstance(game.get(key), bool):
            raise DevApiError(f"value.game.{key} is not an integer")
    if not isinstance(game.get("saved_looks"), dict):
        raise DevApiError("value.game.saved_looks is not an exact-look map")
    settings = value.get("settings")
    if not isinstance(settings, dict):
        raise DevApiError("game.state.get missing value.settings fragment")
    return state


def validate_events_tail(tail: Any) -> dict[str, Any]:
    if not isinstance(tail, dict):
        raise DevApiError(f"game.events.tail returned {type(tail).__name__}, expected object")
    if not isinstance(tail.get("events"), list):
        raise DevApiError("game.events.tail missing 'events' array")
    for key in ("next_seq", "dropped", "evicted"):
        if not isinstance(tail.get(key), (int, float)):
            raise DevApiError(f"game.events.tail missing numeric '{key}'")
    if tail["dropped"] != 0 or tail["evicted"] != 0:
        raise DevApiError(
            f"game.events.tail lost evidence: dropped={tail['dropped']} evicted={tail['evicted']}"
        )
    for event in tail["events"]:
        if (
            not isinstance(event, dict)
            or not isinstance(event.get("seq"), (int, float))
            or not isinstance(event.get("type"), str)
        ):
            raise DevApiError("game.events.tail event missing seq/type")
    return tail


def advance_safely(game: Any, frames: int, methods: set[str]) -> Any:
    """Prefer deterministic time.step; fall back to frame.wait when unavailable."""
    if "time.step" in methods:
        try:
            return game.result("time.step", {"count": frames})
        except TimeoutError as exc:
            raise DevApiError(
                f"time.step timed out while advancing {frames} frame(s); native frame progression is blocked"
            ) from exc
    try:
        return game.wait_frames(frames)
    except TimeoutError as exc:
        raise DevApiError(
            f"frame.wait timed out while advancing {frames} frame(s); native frame progression is blocked"
        ) from exc


def wait_for_ui_id(
    game: Any,
    element_id: str,
    *,
    methods: set[str] | None = None,
    max_frames: int = 180,
    stride: int = 3,
) -> dict[str, Any]:
    available = methods or set()
    last_error: Exception | None = None
    last_tree: dict[str, Any] | None = None
    attempts = max(1, max_frames // max(1, stride))
    for _ in range(attempts):
        try:
            tree = game.result("ui.tree")
        except (DevApiError, TimeoutError) as exc:
            last_error = exc
        else:
            last_tree = tree
            if find_ui_node(tree, element_id) is not None:
                return tree
        try:
            advance_safely(game, stride, available)
        except (DevApiError, TimeoutError) as exc:
            last_error = exc
    detail = f"; last error: {last_error}" if last_error else ""
    if last_tree is not None:
        ids = [
            str(node.get("id_string"))
            for node in last_tree.get("nodes", [])
            if isinstance(node, dict) and node.get("id_string")
        ]
        detail += f"; ui ids: {ids}"
    raise DevApiError(f"ui id {element_id!r} did not appear after {max_frames} frames{detail}")


def click_and_advance(game: Any, element_id: str, methods: set[str], frames: int = 2) -> None:
    gate = (
        game.player_gated()
        if "input.set_player_enabled" in methods and hasattr(game, "player_gated")
        else contextlib.nullcontext(game)
    )
    # Keep the gate scoped to the command itself.  Some games intentionally
    # omit interactive Clay nodes while player input is disabled, so discovery
    # stays outside.  The queued DOWN@0 + UP@1 must both drain before the gate
    # reopens or a real pointer can reclaim the capture slot between edges.
    with gate:
        game.click_ui(element_id, wait_frames=0, observe=None)
        advance_safely(game, max(frames, 2), methods)


def click_node_center_and_advance(game: Any, node: dict[str, Any], methods: set[str], frames: int = 2) -> None:
    bounds = node.get("bounds", {})
    x = float(bounds["x"]) + float(bounds["w"]) * 0.5
    y = float(bounds["y"]) + float(bounds["h"]) * 0.5
    gate = (
        game.player_gated()
        if "input.set_player_enabled" in methods and hasattr(game, "player_gated")
        else contextlib.nullcontext(game)
    )
    with gate:
        game.result("ui.click", {"id": {"x": x, "y": y}, "button": "left"})
        advance_safely(game, max(frames, 2), methods)


def assert_moon_bloom_progress(state: dict[str, Any]) -> dict[str, Any]:
    game = state["value"]["game"]
    if (game["recipe_mask"] & MOON_BLOOM_RECIPE_BIT) == 0:
        raise DevApiError(
            f"Moon+Bloom recipe bit was not committed: recipe_mask={game['recipe_mask']}"
        )
    if game["rounds_completed"] != 1:
        raise DevApiError(
            f"expected exactly one completed round, got {game['rounds_completed']}"
        )
    if (game["lookbook_mask"] & MOON_BLOOM_CROWN_LOOK_BIT) == 0:
        raise DevApiError(
            f"Moon+Bloom Crown look was not committed: lookbook_mask={game['lookbook_mask']}"
        )
    return game


def assert_eight_look_progress(state: dict[str, Any]) -> dict[str, Any]:
    game = state["value"]["game"]
    if game["recipe_mask"] != 0x3F:
        raise DevApiError(f"all six recipes were not discovered: recipe_mask={game['recipe_mask']}")
    if game["rounds_completed"] != 8:
        raise DevApiError(f"expected eight unique looks, got {game['rounds_completed']}")
    saved = game["saved_looks"]
    if len(saved) != 8:
        raise DevApiError(f"expected eight exact saved looks, got keys={sorted(saved)}")
    per_recipe = {}
    for key in saved:
        recipe_id, _, slot = key.rpartition("/")
        if not recipe_id or slot not in {"0", "1", "2"}:
            raise DevApiError(f"invalid bounded saved-look key: {key!r}")
        per_recipe[recipe_id] = per_recipe.get(recipe_id, 0) + 1
    if sorted(per_recipe.values()) != [1, 1, 1, 1, 2, 2]:
        raise DevApiError(f"unexpected six-discovery/two-remix distribution: {per_recipe}")
    return game


def confirm_support_decisions(game: Any, methods: set[str], *, hair_id: str | None = None) -> None:
    state = validate_game_state(game.result("game.state.get", {"path": ""}))["value"]["game"]
    chosen_hair = hair_id or f"dress/item/{state['outfit_hair_id']}"
    wait_for_ui_id(game, chosen_hair, methods=methods)
    click_and_advance(game, chosen_hair, methods)
    state = validate_game_state(game.result("game.state.get", {"path": ""}))["value"]["game"]
    bottom_id = f"dress/item/{state['outfit_bottom_id']}"
    wait_for_ui_id(game, bottom_id, methods=methods)
    click_and_advance(game, bottom_id, methods)
    state = validate_game_state(game.result("game.state.get", {"path": ""}))["value"]["game"]
    shoes_id = f"dress/item/{state['outfit_shoes_id']}"
    wait_for_ui_id(game, shoes_id, methods=methods)
    click_and_advance(game, shoes_id, methods)
    wait_for_ui_id(game, AWAKEN_ID, methods=methods)


def prepare_from_lookbook(game: Any, recipe_index: int, methods: set[str]) -> None:
    wait_for_ui_id(game, LOOKBOOK_OPEN_ID, methods=methods)
    click_and_advance(game, f"{LOOKBOOK_OPEN_ID}/control", methods)
    recipe_id = f"lookbook/recipe/{recipe_index}"
    wait_for_ui_id(game, recipe_id, methods=methods)
    click_and_advance(game, f"{recipe_id}/control", methods)
    detail = wait_for_ui_id(game, "lookbook/back", methods=methods)
    action = LOOKBOOK_CREATE_ANOTHER_ID if find_ui_node(detail, LOOKBOOK_CREATE_ANOTHER_ID) else LOOKBOOK_CREATE_ID
    wait_for_ui_id(game, action, methods=methods)
    click_and_advance(game, f"{action}/control", methods)
    confirm_support_decisions(game, methods)


def awaken_to_card(game: Any, methods: set[str]) -> dict[str, Any]:
    click_and_advance(game, f"{AWAKEN_ID}/control", methods, frames=4)
    return wait_for_ui_id(game, CARD_ID, methods=methods, max_frames=420, stride=6)


def run_smoke(game: Any, out_dir: Path, *, audit: bool = True) -> dict[str, Any]:
    out_dir.mkdir(parents=True, exist_ok=True)

    endpoint_listing = game.result("endpoints")
    methods = extract_endpoint_methods(endpoint_listing)
    missing = missing_required_methods(methods)
    if missing:
        raise DevApiError(f"missing required DevAPI methods: {', '.join(missing)}")

    if "time.step" in methods:
        if "time.set_mode" not in methods:
            raise DevApiError("time.step is advertised without required time.set_mode")
        game.result("time.set_mode", {"mode": "manual"})

    # A controlled native loop may not produce another autonomous frame after
    # endpoint discovery. Step it before asking for further immediate metadata.
    advance_safely(game, 1, methods)

    described = {
        method: game.result("command.describe", {"method": method})
        for method in ("ui.click", "capture.frame", "game.state.get", "game.events.tail")
    }
    state_schema = validate_game_state_schema(game.result("game.state.schema"))
    state_before = validate_game_state(game.result("game.state.get", {"path": ""}))
    events_before = validate_events_tail(game.result("game.events.tail", {}))

    wait_for_ui_id(game, MOON_MAIN_ID, methods=methods)
    click_and_advance(game, MOON_MAIN_ID, methods)

    wait_for_ui_id(game, ACCENT_CATEGORY_ID, methods=methods)
    click_and_advance(game, ACCENT_CATEGORY_ID, methods)
    wait_for_ui_id(game, BLOOM_ACCENT_ID, methods=methods)
    click_and_advance(game, BLOOM_ACCENT_ID, methods)

    focus_state = validate_game_state(game.result("game.state.get", {"path": ""}))
    focus_game = focus_state["value"]["game"]
    if focus_game.get("outfit_main_id") != "top_tee" or focus_game.get("outfit_accent_id") != "acc_hat":
        raise DevApiError(f"focus clicks did not equip Moon+Bloom: {focus_game}")

    confirm_support_decisions(game, methods)
    awaken_tree = wait_for_ui_id(game, AWAKEN_ID, methods=methods)
    awaken_node = find_ui_node(awaken_tree, AWAKEN_ID)
    if awaken_node is None:
        raise DevApiError("awakening CTA disappeared before click")
    click_and_advance(game, f"{AWAKEN_ID}/control", methods, frames=4)
    card_tree = wait_for_ui_id(game, CARD_ID, methods=methods, max_frames=420, stride=6)

    state_after_card = validate_game_state(game.result("game.state.get", {"path": ""}))
    progress = assert_moon_bloom_progress(state_after_card)

    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "run"})
    recipe_card_screenshot = game.capture_screenshot(
        str(out_dir / "moon_bloom_recipe_card.png"), wait_frames=2, audit=audit
    )
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "manual"})

    wait_for_ui_id(game, RESTYLE_ID, methods=methods)
    click_and_advance(game, RESTYLE_ID, methods)
    second_round_tree = wait_for_ui_id(game, AWAKEN_ID, methods=methods)

    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "run"})
    screenshot = game.capture_screenshot(
        str(out_dir / "round_two_restyle.png"), wait_frames=2, audit=audit
    )
    state_after_restyle = validate_game_state(game.result("game.state.get", {"path": ""}))
    assert_moon_bloom_progress(state_after_restyle)
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "manual"})

    # The persisted collection must be visible and navigable, not only a state bit.
    wait_for_ui_id(game, LOOKBOOK_OPEN_ID, methods=methods)
    click_and_advance(game, f"{LOOKBOOK_OPEN_ID}/control", methods)
    lookbook_tree = wait_for_ui_id(game, "lookbook/recipe/3", methods=methods)
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "run"})
    lookbook_screenshot = game.capture_screenshot(
        str(out_dir / "lookbook_after_first_magic.png"), wait_frames=2, audit=audit
    )
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "manual"})
    click_and_advance(game, f"{LOOKBOOK_CLOSE_ID}/control", methods)

    # Replay the exact recipe: it must be labeled KNOWN, preserve collection
    # cardinality, increment analytics rounds, and expose the replay skip.
    wait_for_ui_id(game, MOON_MAIN_ID, methods=methods)
    click_and_advance(game, MOON_MAIN_ID, methods)
    wait_for_ui_id(game, ACCENT_CATEGORY_ID, methods=methods)
    click_and_advance(game, ACCENT_CATEGORY_ID, methods)
    wait_for_ui_id(game, BLOOM_ACCENT_ID, methods=methods)
    click_and_advance(game, BLOOM_ACCENT_ID, methods)
    confirm_support_decisions(game, methods)
    known_preflight_tree = wait_for_ui_id(game, TRY_NEXT_ID, methods=methods)
    if find_ui_node(known_preflight_tree, AWAKEN_ID) is None:
        raise DevApiError("known recipe lost its explicit replay action")
    click_and_advance(game, f"{AWAKEN_ID}/control", methods)
    wait_for_ui_id(game, SKIP_REPLAY_ID, methods=methods)
    click_and_advance(game, f"{SKIP_REPLAY_ID}/control", methods)
    repeat_card_tree = wait_for_ui_id(game, CARD_ID, methods=methods)
    repeat_text = " ".join(
        f"{node.get('text', '')} {node.get('label', '')}" for node in ui_nodes(repeat_card_tree)
    ).upper()
    if "KNOWN LOOK" not in repeat_text or "NEW MAGIC" in repeat_text or "NEW REMIX" in repeat_text:
        raise DevApiError(f"repeat card has false discovery copy: {repeat_text}")
    state_after_repeat = validate_game_state(game.result("game.state.get", {"path": ""}))
    repeat_game = state_after_repeat["value"]["game"]
    if (repeat_game["recipe_mask"] != MOON_BLOOM_RECIPE_BIT or
            repeat_game["rounds_completed"] != 1 or
            repeat_game["lookbook_mask"] != MOON_BLOOM_CROWN_LOOK_BIT):
        raise DevApiError(f"repeat progress contract failed: {repeat_game}")
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "run"})
    repeat_card_screenshot = game.capture_screenshot(
        str(out_dir / "moon_bloom_repeat_known.png"), wait_frames=2, audit=audit
    )
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "manual"})

    # Complete the ten-minute content spine: the remaining five forms, then two
    # visibly player-authored support remixes using different hair signatures.
    collected_recipe_order = [3]
    for recipe_index in (0, 1, 2, 4, 5):
        click_and_advance(game, RESTYLE_ID, methods)
        prepare_from_lookbook(game, recipe_index, methods)
        awaken_to_card(game, methods)
        collected_recipe_order.append(recipe_index)

    for recipe_index, hair_id in ((0, HAIR_LONG_ID), (1, HAIR_PINK_ID)):
        click_and_advance(game, RESTYLE_ID, methods)
        wait_for_ui_id(game, HAIR_CATEGORY_ID, methods=methods)
        click_and_advance(game, HAIR_CATEGORY_ID, methods)
        wait_for_ui_id(game, hair_id, methods=methods)
        click_and_advance(game, hair_id, methods)
        prepare_from_lookbook(game, recipe_index, methods)
        awaken_to_card(game, methods)

    completion_state = validate_game_state(game.result("game.state.get", {"path": ""}))
    completion_game = assert_eight_look_progress(completion_state)
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "run"})
    mastery_screenshot = game.capture_screenshot(
        str(out_dir / "eight_look_mastery.png"), wait_frames=2, audit=audit
    )
    events_after = validate_events_tail(game.result("game.events.tail", {}))
    event_types = [event["type"] for event in events_after["events"]]

    # Prove the detail card exposes authorship and restores all five exact ids.
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "manual"})
    click_and_advance(game, RESTYLE_ID, methods)
    click_and_advance(game, f"{LOOKBOOK_OPEN_ID}/control", methods)
    click_and_advance(game, "lookbook/recipe/3/control", methods)
    detail_tree = wait_for_ui_id(game, LOOKBOOK_WEAR_ID, methods=methods)
    detail_text = " ".join(
        f"{node.get('text', '')} {node.get('label', '')}" for node in ui_nodes(detail_tree)
    ).upper()
    for field in ("HAIR:", "MAIN:", "BOTTOM:", "SHOES:", "ACCENT:"):
        if field not in detail_text:
            raise DevApiError(f"Lookbook detail hides exact outfit authorship: missing {field}")
    click_and_advance(game, f"{LOOKBOOK_WEAR_ID}/control", methods)
    worn_state = validate_game_state(game.result("game.state.get", {"path": ""}))
    worn_game = worn_state["value"]["game"]
    saved_first = completion_game["saved_looks"]["moon-bloom/0"]
    persisted_fields = {
        "outfit_hair_id": "hair_id", "outfit_main_id": "main_id",
        "outfit_bottom_id": "bottom_id", "outfit_shoes_id": "shoes_id",
        "outfit_accent_id": "accent_id",
    }
    for outfit_field, saved_field in persisted_fields.items():
        if worn_game[outfit_field] != saved_first[saved_field]:
            raise DevApiError(f"WEAR THIS LOOK failed for {outfit_field}: {worn_game}")

    reload_state = None
    if {"game.state.save", "game.state.load"}.issubset(methods):
        game.result("game.state.save")
        reload_state = game.result("game.state.load")
        after_reload = validate_game_state(game.result("game.state.get", {"path": ""}))
        assert_eight_look_progress(after_reload)
    if "time.step" in methods:
        game.result("time.set_mode", {"mode": "run"})

    summary = {
        "schema": "runway_awakening.devapi_smoke.v2",
        "method_count": len(methods),
        "required_methods": sorted(REQUIRED_METHODS),
        "described_methods": sorted(described),
        "recipe": "moon-bloom",
        "recipe_bit": MOON_BLOOM_RECIPE_BIT,
        "rounds_completed": progress["rounds_completed"],
        "flow": [MOON_MAIN_ID, ACCENT_CATEGORY_ID, BLOOM_ACCENT_ID, AWAKEN_ID, CARD_ID, RESTYLE_ID],
        "recipe_card_nodes": len(ui_nodes(card_tree)),
        "second_round_ui": AWAKEN_ID,
        "second_round_nodes": len(ui_nodes(second_round_tree)),
        "game_state_schema": state_schema,
        "state_before": state_before,
        "state_after_card": state_after_card,
        "recipe_card_screenshot": recipe_card_screenshot,
        "state_after_restyle": state_after_restyle,
        "state_after_repeat": state_after_repeat,
        "repeat_card_screenshot": repeat_card_screenshot,
        "repeat_copy": "KNOWN LOOK",
        "known_preflight_action": TRY_NEXT_ID,
        "lookbook_nodes": len(ui_nodes(lookbook_tree)),
        "lookbook_screenshot": lookbook_screenshot,
        "events_before": events_before,
        "events_after": events_after,
        "event_types": event_types,
        "collected_recipe_order": collected_recipe_order,
        "completion_state": completion_state,
        "completion_recipe_count": int(completion_game["recipe_mask"]).bit_count(),
        "completion_look_count": int(completion_game["lookbook_mask"]).bit_count(),
        "completion_saved_look_count": len(completion_game["saved_looks"]),
        "completion_rounds": completion_game["rounds_completed"],
        "mastery_screenshot": mastery_screenshot,
        "lookbook_detail_nodes": len(ui_nodes(detail_tree)),
        "worn_state": worn_state,
        "reload_state": reload_state,
        "screenshot": screenshot,
    }
    summary_path = out_dir / "summary.json"
    summary["summary"] = str(summary_path)
    with summary_path.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2)
        handle.write("\n")
    return summary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run the Runway Awakening semantic DevAPI smoke.")
    parser.add_argument("--exe", type=Path, default=default_executable(), help="Native debug executable to launch.")
    parser.add_argument("--port", type=int, default=DEFAULT_DEVAPI_PORT, help="DevAPI TCP port.")
    parser.add_argument("--reuse", action="store_true", help="Attach to an existing DevAPI process when available.")
    parser.add_argument("--window-size", default="640x960", help="Window size passed to the game.")
    parser.add_argument(
        "--out",
        type=Path,
        default=REPO_ROOT / "tmp" / "web_dressup_devapi_smoke",
        help="Evidence output directory.",
    )
    parser.add_argument("--no-audit", action="store_true", help="Skip pixel-health audit for the screenshot.")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    exe = args.exe.resolve()
    if not args.reuse and not exe.exists():
        print(f"build native Debug first or pass --exe: {exe}", file=sys.stderr)
        return 2

    try:
        with running_game(
            port=args.port,
            exe=str(exe),
            cwd=str(REPO_ROOT),
            reuse_existing=args.reuse,
            fresh_state=True,
            autosave_enabled=False,
            window_size=args.window_size,
        ) as game:
            summary = run_smoke(game, args.out, audit=not args.no_audit)
    except (DevApiError, TimeoutError) as exc:
        print(f"devapi smoke failed: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
