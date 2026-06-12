#!/usr/bin/env python3
"""Validate generated game state DevAPI commands."""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from devapi_client import DevApiError, ROOT, running_game

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9123


def check(name: str, condition: bool, extra="") -> bool:
    print(("PASS" if condition else "FAIL"), name, "::", extra)
    return condition


def main() -> int:
    ok = True
    try:
        with running_game(port=PORT) as bus:
            endpoints = bus.result("endpoints")
            required = {
                "game.state.schema",
                "game.state.get",
                "game.state.set",
                "game.state.patch",
                "game.state.save",
                "game.state.load",
                "game.state.reset",
            }
            ok &= check("state endpoints", required.issubset(set(endpoints)), endpoints)

            schema = bus.result("game.state.schema", {"doc": "game"})
            ok &= check("schema document", schema.get("document") == "game", schema)
            ok &= check("schema version", schema.get("version") == 1, schema)

            state = bus.result("game.state.set", {"doc": "game", "path": "wallet.soft", "value": 1234})
            ok &= check("set wallet soft", state.get("wallet", {}).get("soft") == 1234, state)
            fractional = bus.request("game.state.set", {"doc": "game", "path": "wallet.soft", "value": 1.5})
            ok &= check("reject fractional int", fractional.get("ok") is False, fractional)
            state = bus.result(
                "game.state.patch",
                {
                    "doc": "game",
                    "values": {
                        "test_ui_clicks": 7,
                        "test_label_text": "Roundtrip label",
                        "wallet.hard": 3,
                    },
                },
            )
            ok &= check(
                "patch scalar fields",
                state.get("test_ui_clicks") == 7
                and state.get("test_label_text") == "Roundtrip label"
                and state.get("wallet", {}).get("hard") == 3,
                state,
            )
            tutorial_flag = bus.result("game.state.get", {"doc": "game", "path": "tutorial.done"})
            ok &= check("get tutorial flag default", tutorial_flag is False, tutorial_flag)
            state = bus.result("game.state.set", {"doc": "game", "path": "tutorial.done", "value": True})
            ok &= check("set tutorial flag", state.get("tutorial", {}).get("done") is True, state)
            bad_bool = bus.request("game.state.set", {"doc": "game", "path": "tutorial.done", "value": 1})
            ok &= check("reject non-bool flag", bad_bool.get("ok") is False, bad_bool)
            state = bus.result("game.state.set", {"doc": "game", "path": "tutorial.done", "value": False})
            ok &= check("restore tutorial flag", state.get("tutorial", {}).get("done") is False, state)
            state = bus.result("game.state.patch", {"doc": "game", "values": {"settings.master_volume": 0.25, "settings.sfx_volume": 0.5}})
            ok &= check("patch settings", state.get("settings", {}).get("master_volume") == 0.25 and state.get("settings", {}).get("sfx_volume") == 0.5, state)
            bad_volume = bus.request("game.state.set", {"doc": "game", "path": "settings.master_volume", "value": 1.5})
            ok &= check("reject volume out of range", bad_volume.get("ok") is False, bad_volume)

            item_value = {"def_id": "sword_01", "count": 1, "level": 3, "durability": 0.75}
            state = bus.result(
                "game.state.patch",
                {
                    "doc": "game",
                    "values": {
                        "tutorial.done": True,
                        "items.item_001": item_value,
                        "inventory.item_ids": ["item_001"],
                        "equipment.hand_item_id": "item_001",
                    },
                },
            )
            ok &= check("patch tutorial", state.get("tutorial", {}).get("done") is True, state)
            ok &= check("patch item map", state.get("items", {}).get("item_001", {}).get("level") == 3, state)
            ok &= check("patch equipment ref", state.get("equipment", {}).get("hand_item_id") == "item_001", state)

            item_count = bus.result("game.state.get", {"doc": "game", "path": "items.item_001.count"})
            ok &= check("get nested item", item_count == 1, item_count)
            dangling_ref = bus.request("game.state.set", {"doc": "game", "path": "equipment.hand_item_id", "value": "missing_item"})
            ok &= check("reject dangling equipment ref", dangling_ref.get("ok") is False, dangling_ref)

            save_key = "slot_1"
            saved = bus.result("game.state.save", {"key": save_key, "doc": "game"})
            ok &= check("save by key resolves native path", saved.get("key") == save_key and saved.get("resolved", "").endswith("slot_1/game.json"), saved)
            bad_key = bus.request("game.state.save", {"key": "../bad", "doc": "game"})
            ok &= check("reject unsafe save key", bad_key.get("ok") is False, bad_key)
            bus.result("game.state.reset", {"doc": "game"})
            reset_soft = bus.result("game.state.get", {"doc": "game", "path": "wallet.soft"})
            ok &= check("reset wallet soft", reset_soft == 0, reset_soft)
            loaded = bus.result("game.state.load", {"key": save_key, "doc": "game"})
            ok &= check("load restored wallet soft", loaded.get("wallet", {}).get("soft") == 1234, loaded)
            ok &= check(
                "load restored scalar fields",
                loaded.get("test_ui_clicks") == 7
                and loaded.get("test_label_text") == "Roundtrip label"
                and loaded.get("wallet", {}).get("hard") == 3,
                loaded,
            )
            ok &= check("load restored settings", loaded.get("settings", {}).get("master_volume") == 0.25, loaded)
            missing_key = bus.request("game.state.load", {"key": "missing_slot", "doc": "game"})
            ok &= check("reject missing load key", missing_key.get("ok") is False, missing_key)

            fixture = os.path.join(ROOT, "state", "fixtures", "v0_save.json")
            migrated = bus.result("game.state.load", {"doc": "game", "unsafe_path": fixture})
            ok &= check("migration shape", migrated.get("shape") == "sphere", migrated)
            ok &= check("migration render mode", migrated.get("render_mode") == "wire", migrated)
            ok &= check("migration adds wallet", migrated.get("wallet", {}).get("soft") == 0, migrated)
            wrong_doc = os.path.join(ROOT, "state", "fixtures", "wrong_document.json")
            wrong_doc_result = bus.request("game.state.load", {"doc": "game", "unsafe_path": wrong_doc})
            ok &= check("reject wrong document", wrong_doc_result.get("ok") is False, wrong_doc_result)
    except DevApiError as exc:
        print("FAIL devapi ::", exc)
        ok = False

    print("\n=== %s ===" % ("ALL PASSED" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
