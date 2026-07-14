import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("items_lua_sandbox.py")


class ItemsLuaSandboxTests(unittest.TestCase):
    def evaluate(self, modules: dict[str, str], entries: list[str]):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_modules = []
            for name, source in modules.items():
                rel = Path(*name.split(".")).with_suffix(".lua")
                path = root / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(source, encoding="utf-8")
                manifest_modules.append({"name": name, "file": rel.as_posix()})
            manifest = root / "items.lua.json"
            manifest.write_text(json.dumps({
                "schema": "items.lua.sandbox.v1",
                "modules": manifest_modules,
                "entries": entries,
            }), encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(SCRIPT), "evaluate", "--root", str(root), "--manifest", str(manifest)],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )

    def test_representative_declarations_are_canonical_and_repeatable(self):
        modules = {
            "game.items.currencies": '''
local items = require("studio.items")
items.define({ id="game.gold", kind="currency", stack=0 })
''',
            "game.items.weapons": '''
local items = require("studio.items")
local levels = require("studio.levels")
items.define({
  id="game.iron_sword", kind="weapon", stack=1,
  levels=levels.single({ attack=15 }),
  acquire={ cost=items.cost(items.ref("game.gold"), 100) },
})
items.define({
  id="game.levelled_sword", kind="weapon", stack=1,
  levels=levels.table({
    [1]={ attack=10 },
    [2]={ attack=15, cost_to_reach=items.cost(items.ref("game.gold"), 100) },
    [3]={ attack=20, cost_to_reach=items.free() },
  }),
})
''',
        }
        first = self.evaluate(modules, ["game.items.weapons", "game.items.currencies"])
        second = self.evaluate(dict(reversed(list(modules.items()))), ["game.items.currencies", "game.items.weapons"])

        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(first.stdout, second.stdout)
        payload = json.loads(first.stdout)
        self.assertEqual(payload["backend"]["module"], "lupa.lua54")
        self.assertEqual([item["id"] for item in payload["items"]], [
            "game.gold", "game.iron_sword", "game.levelled_sword",
        ])
        self.assertEqual(payload["items"][2]["levels"]["rows"][2]["attack"], 20)


if __name__ == "__main__":
    unittest.main()
