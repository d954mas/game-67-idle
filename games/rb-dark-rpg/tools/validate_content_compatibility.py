#!/usr/bin/env python3
"""Validate rb-dark-rpg authored content against save-compatible stable ids."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


ID_RE = re.compile(r"^[a-z0-9_.]+$")


def load_json(path: Path) -> Any:
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        raise ValueError(f"missing required file: {path}") from None
    except json.JSONDecodeError as exc:
        raise ValueError(f"{path}: invalid JSON: {exc}") from None


def add_id(target: dict[str, set[str]], category: str, value: Any) -> None:
    if isinstance(value, str) and value:
        target.setdefault(category, set()).add(value)


def collect_string_literals(paths: list[Path]) -> set[str]:
    values: set[str] = set()
    literal_re = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')
    for path in paths:
        if not path.exists():
            continue
        for match in literal_re.finditer(path.read_text(encoding="utf-8", errors="ignore")):
            raw = match.group(1)
            if ID_RE.match(raw):
                values.add(raw)
    return values


def collect_recursive_ids(node: Any, observed: dict[str, set[str]]) -> None:
    if isinstance(node, dict):
        for key, value in node.items():
            if key in {"flag_id"}:
                add_id(observed, "flags", value)
            elif key in {"quest_id", "quest_completed", "quest_active"}:
                add_id(observed, "quests", value)
            elif key in {"quest_ids"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "quests", item)
            elif key in {"step_id"}:
                add_id(observed, "quest_steps", value)
            elif key in {"item_id", "not_equipped", "equipped"}:
                add_id(observed, "items", value)
            elif key in {"dialogue_id"}:
                add_id(observed, "dialogues", value)
            elif key in {"location_id", "target_location_id"}:
                add_id(observed, "locations", value)
            elif key in {"encounter_id"}:
                add_id(observed, "encounters", value)
            elif key in {"character_id", "speaker_id", "provider_character_id", "keeper_character_id"}:
                add_id(observed, "characters", value)
            elif key in {"screen_id"}:
                add_id(observed, "unlocks", value)
            elif key in {"unlock_screens"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "unlocks", item)
            elif key in {"unlock_locations"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "unlocks", item)
                    add_id(observed, "locations", item)
            elif key in {"unlock_quests"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "unlocks", item)
                    add_id(observed, "quests", item)
            elif key in {"unlock_encounters"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "unlocks", item)
                    add_id(observed, "encounters", item)
            elif key in {"unlock_npcs"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "unlocks", item)
                    add_id(observed, "characters", item)
            elif key in {"set_flags", "flags"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "flags", item)
            elif key in {"grant_items", "items", "reward_items", "starter_loadout", "upgrade_examples"} and isinstance(value, list):
                for item in value:
                    add_id(observed, "items", item)
            collect_recursive_ids(value, observed)
    elif isinstance(node, list):
        for item in node:
            collect_recursive_ids(item, observed)


def collect_content_ids(game_dir: Path) -> tuple[dict[str, set[str]], dict[str, set[str]]]:
    data_dir = game_dir / "design" / "data"
    defined: dict[str, set[str]] = {
        "characters": set(),
        "locations": set(),
        "encounters": set(),
        "items": set(),
        "quests": set(),
        "quest_steps": set(),
        "dialogues": set(),
        "dialogue_choices": set(),
        "reward_ids": set(),
        "flags": set(),
        "unlocks": set(),
    }
    references = {category: set() for category in defined}

    characters = load_json(data_dir / "characters.json")
    for character in characters.get("characters", []):
        add_id(defined, "characters", character.get("id"))
    collect_recursive_ids(characters, references)

    locations = load_json(data_dir / "locations.json")
    for location in locations.get("locations", []):
        add_id(defined, "locations", location.get("id"))
        for obj in location.get("objects", []):
            add_id(defined, "unlocks", obj.get("id"))
    collect_recursive_ids(locations, references)

    combat = load_json(data_dir / "combat.json")
    for encounter in combat.get("encounters", []):
        add_id(defined, "encounters", encounter.get("id"))
    collect_recursive_ids(combat, references)

    items = load_json(data_dir / "items.json")
    for item in items.get("items", []):
        add_id(defined, "items", item.get("id"))
    collect_recursive_ids(items, references)

    quests = load_json(data_dir / "quests.json")
    for quest in quests.get("quests", []):
        add_id(defined, "quests", quest.get("id"))
        for step in quest.get("steps", []):
            add_id(defined, "quest_steps", step.get("id"))
    collect_recursive_ids(quests, references)

    dialogues = load_json(data_dir / "dialogues.json")
    for dialogue in dialogues.get("dialogues", []):
        dialogue_id = dialogue.get("id")
        add_id(defined, "dialogues", dialogue_id)
        seen_choices: set[str] = set()
        for node in dialogue.get("nodes", []):
            for choice in node.get("choices", []):
                choice_id = choice.get("id")
                if isinstance(dialogue_id, str) and isinstance(choice_id, str):
                    full_choice_id = f"{dialogue_id}.{choice_id}"
                    seen_choices.add(full_choice_id)
                    add_id(defined, "reward_ids", choice.get("reward_id"))
                    effects = choice.get("effects", [])
                    if not choice.get("reward_id") and any(isinstance(effect, dict) and effect.get("type") == "grant_item" for effect in effects):
                        defined["reward_ids"].add(f"{full_choice_id}.immediate")
        defined["dialogue_choices"].update(seen_choices)
    collect_recursive_ids(dialogues, references)

    services = load_json(data_dir / "services.json")
    collect_recursive_ids(services, references)

    defined["flags"].update(references["flags"])
    defined["unlocks"].update(references["unlocks"])

    source_literals = collect_string_literals(list((game_dir / "src").rglob("*.c")) + list((game_dir / "src").rglob("*.h")))
    for value in source_literals:
        if value.startswith("dlg_") and "." in value:
            defined["dialogue_choices"].add(value)
        if value.endswith(".immediate") or value.endswith(".completion"):
            defined["reward_ids"].add(value)
        if value.endswith("_seen") or value.endswith("_received") or value.endswith("_equipped") or value.endswith("_unlocked") or value.endswith("_owned") or value.endswith("_defeated"):
            defined["flags"].add(value)

    return defined, references


def validate_duplicate_ids(label: str, values: list[str], errors: list[str]) -> None:
    seen: set[str] = set()
    for value in values:
        if value in seen:
            errors.append(f"duplicate {label} id: {value}")
        seen.add(value)


def collect_duplicates(game_dir: Path, errors: list[str]) -> None:
    data_dir = game_dir / "design" / "data"
    files = {
        "character": ("characters.json", "characters"),
        "location": ("locations.json", "locations"),
        "encounter": ("combat.json", "encounters"),
        "item": ("items.json", "items"),
        "quest": ("quests.json", "quests"),
        "dialogue": ("dialogues.json", "dialogues"),
    }
    for label, (filename, collection) in files.items():
        data = load_json(data_dir / filename)
        ids = [entry.get("id") for entry in data.get(collection, []) if isinstance(entry.get("id"), str)]
        validate_duplicate_ids(label, ids, errors)

    quests = load_json(data_dir / "quests.json")
    step_ids: list[str] = []
    for quest in quests.get("quests", []):
        for step in quest.get("steps", []):
            if isinstance(step.get("id"), str):
                step_ids.append(step["id"])
    validate_duplicate_ids("quest step", step_ids, errors)


def migration_covers(compat: dict[str, Any], category: str, stable_id: str) -> bool:
    for migration in compat.get("migrations", []):
        if migration.get("category") != category:
            continue
        if migration.get("from_id") != stable_id:
            continue
        if migration.get("action") not in {"delete", "rename", "replace", "archive"}:
            continue
        if not migration.get("plan"):
            continue
        return True
    return False


def validate_stable_ids(compat: dict[str, Any], observed: dict[str, set[str]], errors: list[str]) -> None:
    stable = compat.get("stable_ids", {})
    if not isinstance(stable, dict):
        errors.append("content_compatibility.json: stable_ids must be an object")
        return
    for category, ids in stable.items():
        if not isinstance(ids, list):
            errors.append(f"stable_ids.{category} must be a list")
            continue
        seen: set[str] = set()
        for stable_id in ids:
            if not isinstance(stable_id, str) or not stable_id:
                errors.append(f"stable_ids.{category} contains a non-empty string violation")
                continue
            if stable_id in seen:
                errors.append(f"stable_ids.{category} duplicates {stable_id}")
            seen.add(stable_id)
            if not ID_RE.match(stable_id):
                errors.append(f"stable id {category}.{stable_id} must be lowercase ASCII id syntax")
            if stable_id not in observed.get(category, set()) and not migration_covers(compat, category, stable_id):
                errors.append(
                    f"destructive content change without migration plan: stable_ids.{category} missing {stable_id}"
                )


def validate_status_enum(game_dir: Path, errors: list[str]) -> None:
    quests = load_json(game_dir / "design" / "data" / "quests.json")
    schema = load_json(game_dir / "state" / "game_state.schema.json")
    quest_status = schema.get("enums", {}).get("QuestStatus")
    authored_status = quests.get("status_enum")
    if quest_status != authored_status:
        errors.append(
            "quests.json status_enum must match state/game_state.schema.json enums.QuestStatus"
        )


def validate_refs_warn(defined: dict[str, set[str]], references: dict[str, set[str]], warnings: list[str]) -> None:
    for category in ["characters", "locations", "encounters", "items", "quests", "quest_steps", "dialogues"]:
        missing = sorted(references.get(category, set()) - defined.get(category, set()))
        if missing:
            warnings.append(f"{category}: referenced but not defined: {', '.join(missing)}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-dir", default="games/rb-dark-rpg")
    parser.add_argument("--warnings", action="store_true", help="print non-blocking content reference warnings")
    args = parser.parse_args(argv)

    game_dir = Path(args.game_dir)
    errors: list[str] = []
    warnings: list[str] = []

    try:
        compat = load_json(game_dir / "design" / "data" / "content_compatibility.json")
        observed, references = collect_content_ids(game_dir)
        collect_duplicates(game_dir, errors)
        validate_status_enum(game_dir, errors)
        validate_stable_ids(compat, observed, errors)
        if args.warnings:
            validate_refs_warn(observed, references, warnings)
    except ValueError as exc:
        errors.append(str(exc))

    for warning in warnings:
        print(f"WARNING: {warning}")
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("content compatibility validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
