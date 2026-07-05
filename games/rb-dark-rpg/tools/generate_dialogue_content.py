#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path


def c_ident(value: str) -> str:
    ident = re.sub(r"[^A-Za-z0-9_]", "_", value).upper()
    if not ident or ident[0].isdigit():
        ident = f"_{ident}"
    return ident


def c_str(value) -> str:
    if value is None:
        return "NULL"
    text = str(value)
    escaped = (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def c_bool(value) -> str:
    return "true" if bool(value) else "false"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_asset_region_hashes(path: Path) -> dict[str, str]:
    if not path.exists():
        raise FileNotFoundError(f"asset header not found: {path}")
    hashes: dict[str, str] = {}
    pattern = re.compile(
        r"#define\s+ASSET_ATLAS_REGION_[A-Z0-9_]+\s+\(\(nt_hash64_t\)\{0x([0-9A-Fa-f]+)ULL\}\)\s+/\*\s+([^*]+?)\s+\*/"
    )
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            hashes[match.group(2)] = f"0x{match.group(1).upper()}ULL"
    return hashes


def asset_region_hash_literal(region_name: str | None, hashes: dict[str, str]) -> str:
    if not region_name:
        return "0ULL"
    value = hashes.get(region_name)
    if value is None:
        raise ValueError(f"scene sprite region {region_name!r} is not present in generated game_assets.h")
    return value


def find_quest_id(dialogue: dict) -> str | None:
    for node in dialogue.get("nodes", []):
        for choice in node.get("choices", []):
            for effect in choice.get("effects", []):
                if effect.get("type") == "advance_quest" and effect.get("quest_id"):
                    return effect["quest_id"]
    return None


def reward_name(effect: dict, items: dict[str, dict]) -> str:
    typ = effect.get("type")
    if typ == "item":
        item = items.get(effect.get("item_id", ""), {})
        return item.get("display_name") or effect.get("item_id", "")
    if typ == "grant_xp":
        return "Опыт"
    if typ == "grant_gold":
        return "Золото"
    if typ == "unlock_screen":
        return "Новый путь"
    return effect.get("type", "Награда")


def reward_icon(effect: dict, items: dict[str, dict]) -> str:
    typ = effect.get("type")
    if typ == "grant_xp":
        return "XP"
    if typ == "grant_gold":
        return "G"
    if typ == "unlock_screen":
        return ">>"
    name = reward_name(effect, items)
    return name[:2].upper() if name else "?"


def reward_icon_asset_id(effect: dict, items: dict[str, dict]) -> str | None:
    if effect.get("type") != "item":
        return None
    item = items.get(effect.get("item_id", ""), {})
    return item.get("icon_asset_id")


def reward_kind(effect: dict) -> str:
    typ = effect.get("type")
    if typ == "grant_xp":
        return "DIALOGUE_REWARD_XP"
    if typ == "grant_gold":
        return "DIALOGUE_REWARD_GOLD"
    if typ == "unlock_screen":
        return "DIALOGUE_REWARD_UNLOCK"
    return "DIALOGUE_REWARD_ITEM"


def reward_amount(effect: dict) -> int:
    if "count" in effect:
        return int(effect["count"])
    if "amount" in effect:
        return int(effect["amount"])
    return 1


def item_kind(value: str | None) -> str:
    if value == "gear":
        return "GAME_ITEM_KIND_GEAR"
    if value == "quest_item":
        return "GAME_ITEM_KIND_QUEST_ITEM"
    if value == "clue":
        return "GAME_ITEM_KIND_CLUE"
    if value == "consumable":
        return "GAME_ITEM_KIND_CONSUMABLE"
    if value == "material":
        return "GAME_ITEM_KIND_MATERIAL"
    return "GAME_ITEM_KIND_UNKNOWN"


def item_category_label(item: dict) -> str:
    explicit = item.get("category_label")
    if explicit:
        return explicit
    kind = item.get("kind")
    if kind == "gear":
        return "Снаряжение"
    if kind == "quest_item":
        return "Квестовый предмет"
    if kind == "clue":
        return "Улика"
    if kind == "consumable":
        return "Расходник"
    if kind == "material":
        return "Материал"
    return "Предмет"


def scene_bounds(scene: dict) -> dict:
    bounds = scene.get("bounds") or {}
    return {
        "x": int(bounds.get("x", 0)),
        "y": int(bounds.get("y", 0)),
        "w": int(bounds.get("w", 0)),
        "h": int(bounds.get("h", 0)),
    }


def item_slot(value: str | None) -> str:
    if value == "weapon":
        return "GAME_ITEM_SLOT_WEAPON"
    if value == "offhand":
        return "GAME_ITEM_SLOT_OFFHAND"
    if value == "head":
        return "GAME_ITEM_SLOT_HEAD"
    if value == "armour":
        return "GAME_ITEM_SLOT_ARMOUR"
    if value == "hands":
        return "GAME_ITEM_SLOT_HANDS"
    if value == "waist":
        return "GAME_ITEM_SLOT_WAIST"
    if value == "legs":
        return "GAME_ITEM_SLOT_LEGS"
    if value == "feet":
        return "GAME_ITEM_SLOT_FEET"
    if value == "neck":
        return "GAME_ITEM_SLOT_NECK"
    if value == "ring_left":
        return "GAME_ITEM_SLOT_RING_LEFT"
    if value == "ring_right":
        return "GAME_ITEM_SLOT_RING_RIGHT"
    if value == "relic":
        return "GAME_ITEM_SLOT_RELIC"
    return "GAME_ITEM_SLOT_NONE"


def stat_init(stats: dict | None) -> str:
    stats = stats or {}
    attack_interval = stats.get("attack_interval", 0)
    return (
        "{"
        f".vitality = {int(stats.get('vitality', 0))}, "
        f".strength = {int(stats.get('strength', 0))}, "
        f".protection = {int(stats.get('protection', 0))}, "
        f".intuition = {int(stats.get('intuition', 0))}, "
        f".weapon_damage = {int(stats.get('weapon_damage', 0))}, "
        f".bonus_attack_power = {int(stats.get('bonus_attack_power', 0))}, "
        f".attack_interval = {float(attack_interval):.3f}F"
        "}"
    )


def location_unlock_kind(value: str | None) -> str:
    if value == "flag":
        return "GAME_LOCATION_UNLOCK_FLAG"
    if value == "quest_step":
        return "GAME_LOCATION_UNLOCK_QUEST_STEP"
    return "GAME_LOCATION_UNLOCK_ALWAYS"


def location_requirement_kind(value: str | None) -> str:
    if value == "flag":
        return "GAME_LOCATION_REQUIREMENT_FLAG"
    if value == "equipped":
        return "GAME_LOCATION_REQUIREMENT_EQUIPPED"
    if value == "quest_active":
        return "GAME_LOCATION_REQUIREMENT_QUEST_ACTIVE"
    if value == "quest_status":
        return "GAME_LOCATION_REQUIREMENT_QUEST_STATUS"
    if value == "quest_step":
        return "GAME_LOCATION_REQUIREMENT_QUEST_STEP"
    return "GAME_LOCATION_REQUIREMENT_NONE"


def location_requirement_id(req: dict) -> str | None:
    typ = req.get("type")
    if typ == "flag":
        return req.get("flag_id")
    if typ == "equipped":
        return req.get("item_id")
    if typ == "quest_active":
        return req.get("quest_id")
    if typ == "quest_status":
        return req.get("quest_id")
    if typ == "quest_step":
        return req.get("quest_id")
    return None


def service_requirement_kind(value: str | None) -> str:
    if value == "flag":
        return "GAME_SERVICE_REQUIREMENT_FLAG"
    if value == "quest_completed":
        return "GAME_SERVICE_REQUIREMENT_QUEST_COMPLETED"
    if value == "quest_status":
        return "GAME_SERVICE_REQUIREMENT_QUEST_STATUS"
    if value == "quest_step":
        return "GAME_SERVICE_REQUIREMENT_QUEST_STEP"
    return "GAME_SERVICE_REQUIREMENT_NONE"


def service_requirement_id(req: dict) -> str | None:
    typ = req.get("type")
    if typ == "flag":
        return req.get("flag_id")
    if typ in {"quest_completed", "quest_status", "quest_step"}:
        return req.get("quest_id")
    return None


def emit_location_requirements(lines: list[str], symbol: str, reqs: list[dict]) -> None:
    if not reqs:
        return
    lines.append(f"static const game_location_requirement_t {symbol}[] = {{")
    for req in reqs:
        lines.append("    {")
        lines.append(f"        .kind = {location_requirement_kind(req.get('type'))},")
        lines.append(f"        .id = {c_str(location_requirement_id(req))},")
        lines.append(f"        .step_id = {c_str(req.get('step_id'))},")
        lines.append(f"        .status = {c_str(req.get('status'))},")
        lines.append(f"        .value = {'true' if req.get('value', True) else 'false'},")
        lines.append("    },")
    lines.append("};")
    lines.append("")


def emit_service_requirements(lines: list[str], symbol: str, reqs: list[dict]) -> None:
    if not reqs:
        return
    lines.append(f"static const game_service_requirement_t {symbol}[] = {{")
    for req in reqs:
        lines.append("    {")
        lines.append(f"        .kind = {service_requirement_kind(req.get('type'))},")
        lines.append(f"        .id = {c_str(service_requirement_id(req))},")
        lines.append(f"        .step_id = {c_str(req.get('step_id'))},")
        lines.append(f"        .status = {c_str(req.get('status'))},")
        lines.append(f"        .value = {'true' if req.get('value', True) else 'false'},")
        lines.append("    },")
    lines.append("};")
    lines.append("")


def interaction_object_id(interaction: dict) -> str | None:
    typ = interaction.get("type")
    if typ == "inspect":
        return interaction.get("object_id")
    return None


def effect_kind(value: str | None) -> str:
    if value == "grant_item":
        return "DIALOGUE_EFFECT_GRANT_ITEM"
    if value == "advance_quest":
        return "DIALOGUE_EFFECT_ADVANCE_QUEST"
    if value == "set_flag":
        return "DIALOGUE_EFFECT_SET_FLAG"
    if value == "grant_xp":
        return "DIALOGUE_EFFECT_GRANT_XP"
    if value == "grant_gold":
        return "DIALOGUE_EFFECT_GRANT_GOLD"
    if value == "complete_quest":
        return "DIALOGUE_EFFECT_COMPLETE_QUEST"
    return "DIALOGUE_EFFECT_ADVANCE_QUEST"


def emit_effects(lines: list[str], symbol: str, effects: list[dict]) -> None:
    if not effects:
        return
    lines.append(f"static const dialogue_effect_t {symbol}[] = {{")
    for effect in effects:
        lines.append("    {")
        lines.append(f"        .kind = {effect_kind(effect.get('type'))},")
        lines.append(f"        .item_id = {c_str(effect.get('item_id'))},")
        lines.append(f"        .count = {reward_amount(effect)},")
        lines.append(f"        .quest_id = {c_str(effect.get('quest_id'))},")
        lines.append(f"        .step_id = {c_str(effect.get('step_id'))},")
        lines.append(f"        .flag_id = {c_str(effect.get('flag_id'))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")


def emit_rewards(lines: list[str], symbol: str, rewards: list[dict], items: dict[str, dict]) -> None:
    if not rewards:
        return
    lines.append(f"static const dialogue_reward_t {symbol}[] = {{")
    for effect in rewards:
        rid = effect.get("item_id") or effect.get("screen_id") or effect.get("type", "reward")
        name = reward_name(effect, items)
        summary = name
        detail = name
        lines.append("    {")
        lines.append(f"        .id = {c_str(rid)},")
        lines.append(f"        .name = {c_str(name)},")
        lines.append(f"        .icon_label = {c_str(reward_icon(effect, items))},")
        lines.append(f"        .icon_asset_id = {c_str(reward_icon_asset_id(effect, items))},")
        lines.append(f"        .summary = {c_str(summary)},")
        lines.append(f"        .detail = {c_str(detail)},")
        lines.append(f"        .kind = {reward_kind(effect)},")
        lines.append(f"        .amount = {reward_amount(effect)},")
        lines.append("    },")
    lines.append("};")
    lines.append("")


def main() -> int:
    if len(sys.argv) != 9:
        print(
            "usage: generate_dialogue_content.py <dialogues.json> <characters.json> <quests.json> <items.json> <combat.json> <locations.json> <services.json> <out.c>",
            file=sys.stderr,
        )
        return 2

    dialogues_path = Path(sys.argv[1])
    characters_path = Path(sys.argv[2])
    quests_path = Path(sys.argv[3])
    items_path = Path(sys.argv[4])
    combat_path = Path(sys.argv[5])
    locations_path = Path(sys.argv[6])
    services_path = Path(sys.argv[7])
    out_path = Path(sys.argv[8])
    game_dir = locations_path.parent.parent.parent
    asset_region_hashes = load_asset_region_hashes(game_dir / "src" / "generated" / "game_assets.h")

    dialogues = load_json(dialogues_path)["dialogues"]
    characters = {c["id"]: c for c in load_json(characters_path)["characters"]}
    quests = {q["id"]: q for q in load_json(quests_path)["quests"]}
    items_doc = load_json(items_path)
    items = {i["id"]: i for i in items_doc["items"]}
    equipment_slots = sorted(items_doc.get("equipment_slots", []), key=lambda slot: int(slot.get("ui_order", 9999)))
    combat_doc = load_json(combat_path)
    base_player_stats = combat_doc.get("player_character", {}).get("base", {})
    encounters = combat_doc.get("encounters", [])
    locations = load_json(locations_path).get("locations", [])
    services_doc = load_json(services_path)
    shops = services_doc.get("shops", [])

    lines: list[str] = [
        "#include \"game_content.h\"",
        "",
        "#include <stddef.h>",
        "#include <string.h>",
        "",
        "/* Generated from games/rb-dark-rpg/design/data JSON files. Do not edit by hand. */",
        "",
    ]
    dialogue_symbols: list[tuple[str, str]] = []

    lines.append("static const game_item_definition_t ITEMS[] = {")
    for item in items.values():
        lines.append("    {")
        lines.append(f"        .id = {c_str(item.get('id'))},")
        lines.append(f"        .display_name = {c_str(item.get('display_name'))},")
        lines.append(f"        .category_label = {c_str(item_category_label(item))},")
        lines.append(f"        .description = {c_str(item.get('description'))},")
        lines.append(f"        .icon_asset_id = {c_str(item.get('icon_asset_id'))},")
        lines.append(f"        .kind = {item_kind(item.get('kind'))},")
        lines.append(f"        .slot = {item_slot(item.get('slot'))},")
        lines.append(f"        .stackable = {'true' if item.get('stackable') else 'false'},")
        lines.append(f"        .max_stack = {int(item.get('max_stack', 1))},")
        lines.append(f"        .price_gold = {int(item.get('price_gold', 0))},")
        lines.append(f"        .sellable = {'true' if item.get('sellable') else 'false'},")
        lines.append(f"        .stats = {stat_init(item.get('stats'))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    lines.append(f"static const game_combat_stats_t BASE_PLAYER_STATS = {stat_init(base_player_stats)};")
    lines.append("")

    encounter_steps: list[dict] = []
    for quest in quests.values():
        for step in quest.get("steps", []):
            objective = step.get("objective") or {}
            if objective.get("type") != "win_encounter":
                continue
            on_complete = step.get("on_complete") or {}
            flags = on_complete.get("set_flags") or []
            unlocks = on_complete.get("unlock_encounters") or []
            encounter_steps.append(
                {
                    "quest_id": quest.get("id"),
                    "step_id": step.get("id"),
                    "encounter_id": objective.get("encounter_id"),
                    "complete_flag_id": flags[0] if flags else None,
                    "unlock_id": unlocks[0] if unlocks else None,
                }
            )

    lines.append("static const game_encounter_definition_t ENCOUNTERS[] = {")
    for encounter in encounters:
        reward_items = list(encounter.get("reward_items", []))[:4]
        lines.append("    {")
        lines.append(f"        .id = {c_str(encounter.get('id'))},")
        lines.append(f"        .display_name = {c_str(encounter.get('display_name'))},")
        lines.append(f"        .enemy = {stat_init(encounter)},")
        lines.append(f"        .reward_xp = {int(encounter.get('reward_xp', 0))},")
        lines.append(f"        .reward_gold = {int(encounter.get('reward_gold', 0))},")
        if reward_items:
            for index, item_id in enumerate(reward_items):
                lines.append(f"        .reward_items[{index}] = {c_str(item_id)},")
        lines.append(f"        .reward_item_count = {len(reward_items)},")
        lines.append(f"        .expected_threat = {c_str(encounter.get('expected_threat_with_starter_loadout'))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    lines.append("static const game_quest_encounter_step_t QUEST_ENCOUNTER_STEPS[] = {")
    for step in encounter_steps:
        lines.append("    {")
        lines.append(f"        .quest_id = {c_str(step.get('quest_id'))},")
        lines.append(f"        .step_id = {c_str(step.get('step_id'))},")
        lines.append(f"        .encounter_id = {c_str(step.get('encounter_id'))},")
        lines.append(f"        .complete_flag_id = {c_str(step.get('complete_flag_id'))},")
        lines.append(f"        .unlock_id = {c_str(step.get('unlock_id'))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    lines.append("static const game_equipment_slot_definition_t EQUIPMENT_SLOTS[] = {")
    for slot in equipment_slots:
        lines.append("    {")
        lines.append(f"        .id = {c_str(slot.get('id'))},")
        lines.append(f"        .display_name = {c_str(slot.get('display_name'))},")
        lines.append(f"        .slot = {item_slot(slot.get('id'))},")
        lines.append(f"        .mvp = {'true' if slot.get('mvp') else 'false'},")
        lines.append(f"        .ui_order = {int(slot.get('ui_order', 0))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    shop_item_symbols: dict[str, str] = {}
    for shop in shops:
        shop_symbol = c_ident(shop.get("id", "shop"))
        inventory = shop.get("inventory") or []
        for index, entry in enumerate(inventory):
            reqs = entry.get("requirements") or []
            req_symbol = f"SHOP_{shop_symbol}_ITEM_{index}_REQS"
            emit_service_requirements(lines, req_symbol, reqs)
        item_symbol = f"SHOP_{shop_symbol}_ITEMS"
        shop_item_symbols[shop.get("id")] = item_symbol
        lines.append(f"static const game_shop_item_t {item_symbol}[] = {{")
        for index, entry in enumerate(inventory):
            reqs = entry.get("requirements") or []
            req_symbol = f"SHOP_{shop_symbol}_ITEM_{index}_REQS"
            lines.append("    {")
            lines.append(f"        .item_id = {c_str(entry.get('item_id'))},")
            lines.append(f"        .price_gold = {int(entry.get('price_gold', 0))},")
            lines.append(f"        .requirements = {req_symbol if reqs else 'NULL'},")
            lines.append(f"        .requirement_count = {len(reqs)},")
            lines.append("    },")
        lines.append("};")
        lines.append("")

    lines.append("static const game_shop_definition_t SHOPS[] = {")
    for shop in shops:
        item_symbol = shop_item_symbols.get(shop.get("id"))
        inventory = shop.get("inventory") or []
        lines.append("    {")
        lines.append(f"        .id = {c_str(shop.get('id'))},")
        lines.append(f"        .display_name = {c_str(shop.get('display_name'))},")
        lines.append(f"        .keeper_character_id = {c_str(shop.get('keeper_character_id'))},")
        lines.append(f"        .location_id = {c_str(shop.get('location_id'))},")
        lines.append(f"        .items = {item_symbol if item_symbol else 'NULL'},")
        lines.append(f"        .item_count = {len(inventory)},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    equip_steps: list[dict] = []
    for quest in quests.values():
        for step in quest.get("steps", []):
            objective = step.get("objective") or {}
            if objective.get("type") != "equip_item":
                continue
            on_complete = step.get("on_complete") or {}
            flags = on_complete.get("set_flags") or []
            unlocks = on_complete.get("unlock_encounters") or []
            equip_steps.append(
                {
                    "quest_id": quest.get("id"),
                    "step_id": step.get("id"),
                    "item_id": objective.get("item_id"),
                    "complete_flag_id": flags[0] if flags else None,
                    "unlock_id": unlocks[0] if unlocks else None,
                }
            )

    lines.append("static const game_quest_equip_step_t QUEST_EQUIP_STEPS[] = {")
    for step in equip_steps:
        lines.append("    {")
        lines.append(f"        .quest_id = {c_str(step.get('quest_id'))},")
        lines.append(f"        .step_id = {c_str(step.get('step_id'))},")
        lines.append(f"        .item_id = {c_str(step.get('item_id'))},")
        lines.append(f"        .complete_flag_id = {c_str(step.get('complete_flag_id'))},")
        lines.append(f"        .unlock_id = {c_str(step.get('unlock_id'))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    visit_steps: list[dict] = []
    for quest in quests.values():
        for step in quest.get("steps", []):
            objective = step.get("objective") or {}
            if objective.get("type") != "visit_location":
                continue
            visit_steps.append(
                {
                    "quest_id": quest.get("id"),
                    "step_id": step.get("id"),
                    "location_id": objective.get("location_id"),
                }
            )

    lines.append("static const game_quest_visit_step_t QUEST_VISIT_STEPS[] = {")
    for step in visit_steps:
        lines.append("    {")
        lines.append(f"        .quest_id = {c_str(step.get('quest_id'))},")
        lines.append(f"        .step_id = {c_str(step.get('step_id'))},")
        lines.append(f"        .location_id = {c_str(step.get('location_id'))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    inspect_steps: list[dict] = []
    for quest in quests.values():
        for step in quest.get("steps", []):
            objective = step.get("objective") or {}
            if objective.get("type") != "inspect_object":
                continue
            inspect_steps.append(
                {
                    "quest_id": quest.get("id"),
                    "step_id": step.get("id"),
                    "object_id": objective.get("target_id"),
                }
            )

    lines.append("static const game_quest_inspect_step_t QUEST_INSPECT_STEPS[] = {")
    for step in inspect_steps:
        lines.append("    {")
        lines.append(f"        .quest_id = {c_str(step.get('quest_id'))},")
        lines.append(f"        .step_id = {c_str(step.get('step_id'))},")
        lines.append(f"        .object_id = {c_str(step.get('object_id'))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    location_exit_symbols: dict[str, str] = {}
    for location in locations:
        loc_symbol = c_ident(location.get("id", "location"))
        for index, exit_def in enumerate(location.get("exits", [])):
            reqs = exit_def.get("requirements") or []
            req_symbol = f"LOCATION_{loc_symbol}_EXIT_{index}_REQS"
            emit_location_requirements(lines, req_symbol, reqs)
            exit_symbol = f"LOCATION_{loc_symbol}_EXITS"
            location_exit_symbols[location.get("id")] = exit_symbol

    location_object_symbols: dict[str, str] = {}
    for location in locations:
        loc_symbol = c_ident(location.get("id", "location"))
        objects = location.get("objects") or []
        for index, obj in enumerate(objects):
            reqs = obj.get("requirements") or []
            req_symbol = f"LOCATION_{loc_symbol}_OBJECT_{index}_REQS"
            emit_location_requirements(lines, req_symbol, reqs)
            interactions = obj.get("interactions") or []
            interaction_symbol = f"LOCATION_{loc_symbol}_OBJECT_{index}_INTERACTIONS"
            if interactions:
                for interaction_index, interaction in enumerate(interactions):
                    interaction_reqs = interaction.get("requirements") or []
                    interaction_req_symbol = f"LOCATION_{loc_symbol}_OBJECT_{index}_INTERACTION_{interaction_index}_REQS"
                    emit_location_requirements(lines, interaction_req_symbol, interaction_reqs)
                lines.append(f"static const game_location_interaction_t {interaction_symbol}[] = {{")
                for interaction_index, interaction in enumerate(interactions):
                    interaction_reqs = interaction.get("requirements") or []
                    interaction_req_symbol = f"LOCATION_{loc_symbol}_OBJECT_{index}_INTERACTION_{interaction_index}_REQS"
                    lines.append("    {")
                    lines.append(f"        .interaction_type = {c_str(interaction.get('type'))},")
                    lines.append(f"        .dialogue_id = {c_str(interaction.get('dialogue_id'))},")
                    lines.append(f"        .shop_id = {c_str(interaction.get('shop_id'))},")
                    lines.append(f"        .service_id = {c_str(interaction.get('service_id'))},")
                    lines.append(f"        .quest_id = {c_str(interaction.get('quest_id'))},")
                    lines.append(f"        .encounter_id = {c_str(interaction.get('encounter_id'))},")
                    lines.append(f"        .object_id = {c_str(interaction_object_id(interaction))},")
                    lines.append(f"        .requirements = {interaction_req_symbol if interaction_reqs else 'NULL'},")
                    lines.append(f"        .requirement_count = {len(interaction_reqs)},")
                    lines.append("    },")
                lines.append("};")
                lines.append("")

        object_symbol = f"LOCATION_{loc_symbol}_OBJECTS"
        if objects:
            location_object_symbols[location.get("id")] = object_symbol
            lines.append(f"static const game_location_object_t {object_symbol}[] = {{")
            for index, obj in enumerate(objects):
                interactions = obj.get("interactions") or []
                reqs = obj.get("requirements") or []
                scene = obj.get("scene") or {}
                bounds = scene_bounds(scene)
                req_symbol = f"LOCATION_{loc_symbol}_OBJECT_{index}_REQS"
                interaction_symbol = f"LOCATION_{loc_symbol}_OBJECT_{index}_INTERACTIONS"
                lines.append("    {")
                lines.append(f"        .id = {c_str(obj.get('id'))},")
                lines.append(f"        .kind = {c_str(obj.get('kind'))},")
                lines.append(f"        .display_name = {c_str(obj.get('display_name'))},")
                lines.append(f"        .character_id = {c_str(obj.get('character_id'))},")
                lines.append(f"        .asset_id = {c_str(obj.get('asset_id'))},")
                lines.append(f"        .encounter_id = {c_str(obj.get('encounter_id'))},")
                lines.append(
                    f"        .scene_bounds = {{.x = {bounds['x']}, .y = {bounds['y']}, .w = {bounds['w']}, .h = {bounds['h']}}},"
                )
                lines.append(f"        .scene_anchor_x = {float(scene.get('anchor_x', 0.0)):.3f}F,")
                lines.append(f"        .scene_anchor_y = {float(scene.get('anchor_y', 0.0)):.3f}F,")
                lines.append(f"        .scene_sprite_region_name = {c_str(scene.get('sprite_region_name'))},")
                lines.append(
                    f"        .scene_sprite_region_hash = {asset_region_hash_literal(scene.get('sprite_region_name'), asset_region_hashes)},"
                )
                lines.append(f"        .scene_sprite_target_h = {float(scene.get('sprite_target_h', 0.0)):.3f}F,")
                lines.append(f"        .scene_enabled = {c_bool(scene.get('enabled', False))},")
                lines.append(f"        .interactions = {interaction_symbol if interactions else 'NULL'},")
                lines.append(f"        .interaction_count = {len(interactions)},")
                lines.append(f"        .requirements = {req_symbol if reqs else 'NULL'},")
                lines.append(f"        .requirement_count = {len(reqs)},")
                lines.append("    },")
            lines.append("};")
            lines.append("")

    for location in locations:
        loc_symbol = c_ident(location.get("id", "location"))
        exits = location.get("exits") or []
        if not exits:
            continue
        exit_symbol = f"LOCATION_{loc_symbol}_EXITS"
        location_exit_symbols[location.get("id")] = exit_symbol
        lines.append(f"static const game_location_exit_t {exit_symbol}[] = {{")
        for index, exit_def in enumerate(exits):
            reqs = exit_def.get("requirements") or []
            req_symbol = f"LOCATION_{loc_symbol}_EXIT_{index}_REQS"
            lines.append("    {")
            lines.append(f"        .target_location_id = {c_str(exit_def.get('target_location_id'))},")
            lines.append(f"        .requirements = {req_symbol if reqs else 'NULL'},")
            lines.append(f"        .requirement_count = {len(reqs)},")
            lines.append("    },")
        lines.append("};")
        lines.append("")

    lines.append("static const game_location_definition_t LOCATIONS[] = {")
    for location in locations:
        unlock = location.get("unlock") or {}
        map_pos = location.get("map") or {}
        has_map_position = isinstance(map_pos, dict) and "x" in map_pos and "y" in map_pos
        map_x = float(map_pos.get("x", 0.0)) if has_map_position else 0.0
        map_y = float(map_pos.get("y", 0.0)) if has_map_position else 0.0
        exit_symbol = location_exit_symbols.get(location.get("id"))
        object_symbol = location_object_symbols.get(location.get("id"))
        lines.append("    {")
        lines.append(f"        .id = {c_str(location.get('id'))},")
        lines.append(f"        .display_name = {c_str(location.get('display_name'))},")
        lines.append(f"        .kind = {c_str(location.get('kind'))},")
        lines.append(f"        .screen_id = {c_str(location.get('screen_id'))},")
        lines.append(f"        .has_map_position = {'true' if has_map_position else 'false'},")
        lines.append(f"        .map_x = {map_x:.3f}F,")
        lines.append(f"        .map_y = {map_y:.3f}F,")
        lines.append(f"        .unlock_kind = {location_unlock_kind(unlock.get('mode'))},")
        lines.append(f"        .unlock_flag_id = {c_str(unlock.get('flag_id'))},")
        lines.append(f"        .unlock_quest_id = {c_str(unlock.get('quest_id'))},")
        lines.append(f"        .unlock_step_id = {c_str(unlock.get('step_id'))},")
        lines.append(f"        .objects = {object_symbol if object_symbol else 'NULL'},")
        lines.append(f"        .object_count = {len(location.get('objects', []))},")
        lines.append(f"        .exits = {exit_symbol if exit_symbol else 'NULL'},")
        lines.append(f"        .exit_count = {len(location.get('exits', []))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")

    for dialogue in dialogues:
        did = dialogue["id"]
        dsym = c_ident(did)
        quest_id = find_quest_id(dialogue)
        quest_name = quests.get(quest_id or "", {}).get("title") if quest_id else None
        preview = dialogue.get("quest_preview") or {}

        immediate = preview.get("immediate_rewards", [])
        completion = preview.get("completion_rewards", [])
        immediate_sym = f"{dsym}_IMMEDIATE_REWARDS"
        completion_sym = f"{dsym}_COMPLETION_REWARDS"
        preview_sym = f"{dsym}_PREVIEW"
        nodes_sym = f"{dsym}_NODES"

        emit_rewards(lines, immediate_sym, immediate, items)
        emit_rewards(lines, completion_sym, completion, items)
        if preview:
            lines.append(f"static const dialogue_quest_preview_t {preview_sym} = {{")
            lines.append(f"    .goal = {c_str(preview.get('goal'))},")
            lines.append(f"    .immediate_rewards = {immediate_sym if immediate else 'NULL'},")
            lines.append(f"    .immediate_reward_count = {len(immediate)},")
            lines.append(f"    .completion_rewards = {completion_sym if completion else 'NULL'},")
            lines.append(f"    .completion_reward_count = {len(completion)},")
            lines.append("};")
            lines.append("")

        for node in dialogue.get("nodes", []):
            choices = node.get("choices", [])
            choice_sym = f"{dsym}_{c_ident(node['id'])}_CHOICES"
            if not choices:
                continue
            for choice in choices:
                emit_effects(lines, f"{choice_sym}_{c_ident(choice.get('id', 'choice'))}_EFFECTS", choice.get("effects", []))
            lines.append(f"static const dialogue_choice_t {choice_sym}[] = {{")
            for choice in choices:
                effects = choice.get("effects", [])
                advance = next((e for e in effects if e.get("type") == "advance_quest"), None)
                kind = "DIALOGUE_CHOICE_PROGRESS" if advance else "DIALOGUE_CHOICE_BRANCH"
                effect_sym = f"{choice_sym}_{c_ident(choice.get('id', 'choice'))}_EFFECTS"
                lines.append("    {")
                lines.append(f"        .id = {c_str(choice.get('id'))},")
                lines.append(f"        .text = {c_str(choice.get('text'))},")
                lines.append(f"        .next_node_id = {c_str(choice.get('next_node_id'))},")
                lines.append(f"        .kind = {kind},")
                lines.append(f"        .quest_id = {c_str(advance.get('quest_id') if advance else None)},")
                lines.append(f"        .step_id = {c_str(advance.get('step_id') if advance else None)},")
                lines.append(f"        .reward_id = {c_str(choice.get('reward_id'))},")
                lines.append(f"        .effects = {effect_sym if effects else 'NULL'},")
                lines.append(f"        .effect_count = {len(effects)},")
                lines.append("    },")
            lines.append("};")
            lines.append("")

        lines.append(f"static const dialogue_node_t {nodes_sym}[] = {{")
        for node in dialogue.get("nodes", []):
            choices = node.get("choices", [])
            speaker = characters.get(node.get("speaker_id", ""), {})
            choice_sym = f"{dsym}_{c_ident(node['id'])}_CHOICES"
            lines.append("    {")
            lines.append(f"        .id = {c_str(node.get('id'))},")
            lines.append(f"        .speaker_id = {c_str(node.get('speaker_id'))},")
            lines.append(f"        .speaker_name = {c_str(speaker.get('display_name'))},")
            lines.append(f"        .quest_name = {c_str(quest_name)},")
            lines.append(f"        .text = {c_str(node.get('text'))},")
            lines.append(f"        .choices = {choice_sym if choices else 'NULL'},")
            lines.append(f"        .choice_count = {len(choices)},")
            lines.append("    },")
        lines.append("};")
        lines.append("")

        dialogue_symbols.append((did, dsym))
        lines.append(f"static const dialogue_definition_t {dsym}_DEF = {{")
        lines.append(f"    .id = {c_str(did)},")
        lines.append(f"    .title = {c_str(dialogue.get('title'))},")
        lines.append(f"    .entry_node_id = {c_str(dialogue.get('entry_node_id'))},")
        lines.append(f"    .nodes = {nodes_sym},")
        lines.append(f"    .node_count = {len(dialogue.get('nodes', []))},")
        lines.append(f"    .quest_preview = {'&' + preview_sym if preview else 'NULL'},")
        lines.append("};")
        lines.append("")

    lines.append("const dialogue_definition_t *game_content_find_dialogue(const char *dialogue_id) {")
    lines.append("    if (!dialogue_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    for did, dsym in dialogue_symbols:
        lines.append(f"    if (strcmp(dialogue_id, {c_str(did)}) == 0) {{")
        lines.append(f"        return &{dsym}_DEF;")
        lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("const game_item_definition_t *game_content_find_item(const char *item_id) {")
    lines.append("    if (!item_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof ITEMS / sizeof ITEMS[0]); ++i) {")
    lines.append("        if (strcmp(ITEMS[i].id, item_id) == 0) {")
    lines.append("            return &ITEMS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("const game_location_definition_t *game_content_find_location(const char *location_id) {")
    lines.append("    if (!location_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof LOCATIONS / sizeof LOCATIONS[0]); ++i) {")
    lines.append("        if (strcmp(LOCATIONS[i].id, location_id) == 0) {")
    lines.append("            return &LOCATIONS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("int game_content_location_count(void) {")
    lines.append("    return (int)(sizeof LOCATIONS / sizeof LOCATIONS[0]);")
    lines.append("}")
    lines.append("")
    lines.append("const game_location_definition_t *game_content_location_at(int index) {")
    lines.append("    if (index < 0 || index >= (int)(sizeof LOCATIONS / sizeof LOCATIONS[0])) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    return &LOCATIONS[index];")
    lines.append("}")
    lines.append("")
    lines.append("const game_shop_definition_t *game_content_find_shop(const char *shop_id) {")
    lines.append("    if (!shop_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof SHOPS / sizeof SHOPS[0]); ++i) {")
    lines.append("        if (strcmp(SHOPS[i].id, shop_id) == 0) {")
    lines.append("            return &SHOPS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("int game_content_shop_count(void) {")
    lines.append("    return (int)(sizeof SHOPS / sizeof SHOPS[0]);")
    lines.append("}")
    lines.append("")
    lines.append("const game_shop_definition_t *game_content_shop_at(int index) {")
    lines.append("    if (index < 0 || index >= game_content_shop_count()) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    return &SHOPS[index];")
    lines.append("}")
    lines.append("")
    lines.append("const char *game_content_next_quest_step(const char *quest_id, const char *step_id) {")
    lines.append("    if (!quest_id || !step_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    for quest in quests.values():
        steps = quest.get("steps", [])
        for index, step in enumerate(steps):
            next_step = steps[index + 1]["id"] if index + 1 < len(steps) else None
            lines.append(f"    if (strcmp(quest_id, {c_str(quest.get('id'))}) == 0 && strcmp(step_id, {c_str(step.get('id'))}) == 0) {{")
            lines.append(f"        return {c_str(next_step)};")
            lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("const game_combat_stats_t *game_content_base_player_stats(void) {")
    lines.append("    return &BASE_PLAYER_STATS;")
    lines.append("}")
    lines.append("")
    lines.append("const game_encounter_definition_t *game_content_find_encounter(const char *encounter_id) {")
    lines.append("    if (!encounter_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof ENCOUNTERS / sizeof ENCOUNTERS[0]); ++i) {")
    lines.append("        if (strcmp(ENCOUNTERS[i].id, encounter_id) == 0) {")
    lines.append("            return &ENCOUNTERS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("int game_content_equipment_slot_count(void) {")
    lines.append("    return (int)(sizeof EQUIPMENT_SLOTS / sizeof EQUIPMENT_SLOTS[0]);")
    lines.append("}")
    lines.append("")
    lines.append("const game_equipment_slot_definition_t *game_content_equipment_slot_at(int index) {")
    lines.append("    if (index < 0 || index >= game_content_equipment_slot_count()) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    return &EQUIPMENT_SLOTS[index];")
    lines.append("}")
    lines.append("")
    lines.append("const game_quest_equip_step_t *game_content_find_equip_step(const char *quest_id, const char *item_id) {")
    lines.append("    if (!quest_id || !item_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof QUEST_EQUIP_STEPS / sizeof QUEST_EQUIP_STEPS[0]); ++i) {")
    lines.append("        if (strcmp(QUEST_EQUIP_STEPS[i].quest_id, quest_id) == 0 && strcmp(QUEST_EQUIP_STEPS[i].item_id, item_id) == 0) {")
    lines.append("            return &QUEST_EQUIP_STEPS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("const game_quest_visit_step_t *game_content_find_visit_step(const char *quest_id, const char *location_id) {")
    lines.append("    if (!quest_id || !location_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof QUEST_VISIT_STEPS / sizeof QUEST_VISIT_STEPS[0]); ++i) {")
    lines.append("        if (strcmp(QUEST_VISIT_STEPS[i].quest_id, quest_id) == 0 && strcmp(QUEST_VISIT_STEPS[i].location_id, location_id) == 0) {")
    lines.append("            return &QUEST_VISIT_STEPS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("const game_quest_inspect_step_t *game_content_find_inspect_step(const char *quest_id, const char *object_id) {")
    lines.append("    if (!quest_id || !object_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof QUEST_INSPECT_STEPS / sizeof QUEST_INSPECT_STEPS[0]); ++i) {")
    lines.append("        if (strcmp(QUEST_INSPECT_STEPS[i].quest_id, quest_id) == 0 && strcmp(QUEST_INSPECT_STEPS[i].object_id, object_id) == 0) {")
    lines.append("            return &QUEST_INSPECT_STEPS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")
    lines.append("const game_quest_encounter_step_t *game_content_find_encounter_step(const char *quest_id, const char *encounter_id) {")
    lines.append("    if (!quest_id || !encounter_id) {")
    lines.append("        return NULL;")
    lines.append("    }")
    lines.append("    for (int i = 0; i < (int)(sizeof QUEST_ENCOUNTER_STEPS / sizeof QUEST_ENCOUNTER_STEPS[0]); ++i) {")
    lines.append("        if (strcmp(QUEST_ENCOUNTER_STEPS[i].quest_id, quest_id) == 0 && strcmp(QUEST_ENCOUNTER_STEPS[i].encounter_id, encounter_id) == 0) {")
    lines.append("            return &QUEST_ENCOUNTER_STEPS[i];")
    lines.append("        }")
    lines.append("    }")
    lines.append("    return NULL;")
    lines.append("}")
    lines.append("")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
