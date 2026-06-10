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

            state = bus.result("game.state.set", {"doc": "game", "path": "meme_coins", "value": 1234})
            ok &= check("set meme coins", state.get("meme_coins") == 1234, state)
            fractional = bus.request("game.state.set", {"doc": "game", "path": "meme_coins", "value": 1.5})
            ok &= check("reject fractional int", fractional.get("ok") is False, fractional)
            state = bus.result(
                "game.state.patch",
                {
                    "doc": "game",
                    "values": {
                        "status": 2,
                        "click_power": 2,
                        "first_upgrade_owned": True,
                        "second_upgrade_owned": True,
                        "third_upgrade_owned": True,
                        "fourth_upgrade_owned": True,
                        "fifth_upgrade_owned": True,
                        "active_job_id": "kiosk_memes",
                        "active_job_elapsed_ms": 1200,
                        "active_job_duration_ms": 6000,
                    },
                },
            )
            ok &= check(
                "patch gameplay progression",
                state.get("status") == 2
                and state.get("click_power") == 2
                and state.get("first_upgrade_owned") is True
                and state.get("second_upgrade_owned") is True
                and state.get("third_upgrade_owned") is True
                and state.get("fourth_upgrade_owned") is True
                and state.get("fifth_upgrade_owned") is True
                and state.get("active_job_id") == "kiosk_memes"
                and state.get("active_job_elapsed_ms") == 1200,
                state,
            )
            third_flag = bus.result("game.state.get", {"doc": "game", "path": "third_upgrade_owned"})
            ok &= check("get third upgrade flag", third_flag is True, third_flag)
            state = bus.result("game.state.set", {"doc": "game", "path": "third_upgrade_owned", "value": False})
            ok &= check("set third upgrade flag", state.get("third_upgrade_owned") is False, state)
            state = bus.result("game.state.set", {"doc": "game", "path": "third_upgrade_owned", "value": True})
            ok &= check("restore third upgrade flag", state.get("third_upgrade_owned") is True, state)
            fourth_flag = bus.result("game.state.get", {"doc": "game", "path": "fourth_upgrade_owned"})
            ok &= check("get fourth upgrade flag", fourth_flag is True, fourth_flag)
            state = bus.result("game.state.set", {"doc": "game", "path": "fourth_upgrade_owned", "value": False})
            ok &= check("set fourth upgrade flag", state.get("fourth_upgrade_owned") is False, state)
            state = bus.result("game.state.set", {"doc": "game", "path": "fourth_upgrade_owned", "value": True})
            ok &= check("restore fourth upgrade flag", state.get("fourth_upgrade_owned") is True, state)
            fifth_flag = bus.result("game.state.get", {"doc": "game", "path": "fifth_upgrade_owned"})
            ok &= check("get fifth upgrade flag", fifth_flag is True, fifth_flag)
            state = bus.result("game.state.set", {"doc": "game", "path": "fifth_upgrade_owned", "value": False})
            ok &= check("set fifth upgrade flag", state.get("fifth_upgrade_owned") is False, state)
            state = bus.result("game.state.set", {"doc": "game", "path": "fifth_upgrade_owned", "value": True})
            ok &= check("restore fifth upgrade flag", state.get("fifth_upgrade_owned") is True, state)
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
            reset_coins = bus.result("game.state.get", {"doc": "game", "path": "meme_coins"})
            ok &= check("reset meme coins", reset_coins == 0, reset_coins)
            loaded = bus.result("game.state.load", {"key": save_key, "doc": "game"})
            ok &= check("load restored meme coins", loaded.get("meme_coins") == 1234, loaded)
            ok &= check(
                "load restored gameplay progression",
                loaded.get("status") == 2
                and loaded.get("active_job_id") == "kiosk_memes"
                and loaded.get("third_upgrade_owned") is True
                and loaded.get("fourth_upgrade_owned") is True
                and loaded.get("fifth_upgrade_owned") is True,
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
            ok &= check("migration adds third upgrade flag", migrated.get("third_upgrade_owned") is False, migrated)
            ok &= check("migration adds fourth upgrade flag", migrated.get("fourth_upgrade_owned") is False, migrated)
            ok &= check("migration adds fifth upgrade flag", migrated.get("fifth_upgrade_owned") is False, migrated)
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
