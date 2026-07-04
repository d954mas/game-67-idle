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


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


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
    if typ == "unlock_screen":
        return "Новый путь"
    return effect.get("type", "Награда")


def reward_icon(effect: dict, items: dict[str, dict]) -> str:
    typ = effect.get("type")
    if typ == "grant_xp":
        return "XP"
    if typ == "unlock_screen":
        return ">>"
    name = reward_name(effect, items)
    return name[:2].upper() if name else "?"


def reward_kind(effect: dict) -> str:
    typ = effect.get("type")
    if typ == "grant_xp":
        return "DIALOGUE_REWARD_XP"
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
    return "GAME_ITEM_KIND_UNKNOWN"


def item_slot(value: str | None) -> str:
    if value == "weapon":
        return "GAME_ITEM_SLOT_WEAPON"
    if value == "armour":
        return "GAME_ITEM_SLOT_ARMOUR"
    if value == "legs":
        return "GAME_ITEM_SLOT_LEGS"
    if value == "charm":
        return "GAME_ITEM_SLOT_CHARM"
    return "GAME_ITEM_SLOT_NONE"


def effect_kind(value: str | None) -> str:
    if value == "grant_item":
        return "DIALOGUE_EFFECT_GRANT_ITEM"
    if value == "advance_quest":
        return "DIALOGUE_EFFECT_ADVANCE_QUEST"
    if value == "set_flag":
        return "DIALOGUE_EFFECT_SET_FLAG"
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
        lines.append(f"        .summary = {c_str(summary)},")
        lines.append(f"        .detail = {c_str(detail)},")
        lines.append(f"        .kind = {reward_kind(effect)},")
        lines.append(f"        .amount = {reward_amount(effect)},")
        lines.append("    },")
    lines.append("};")
    lines.append("")


def main() -> int:
    if len(sys.argv) != 6:
        print(
            "usage: generate_dialogue_content.py <dialogues.json> <characters.json> <quests.json> <items.json> <out.c>",
            file=sys.stderr,
        )
        return 2

    dialogues_path = Path(sys.argv[1])
    characters_path = Path(sys.argv[2])
    quests_path = Path(sys.argv[3])
    items_path = Path(sys.argv[4])
    out_path = Path(sys.argv[5])

    dialogues = load_json(dialogues_path)["dialogues"]
    characters = {c["id"]: c for c in load_json(characters_path)["characters"]}
    quests = {q["id"]: q for q in load_json(quests_path)["quests"]}
    items = {i["id"]: i for i in load_json(items_path)["items"]}

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
        lines.append(f"        .kind = {item_kind(item.get('kind'))},")
        lines.append(f"        .slot = {item_slot(item.get('slot'))},")
        lines.append(f"        .stackable = {'true' if item.get('stackable') else 'false'},")
        lines.append(f"        .max_stack = {int(item.get('max_stack', 1))},")
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

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
