from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


REQUIRED_FILES = [
    "concept.md",
    "gdd.md",
    "references.md",
    "combat_spec.md",
    "handoff_status.md",
    "game_implementation_plan.md",
    "index.html",
    "site.css",
    "site.js",
    "server.mjs",
    "data/balance.json",
    "data/ui_flow.json",
    "data/combat.json",
    "data/asset_manifest.json",
    "data/implementation_tasks.json",
]


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def load_json(relative: str) -> dict:
    path = ROOT / relative
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail(f"{relative} is invalid JSON: {exc}")


def require_text(path: Path, needle: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    if needle not in text:
        fail(f"{path.relative_to(ROOT)} is missing {label}: {needle}")


def main() -> None:
    for relative in REQUIRED_FILES:
        path = ROOT / relative
        if not path.exists():
            fail(f"missing required file: {relative}")

    balance = load_json("data/balance.json")
    ui_flow = load_json("data/ui_flow.json")
    combat = load_json("data/combat.json")
    assets = load_json("data/asset_manifest.json")
    tasks = load_json("data/implementation_tasks.json")

    for key in ["currencies", "stats", "activities", "upgrades"]:
        if not balance.get(key):
            fail(f"balance.json missing non-empty {key}")

    if len(ui_flow.get("screens", [])) < 5:
        fail("ui_flow.json should define at least 5 screens")

    if len(tasks.get("phases", [])) < 3:
        fail("implementation_tasks.json should define implementation phases")

    if not combat.get("enemies"):
        fail("combat.json missing enemies")
    if not combat.get("player_actions"):
        fail("combat.json missing player_actions")
    if not combat.get("expected_paths"):
        fail("combat.json missing expected_paths")
    if "ruin_wolf" not in {enemy.get("id") for enemy in combat.get("enemies", [])}:
        fail("combat.json should define first enemy ruin_wolf")

    for asset in assets.get("assets", []):
        file_value = asset.get("file")
        if not file_value:
            fail("asset_manifest entry missing file")
        if not (ROOT / file_value).exists():
            fail(f"asset file does not exist: {file_value}")

    index = ROOT / "index.html"
    require_text(index, "fake-shot-ruins-background.png", "ruins fake shot")
    require_text(index, "camp-preparation-background.png", "camp fake shot")
    require_text(index, "data/balance.json", "balance data link")
    require_text(index, "data/ui_flow.json", "ui flow data link")
    require_text(index, "First Combat", "first combat site section")

    gdd = ROOT / "gdd.md"
    require_text(gdd, "Camp is a preparation/narrative screen, not a base builder.", "camp scope rule")
    require_text(gdd, "choose destination -> encounter/event", "core loop")
    require_text(gdd, "data/combat.json", "combat data source")

    combat_spec = ROOT / "combat_spec.md"
    require_text(combat_spec, "Expected First Win", "expected combat path")
    require_text(combat_spec, "Camp Recovery Link", "combat to camp recovery link")

    print("Fantasy Pocket RPG GDD package is valid.")


if __name__ == "__main__":
    main()
