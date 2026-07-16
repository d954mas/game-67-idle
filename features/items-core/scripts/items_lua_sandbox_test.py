import json
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest


SCRIPT = Path(__file__).with_name("items_lua_sandbox.py")
FIXTURE_ROOT = Path(__file__).parents[1] / "tests" / "fixtures" / "lua_sandbox"
TEMPLATE_ROOT = Path(__file__).parents[3] / "templates" / "template"
SPEC = importlib.util.spec_from_file_location("items_lua_sandbox", SCRIPT)
SANDBOX = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(SANDBOX)


class ItemsLuaSandboxTests(unittest.TestCase):
    def evaluate(
        self,
        modules: dict[str, str | bytes],
        entries: list[str],
        *extra_args: str,
    ):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest_modules = []
            for name, source in modules.items():
                rel = Path(*name.split(".")).with_suffix(".lua")
                path = root / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                if isinstance(source, bytes):
                    path.write_bytes(source)
                else:
                    path.write_text(source, encoding="utf-8")
                manifest_modules.append({"name": name, "file": rel.as_posix()})
            manifest = root / "items.lua.json"
            manifest.write_text(json.dumps({
                "schema": "items.lua.sandbox.v1",
                "modules": manifest_modules,
                "entries": entries,
            }), encoding="utf-8")
            if "--timeout-ms" not in extra_args:
                option_names = {
                    "--memory-bytes": "memoryBytes",
                    "--instruction-limit": "instructionLimit",
                    "--recursion-limit": "recursionLimit",
                    "--max-output-rows": "maxOutputRows",
                    "--max-output-bytes": "maxOutputBytes",
                    "--max-source-bytes": "maxSourceBytes",
                }
                request = {"root": str(root), "manifest": str(manifest)}
                for index in range(0, len(extra_args), 2):
                    request[option_names[extra_args[index]]] = int(extra_args[index + 1])
                return subprocess.run(
                    [sys.executable, str(SCRIPT), "--worker"], input=json.dumps(request),
                    text=True, capture_output=True, encoding="utf-8", timeout=10,
                )
            return subprocess.run(
                [
                    sys.executable, str(SCRIPT), "evaluate", "--root", str(root),
                    "--manifest", str(manifest), *extra_args,
                ],
                text=True, capture_output=True, encoding="utf-8", timeout=10,
            )

    def assert_error(self, result, code: str, file: str, line: int):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        payload = json.loads(result.stderr)
        self.assertEqual(payload["schema"], "items.lua.error.v1")
        self.assertEqual(payload["error"]["code"], code)
        self.assertEqual(payload["error"]["file"], file)
        self.assertEqual(payload["error"]["line"], line)
        self.assertEqual(payload["error"]["column"], 1)
        self.assertTrue(payload["error"]["path"].startswith("$"))

    def evaluate_fixture(self):
        return subprocess.run(
            [
                sys.executable, str(SCRIPT), "evaluate", "--root", str(FIXTURE_ROOT),
                "--manifest", str(FIXTURE_ROOT / "items.lua.json"),
            ],
            text=True, capture_output=True, encoding="utf-8", timeout=10,
        )

    def test_representative_declarations_are_canonical_and_repeatable(self):
        modules = {
            entry["name"]: (FIXTURE_ROOT / entry["file"]).read_text(encoding="utf-8")
            for entry in json.loads((FIXTURE_ROOT / "items.lua.json").read_text(encoding="utf-8"))["modules"]
        }
        first = self.evaluate_fixture()
        second = self.evaluate(dict(reversed(list(modules.items()))), ["game.items.currencies", "game.items.weapons"])

        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(first.stdout, second.stdout)
        payload = json.loads(first.stdout)
        self.assertEqual(payload, json.loads((FIXTURE_ROOT / "expected.json").read_text(encoding="utf-8")))
        self.assertEqual(payload["backend"]["module"], "lupa.lua54")
        self.assertEqual([item["id"] for item in payload["items"]], [
            "game.gold", "game.iron_sword", "game.levelled_sword",
        ])
        self.assertEqual(payload["items"][2]["levels"]["rows"][2]["attack"], 20)

    def test_template_lua_reproduces_six_catalog_definitions_without_containers(self):
        result = subprocess.run(
            [
                sys.executable, str(SCRIPT), "evaluate", "--root", str(TEMPLATE_ROOT),
                "--manifest", str(TEMPLATE_ROOT / "items.lua.json"),
            ],
            text=True, capture_output=True, encoding="utf-8", timeout=10,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["kinds"], ["consumable", "currency", "material", "weapon"])
        self.assertEqual(payload["items"], [
            {
                "authoring_mode": "none", "base_value": 0,
                "created": "2026-07-07",
                "currency": {"cap": 100, "hud": "counter"},
                "icon": "icons/energy", "id": "tmpl.energy",
                "kind": "currency", "name": "Energy", "stack": 0,
            },
            {
                "authoring_mode": "none", "base_value": 1,
                "created": "2026-07-07",
                "currency": {"cap": 0, "hud": "counter"},
                "icon": "icons/gold", "id": "tmpl.gold",
                "kind": "currency", "name": "Gold", "stack": 0,
            },
            {
                "authoring_mode": "none", "base_value": 10,
                "created": "2026-07-07", "icon": "icons/potion",
                "id": "tmpl.potion", "kind": "consumable",
                "name": "Healing Potion", "stack": 99, "tags": ["heal"],
                "use": {"effect_id": "heal", "params": {"amount": 25}},
            },
            {
                "authoring_mode": "single", "base_value": 50,
                "created": "2026-07-07", "equip": {"slot": "weapon"},
                "icon": "icons/sword", "id": "tmpl.sword", "kind": "weapon",
                "levels": {"mode": "single", "provenance": [{}], "rows": [{}]},
                "name": "Iron Sword", "stack": 1, "tags": ["melee"],
            },
            {
                "authoring_mode": "none", "base_value": 2,
                "created": "2026-07-07", "icon": "icons/wood",
                "id": "tmpl.wood", "kind": "material", "name": "Wood",
                "stack": 999,
            },
            {
                "authoring_mode": "none", "base_value": 0,
                "created": "2026-07-07",
                "currency": {"cap": 0, "hud": "bar"},
                "icon": "icons/xp", "id": "tmpl.xp",
                "kind": "currency", "name": "Experience", "stack": 0,
            },
        ])

    def test_item_definition_sources_are_honest_and_stable(self):
        result = self.evaluate({"game.items": '''-- heading
local items = require("studio.items")
items.define({ id="game.gold", kind="currency", stack=0 })
'''}, ["game.items"])

        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["sources"], {
            "game.gold": {
                "file": "game/items.lua",
                "line": 3,
                "column": 1,
                "end_line": 3,
                "end_column": 59,
                "kind": "definition",
                "snippet": 'items.define({ id="game.gold", kind="currency", stack=0 })',
            },
        })

        unicode_separator = self.evaluate({
            "game.unicode": 'local items=require("studio.items")\nlocal text="a\u2028b"\nitems.define({ id="game.unicode" })',
        }, ["game.unicode"])
        self.assertEqual(unicode_separator.returncode, 0, unicode_separator.stderr)
        unicode_source = json.loads(unicode_separator.stdout)["sources"]["game.unicode"]
        self.assertEqual(unicode_source["line"], 3)
        self.assertEqual(unicode_source["snippet"], 'items.define({ id="game.unicode" })')

    def test_refs_resolve_after_registration_and_fail_at_ref_source(self):
        forward = self.evaluate({
            "game.a_weapon": '''local items = require("studio.items")
items.define({
  id="game.sword", kind="weapon", stack=1,
  acquire={ cost=items.cost(items.ref("game.gold"), 10) },
})''',
            "game.z_currency": '''local items = require("studio.items")
items.define({ id="game.gold", kind="currency", stack=0 })''',
        }, ["game.a_weapon", "game.z_currency"])
        self.assertEqual(forward.returncode, 0, forward.stderr)
        ref = json.loads(forward.stdout)["items"][1]["acquire"]["cost"]["item"]
        self.assertEqual(ref, {"__studio_kind": "item_ref", "id": "game.gold"})

        hidden = self.evaluate({
            "game.items": '''local items = require("studio.items")
local gold = items.ref("game.gold")
items.define({ id="game.sword", kind="weapon", stack=1, leaked_source=gold.__studio_source })
items.define({ id="game.gold", kind="currency", stack=0 })''',
        }, ["game.items"])
        self.assertEqual(hidden.returncode, 0, hidden.stderr)
        sword = json.loads(hidden.stdout)["items"][1]
        self.assertNotIn("leaked_source", sword)

        missing = self.evaluate({
            "game.weapon": '''local items = require("studio.items")
items.define({
  id="game.sword", kind="weapon", stack=1,
  acquire={ cost=items.cost(items.ref("game.missing"), 10) },
})''',
        }, ["game.weapon"])
        self.assert_error(missing, "reference.missing", "game/weapon.lua", 4)
        self.assertIn("game.missing", json.loads(missing.stderr)["error"]["message"])

        duplicate = self.evaluate({
            "game.items": '''local items = require("studio.items")
items.define({ id="game.same", kind="currency", stack=0 })
items.define({ id="game.same", kind="material", stack=9 })''',
        }, ["game.items"])
        self.assert_error(duplicate, "definition.duplicate_id", "game/items.lua", 3)

    def test_schema_and_kinds_register_before_definitions_independent_of_module_order(self):
        modules = {
            "game.a_items": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", kind="weapon", stack=1,
  levels=levels.single({ attack=15 }),
})''',
            "game.z_schema": '''local field = require("studio.field")
local items = require("studio.items")
local options = {
  id="game.weapon.level.attack", required_for={"weapon"}, min=0, max=100,
  unit="damage", rounding="exact", label_key="item.attack",
}
local extension = { level_row={ attack=field.i64(options) } }
items.extend_schema(extension)
options.id = "game.changed"
extension.level_row.attack = nil''',
        }
        first = self.evaluate(modules, ["game.a_items", "game.z_schema"])
        second = self.evaluate(
            dict(reversed(list(modules.items()))),
            ["game.z_schema", "game.a_items"],
        )

        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(first.stdout, second.stdout)
        payload = json.loads(first.stdout)
        self.assertEqual(payload["kinds"], ["weapon"])
        self.assertEqual(payload["field_sources"]["game.weapon.level.attack"], {
            "file": "game/z_schema.lua",
            "line": 7,
            "column": 1,
            "end_line": 7,
            "end_column": 62,
            "kind": "field",
            "snippet": "local extension = { level_row={ attack=field.i64(options) } }",
        })
        self.assertEqual(payload["fields"], [{
            "id": "game.weapon.level.attack",
            "label_key": "item.attack",
            "max": 100,
            "member": "attack",
            "min": 0,
            "required_for": ["weapon"],
            "rounding": "exact",
            "section": "level_row",
            "type": "i64",
            "unit": "damage",
        }])

    def test_schema_handles_are_authentic_and_field_id_conflicts_are_source_local(self):
        forged = self.evaluate({"game.schema": '''local items = require("studio.items")
items.extend_schema({ level_row={ attack={
  __studio_kind="field", type="i64", id="game.weapon.level.attack",
} } })'''
        }, ["game.schema"])
        self.assert_error(forged, "schema.invalid_handle", "game/schema.lua", 2)

        duplicate = self.evaluate({"game.schema": '''local field = require("studio.field")
local items = require("studio.items")
items.extend_schema({ level_row={ attack=field.i64({ id="game.weapon.level.attack" }) } })
items.extend_schema({ level_row={ damage=field.i64({ id="game.weapon.level.attack" }) } })'''
        }, ["game.schema"])
        self.assert_error(duplicate, "schema.duplicate_field_id", "game/schema.lua", 4)

        duplicate_member = self.evaluate({"game.schema": '''local field = require("studio.field")
local items = require("studio.items")
items.extend_schema({ level_row={ attack=field.i64({ id="game.weapon.level.attack" }) } })
items.extend_schema({ level_row={ attack=field.i64({ id="game.weapon.level.damage" }) } })'''
        }, ["game.schema"])
        self.assert_error(duplicate_member, "schema.duplicate_member", "game/schema.lua", 4)

        sealed = self.evaluate({"game.schema": '''local field = require("studio.field")
local items = require("studio.items")
items.extend_schema({ level_row={ attack=field.i64({ id="items.level.attack" }) } })'''
        }, ["game.schema"])
        self.assert_error(sealed, "schema.sealed_field_id", "game/schema.lua", 3)

        reserved = self.evaluate({"game.schema": '''local field = require("studio.field")
local items = require("studio.items")
items.extend_schema({ level_row={ attack=field.i64({
  id="game.weapon.level.attack", type="string",
}) } })'''
        }, ["game.schema"])
        self.assert_error(reserved, "schema.reserved_key", "game/schema.lua", 3)

        for invalid_id in (
            "game.weapon..attack",
            "game.weapon.1attack",
            "game.weapon.",
        ):
            with self.subTest(invalid_id=invalid_id):
                invalid = self.evaluate({"game.schema": f'''local field = require("studio.field")
local items = require("studio.items")
items.extend_schema({{ level_row={{ attack=field.i64({{ id="{invalid_id}" }}) }} }})'''
                }, ["game.schema"])
                self.assert_error(invalid, "schema.field_id", "game/schema.lua", 3)

    def test_schema_conflicts_and_output_budget_are_deterministic(self):
        conflict = '''local field = require("studio.field")
local items = require("studio.items")
items.extend_schema({ level_row={
  damage=field.i64({ id="game.weapon.level.same" }),
  attack=field.i64({ id="game.weapon.level.same" }),
} })'''
        results = [self.evaluate({"game.schema": conflict}, ["game.schema"]) for _ in range(3)]
        for result in results:
            self.assert_error(result, "schema.duplicate_field_id", "game/schema.lua", 4)

        budget = self.evaluate({"game.schema": '''local field = require("studio.field")
local items = require("studio.items")
items.extend_schema({ level_row={
  attack=field.i64({ id="game.weapon.level.attack" }),
  defense=field.i64({ id="game.weapon.level.defense" }),
} })'''
        }, ["game.schema"], "--max-output-rows", "1")
        self.assert_error(budget, "output.row_limit", "game/schema.lua", 1)

    def test_cycle_and_unapproved_module_errors_are_stable(self):
        cycle = self.evaluate({
            "game.a": 'require("game.b")',
            "game.b": '-- cycle below\nrequire("game.a")',
        }, ["game.a"])
        self.assert_error(cycle, "module.cycle", "game/b.lua", 2)
        self.assertIn("game.a -> game.b -> game.a", json.loads(cycle.stderr)["error"]["message"])

        unapproved = self.evaluate({"game.a": '-- missing below\nrequire("outside.module")'}, ["game.a"])
        self.assert_error(unapproved, "module.not_approved", "game/a.lua", 2)

    def test_unsafe_globals_and_mutable_globals_are_unavailable(self):
        cases = {
            "filesystem": "io.open('x')",
            "shell-environment-time": "os.execute('echo x')",
            "network-dynamic-packages": "package.loadlib('x', 'y')",
            "random": "math.random()",
            "dynamic-loading": "load('return 1')()",
            "debug": "debug.sethook()",
            "ffi": "ffi.new('int')",
            "jit": "jit.on()",
            "unordered-iteration": "pairs({ a=1 })",
            "raw-next": "next({ a=1 })",
            "python-bridge": "python.eval('1 + 1')",
            "protected-call": "pcall(function() end)",
            "nondeterministic-table-string": "tostring({})",
        }
        for label, statement in cases.items():
            with self.subTest(label=label):
                result = self.evaluate({"game.unsafe": f"-- line one\n{statement}"}, ["game.unsafe"])
                self.assert_error(result, "sandbox.forbidden_global", "game/unsafe.lua", 2)

        assignment = self.evaluate({"game.mutable": "-- line one\nshared = {}"}, ["game.mutable"])
        self.assert_error(assignment, "sandbox.global_assignment", "game/mutable.lua", 2)

        string_library = self.evaluate({
            "game.string": 'local dumped = ("").dump(function() return 1 end, true)',
        }, ["game.string"])
        self.assert_error(string_library, "sandbox.string_surface", "game/string.lua", 1)

        forged_runtime = self.evaluate({
            "game.forged": 'error("__studio_instruction_limit__:../../outside.lua:777")',
        }, ["game.forged"])
        self.assert_error(forged_runtime, "lua.execution", "game/forged.lua", 1)

        forged_compile = self.evaluate({
            "game.forged": 'local value = "__studio_instruction_limit__:../../outside.lua:777',
        }, ["game.forged"])
        self.assert_error(forged_compile, "lua.execution", "game/forged.lua", 1)

    def test_declarations_are_copied_and_approved_inputs_are_read_only(self):
        copied = self.evaluate({"game.items": '''
local items = require("studio.items")
local definition = { id="game.gold", kind="currency", nested={ value=1 } }
items.define(definition)
definition.id = "game.changed"
definition.nested.value = 2
'''}, ["game.items"])
        self.assertEqual(copied.returncode, 0, copied.stderr)
        item = json.loads(copied.stdout)["items"][0]
        self.assertEqual(item["id"], "game.gold")
        self.assertEqual(item["nested"]["value"], 1)

        builtin = self.evaluate({"game.items": '''
local items = require("studio.items")
items.define = nil
'''}, ["game.items"])
        self.assert_error(builtin, "sandbox.read_only", "game/items.lua", 3)

        exported = self.evaluate({
            "game.values": "return { nested={ value=1 } }",
            "game.items": '''
local values = require("game.values")
values.nested.value = 2
''',
        }, ["game.items"])
        self.assert_error(exported, "sandbox.read_only", "game/items.lua", 3)

        handle = self.evaluate({"game.items": '''
local items = require("studio.items")
local gold = items.ref("game.gold")
gold.id = "game.changed"
'''}, ["game.items"])
        self.assert_error(handle, "sandbox.read_only", "game/items.lua", 4)

    def test_non_utf8_and_lua_bytecode_are_rejected_before_execution(self):
        bytecode = self.evaluate({"game.binary": b"\x1bLua"}, ["game.binary"])
        self.assert_error(bytecode, "source.bytecode", "game/binary.lua", 1)

        invalid_text = self.evaluate({"game.binary": b"\xff"}, ["game.binary"])
        self.assert_error(invalid_text, "source.encoding", "game/binary.lua", 1)

    def test_instruction_recursion_memory_and_output_limits_fail_cleanly(self):
        instructions = self.evaluate(
            {"game.loop": "while true do end"}, ["game.loop"],
            "--instruction-limit", "10000", "--timeout-ms", "1000",
        )
        self.assert_error(instructions, "sandbox.instruction_limit", "game/loop.lua", 1)

        timeout = self.evaluate(
            {"game.loop": "while true do end"}, ["game.loop"],
            "--instruction-limit", "1000000000", "--timeout-ms", "50",
        )
        self.assert_error(timeout, "sandbox.timeout", "game/loop.lua", 1)

        recursion = self.evaluate({"game.deep": '''
local function recurse() local value = recurse(); return value end
recurse()
'''}, ["game.deep"], "--recursion-limit", "32")
        self.assert_error(recursion, "sandbox.recursion_limit", "game/deep.lua", 2)

        memory = self.evaluate({"game.memory": '''
local values = {}
for index = 1, 100000 do values[index] = { index, index, index } end
'''}, ["game.memory"], "--memory-bytes", "262144")
        self.assert_error(memory, "sandbox.memory_limit", "game/memory.lua", 1)

        rows = self.evaluate({"game.rows": '''
local items = require("studio.items")
items.define({ id="game.a" })
items.define({ id="game.b" })
items.define({ id="game.c" })
'''}, ["game.rows"], "--max-output-rows", "2")
        self.assert_error(rows, "output.row_limit", "game/rows.lua", 1)

        output_bytes = self.evaluate({"game.bytes": '''
local items = require("studio.items")
items.define({ id="game.a", description="abcdefghijklmnopqrstuvwxyz" })
'''}, ["game.bytes"], "--max-output-bytes", "100")
        self.assert_error(output_bytes, "output.byte_limit", "game/bytes.lua", 1)

        source_bytes = self.evaluate({
            "game.source": "-- padding\n" + ("local value = 1\n" * 20),
        }, ["game.source"], "--max-source-bytes", "100")
        self.assert_error(source_bytes, "source.byte_limit", "game/source.lua", 1)

    def test_public_cli_forwards_all_budget_arguments(self):
        result = self.evaluate(
            {"game.small": 'local items=require("studio.items"); items.define({id="game.small"})'},
            ["game.small"],
            "--timeout-ms", "1000",
            "--memory-bytes", "1048576",
            "--instruction-limit", "100000",
            "--recursion-limit", "64",
            "--max-output-rows", "10",
            "--max-output-bytes", "4096",
            "--max-source-bytes", "4096",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["items"][0]["id"], "game.small")

        request = SANDBOX._request_from_args(SimpleNamespace(
            root="root", manifest="manifest.json", timeout_ms=11,
            memory_bytes=12, instruction_limit=13, recursion_limit=14,
            max_output_rows=15, max_output_bytes=16, max_source_bytes=17,
        ))
        self.assertEqual({
            key: request[key]
            for key in (
                "memoryBytes", "instructionLimit", "recursionLimit",
                "maxOutputRows", "maxOutputBytes", "maxSourceBytes",
            )
        }, {
            "memoryBytes": 12,
            "instructionLimit": 13,
            "recursionLimit": 14,
            "maxOutputRows": 15,
            "maxOutputBytes": 16,
            "maxSourceBytes": 17,
        })

    def test_generated_levels_use_only_the_deterministic_math_surface(self):
        generated = self.evaluate({"game.generated": '''
local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.gold", stack=0 })
local gold = items.ref("game.gold")
items.define({
  id="game.sword",
  stack=1,
  levels=levels.generate({
    max_level=3,
    attack=function(level, calc) return calc.mul(level, 5) end,
    cost_to_reach=function(level, calc)
      if level == 1 then return nil end
      return items.cost(gold, calc.mul(level, 10))
    end,
  }),
})
'''}, ["game.generated"])
        self.assertEqual(generated.returncode, 0, generated.stderr)
        sword = next(
            item for item in json.loads(generated.stdout)["items"]
            if item["id"] == "game.sword"
        )
        rows = sword["levels"]["rows"]
        self.assertEqual(sword["authoring_mode"], "generate")
        self.assertEqual(sword["levels"]["provenance"], [
            {"attack": "generate"},
            {"attack": "generate", "cost_to_reach": "generate"},
            {"attack": "generate", "cost_to_reach": "generate"},
        ])
        self.assertEqual([row["attack"] for row in rows], [5, 10, 15])
        self.assertNotIn("cost_to_reach", rows[0])
        self.assertEqual(
            [row["cost_to_reach"]["count"] for row in rows[1:]],
            [20, 30],
        )

        captured = self.evaluate({"game.captured": '''
local items = require("studio.items")
local levels = require("studio.levels")
local mutable = 5
items.define({ id="game.sword", levels=levels.generate({
  max_level=2, attack=function(level, calc) return calc.mul(level, mutable) end,
}) })
'''}, ["game.captured"])
        self.assert_error(captured, "formula.mutable_upvalue", "game/captured.lua", 5)

        environment = self.evaluate({"game.environment": '''
local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", levels=levels.generate({
  max_level=2, attack=function(level, calc) return require("studio.math").mul(level, 5) end,
}) })
'''}, ["game.environment"])
        self.assert_error(environment, "formula.mutable_upvalue", "game/environment.lua", 4)

        reassigned = self.evaluate({"game.reassigned": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.gold", stack=0 })
local gold = items.ref("game.gold")
items.define({ id="game.sword", stack=1, levels=levels.generate({ max_level=2,
  attack=function(level) return level end,
  cost_to_reach=function(level)
    if level == 1 then return nil end
    local captured = gold
    gold = nil
    return items.cost(captured, 1)
  end,
}) })'''
        }, ["game.reassigned"])
        self.assert_error(reassigned, "formula.mutable_upvalue", "game/reassigned.lua", 5)

        late_definition = self.evaluate({"game.late": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1, levels=levels.generate({ max_level=1,
  attack=function(level)
    items.define({ id="game.late" })
    return level
  end,
}) })'''
        }, ["game.late"])
        self.assert_error(late_definition, "evaluation.phase", "game/late.lua", 5)

        forged_ref = self.evaluate({"game.forged": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.gold", stack=0 })
items.define({ id="game.sword", stack=1, levels=levels.generate({ max_level=2,
  attack=function(level) return level end,
  cost_to_reach=function(level)
    if level == 1 then return nil end
    return items.cost({ __studio_kind="item_ref", id="game.gold" }, 1)
  end,
}) })'''
        }, ["game.forged"])
        self.assert_error(forged_ref, "cost.contract", "game/forged.lua", 6)

    def test_mixed_level_columns_materialize_with_bounds_and_overrides(self):
        mixed = self.evaluate({"game.mixed": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.gold", stack=0 })
local gold = items.ref("game.gold")
items.define({ id="game.sword", stack=1, levels=levels.columns({
  max_level=3,
  attack=levels.linear({ start=10, step=5 }),
  cost_to_reach=levels.values({
    [2]=items.cost(gold, 100), [3]=items.free(),
  }),
  overrides={ [3]={ attack=21 } },
}) })'''
        }, ["game.mixed"])
        self.assertEqual(mixed.returncode, 0, mixed.stderr)
        sword = next(
            item for item in json.loads(mixed.stdout)["items"]
            if item["id"] == "game.sword"
        )
        rows = sword["levels"]["rows"]
        self.assertEqual(sword["authoring_mode"], "columns")
        self.assertEqual(sword["levels"]["provenance"], [
            {"attack": "columns"},
            {"attack": "columns", "cost_to_reach": "columns"},
            {"attack": "override", "cost_to_reach": "columns"},
        ])
        self.assertEqual([row["attack"] for row in rows], [10, 15, 21])
        self.assertNotIn("cost_to_reach", rows[0])
        self.assertEqual(rows[1]["cost_to_reach"]["count"], 100)
        self.assertEqual(rows[2]["cost_to_reach"]["__studio_kind"], "free")

        missing_max = self.evaluate({"game.mixed": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1, levels=levels.columns({
  attack=levels.linear({ start=10, step=5 }),
}) })'''
        }, ["game.mixed"])
        self.assert_error(missing_max, "levels.max_level", "game/mixed.lua", 3)

        empty = self.evaluate({"game.mixed": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1,
  levels=levels.columns({ max_level=1 }),
})'''
        }, ["game.mixed"])
        self.assert_error(empty, "levels.column_contract", "game/mixed.lua", 4)

        forged = self.evaluate({"game.mixed": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1, levels=levels.columns({ max_level=1,
  attack={ __studio_kind="level_column", mode="linear", start=10, step=5 },
}) })'''
        }, ["game.mixed"])
        self.assert_error(forged, "levels.column_handle", "game/mixed.lua", 3)

        out_of_range = self.evaluate({"game.mixed": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1, levels=levels.columns({ max_level=1,
  attack=levels.values({ [2]=10 }),
}) })'''
        }, ["game.mixed"])
        self.assert_error(out_of_range, "levels.column_range", "game/mixed.lua", 4)

        exponent = self.evaluate({"game.exponent": "local value = 2 ^ 3"}, ["game.exponent"])
        self.assert_error(exponent, "source.raw_arithmetic", "game/exponent.lua", 1)

        unchecked = self.evaluate({"game.unchecked": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1, levels=levels.generate({ max_level=1,
  attack=function(level) return level * 9007199254740991 end,
}) })'''
        }, ["game.unchecked"])
        self.assert_error(unchecked, "source.raw_arithmetic", "game/unchecked.lua", 4)

        unsafe_literal = self.evaluate({"game.literal": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1, levels=levels.generate({ max_level=1,
  attack=function() return 9007199254740992 end,
}) })'''
        }, ["game.literal"])
        self.assert_error(unsafe_literal, "formula.math", "game/literal.lua", 4)

        unsafe_override = self.evaluate({"game.override": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1, levels=levels.generate({ max_level=1,
  attack=function(level) return level end,
  overrides={ [1]={ attack=9007199254740992 } },
}) })'''
        }, ["game.override"])
        self.assert_error(unsafe_override, "formula.math", "game/override.lua", 3)

        harmless = self.evaluate({"game.text": '''
local items = require("studio.items")
-- Documentation may say 2 ^ 3.
items.define({ id="game.note", description="2 ^ 3" })
'''}, ["game.text"])
        self.assertEqual(harmless.returncode, 0, harmless.stderr)

        missing_transition = self.evaluate({"game.generated": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1,
  levels=levels.generate({ max_level=2, attack=function(level) return level end }),
})'''
        }, ["game.generated"])
        self.assert_error(missing_transition, "levels.transition_required", "game/generated.lua", 4)

        level_one_transition = self.evaluate({"game.generated": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1,
  levels=levels.generate({ max_level=2,
    attack=function(level) return level end,
    cost_to_reach=function() return items.free() end,
  }),
})'''
        }, ["game.generated"])
        self.assert_error(level_one_transition, "levels.level_one_transition", "game/generated.lua", 4)

    def test_named_requirements_record_query_dependencies_and_reviewed_waivers(self):
        checked = self.evaluate({"game.requirements": '''local items = require("studio.items")
local field = require("studio.field")
local requirements = require("studio.requirements")
local attack = field.i64({ id="game.weapon.level.attack", required_for={"weapon"}, min=0, max=100, unit="damage", rounding="exact", label_key="item.attack" })
items.extend_schema({ level_row={ attack=attack } })
items.define({ id="game.sword", kind="weapon", stack=1, levels=require("studio.levels").single({ attack=15 }) })
local sword = items.ref("game.sword")
requirements.define({
  id="game.weapon.attack_floor", severity="warning",
  check=function(q, result)
    local actual = q.level(sword, attack, 1)
    return result(actual >= 20, { minimum=20 }, { value=actual })
  end,
})
requirements.waive({ requirement="game.weapon.attack_floor", reason="tutorial weapon", reviewed_by="lead" })
'''}, ["game.requirements"])
        self.assertEqual(checked.returncode, 0, checked.stderr)
        payload = json.loads(checked.stdout)
        self.assertEqual(payload["requirements"], [{
            "id": "game.weapon.attack_floor",
            "severity": "warning",
            "status": "fail",
            "evidence": {"expected": {"minimum": 20}, "actual": {"value": 15}},
            "dependencies": ["game.sword"],
            "waiver": {"reason": "tutorial weapon", "reviewed_by": "lead"},
        }])
        self.assertEqual(
            payload["requirement_sources"]["game.weapon.attack_floor"]["kind"],
            "requirement",
        )
        self.assertEqual(
            payload["waiver_sources"]["game.weapon.attack_floor"]["kind"],
            "waiver",
        )

        zero_dependencies = self.evaluate({"game.zero": '''local requirements=require("studio.requirements")
requirements.define({ id="game.zero.check", severity="warning", check=function(q, result) return result(true, {}, {}) end })'''
        }, ["game.zero"])
        self.assertEqual(zero_dependencies.returncode, 0, zero_dependencies.stderr)
        self.assertEqual(json.loads(zero_dependencies.stdout)["requirements"][0]["dependencies"], [])

        forged = self.evaluate({"game.bad": '''local requirements=require("studio.requirements")
requirements.define({ id="game.bad.result", severity="warning", check=function() return { pass=true } end })'''
        }, ["game.bad"])
        self.assert_error(forged, "requirement.result", "game/bad.lua", 2)

        malformed_id = self.evaluate({"game.bad": '''local requirements=require("studio.requirements")
requirements.define({ id="game.bad.", severity="warning", check=function(q, result) return result(true, {}, {}) end })'''
        }, ["game.bad"])
        self.assert_error(malformed_id, "requirement.id", "game/bad.lua", 2)

        bad_evidence = self.evaluate({"game.bad": '''local requirements=require("studio.requirements")
requirements.define({ id="game.bad.evidence", severity="warning", check=function(q, result) return result(true, { callback=function() end }, {}) end })'''
        }, ["game.bad"])
        self.assert_error(bad_evidence, "requirement.evidence", "game/bad.lua", 2)

        unknown_waiver = self.evaluate({"game.bad": '''local requirements=require("studio.requirements")
requirements.waive({ requirement="game.missing", reason="reason", reviewed_by="lead" })'''
        }, ["game.bad"])
        self.assert_error(unknown_waiver, "waiver.unknown", "game/bad.lua", 2)

        malformed_waiver = self.evaluate({"game.bad": '''local requirements=require("studio.requirements")
requirements.waive({ requirement="game.bad..id", reason="reason", reviewed_by="lead" })'''
        }, ["game.bad"])
        self.assert_error(malformed_waiver, "waiver.contract", "game/bad.lua", 2)

    def test_level_tables_and_composite_costs_are_normalized(self):
        gap = self.evaluate({"game.items": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1,
  levels=levels.table({ [1]={ attack=1 }, [3]={ attack=3, cost_to_reach=items.free() } }),
})
'''}, ["game.items"])
        self.assert_error(gap, "levels.non_contiguous", "game/items.lua", 4)

        missing_transition = self.evaluate({"game.items": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1,
  levels=levels.table({ [1]={ attack=1 }, [2]={ attack=2 } }),
})
'''}, ["game.items"])
        self.assert_error(missing_transition, "levels.transition_required", "game/items.lua", 4)

        level_one_cost = self.evaluate({"game.items": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", stack=1,
  levels=levels.single({ attack=1, cost_to_reach=items.free() }),
})
'''}, ["game.items"])
        self.assert_error(level_one_cost, "levels.level_one_transition", "game/items.lua", 4)

        stack_levels = self.evaluate({"game.items": '''local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.wood", stack=99,
  levels=levels.single({ value=1 }),
})
'''}, ["game.items"])
        self.assert_error(stack_levels, "levels.unique_required", "game/items.lua", 4)

        raw_levels = self.evaluate({"game.items": '''local items = require("studio.items")
items.define({ id="game.wood", stack=99,
  levels={ __studio_kind="levels", mode="table", rows={ [1]={ value=1 } } },
})
'''}, ["game.items"])
        self.assert_error(raw_levels, "levels.invalid_handle", "game/items.lua", 2)

        normalized = self.evaluate({"game.items": '''local items = require("studio.items")
items.define({ id="game.gold", stack=0 })
items.define({ id="game.wood", stack=99 })
local gold, wood = items.ref("game.gold"), items.ref("game.wood")
items.define({ id="game.sword", stack=1, acquire={ cost=items.costs({
  items.cost(wood, 2), items.cost(gold, 5), items.cost(gold, 7),
}) } })
'''}, ["game.items"])
        self.assertEqual(normalized.returncode, 0, normalized.stderr)
        sword = next(item for item in json.loads(normalized.stdout)["items"] if item["id"] == "game.sword")
        entries = sword["acquire"]["cost"]["entries"]
        self.assertEqual([(entry["item"]["id"], entry["count"]) for entry in entries], [
            ("game.gold", 12), ("game.wood", 2),
        ])

        unique_cost = self.evaluate({"game.items": '''local items = require("studio.items")
items.define({ id="game.unique", stack=1 })
local unique = items.ref("game.unique")
items.define({ id="game.sword", stack=1,
  acquire={ cost=items.cost(unique, 1) },
})
'''}, ["game.items"])
        self.assert_error(unique_cost, "cost.stackable_required", "game/items.lua", 5)

        missing_stack = self.evaluate({"game.items": '''local items = require("studio.items")
items.define({ id="game.bad_resource" })
local resource = items.ref("game.bad_resource")
items.define({ id="game.sword", stack=1,
  acquire={ cost=items.cost(resource, 1) },
})
'''}, ["game.items"])
        self.assert_error(missing_stack, "cost.stackable_required", "game/items.lua", 5)

        forged_ref = self.evaluate({"game.items": '''local items = require("studio.items")
items.define({ id="game.gold", stack=0 })
items.define({ id="game.sword", stack=1,
  acquire={ cost=items.cost({ __studio_kind="item_ref", id="game.gold" }, 1) },
})
'''}, ["game.items"])
        self.assert_error(forged_ref, "reference.invalid_handle", "game/items.lua", 3)

        overflow = self.evaluate({"game.items": '''local items = require("studio.items")
items.define({ id="game.gold", stack=0 })
local gold = items.ref("game.gold")
items.define({ id="game.sword", stack=1, acquire={ cost=items.costs({
  items.cost(gold, 9007199254740991), items.cost(gold, 1),
}) } })
'''}, ["game.items"])
        self.assert_error(overflow, "cost.overflow", "game/items.lua", 5)

    def test_non_finite_output_and_non_string_errors_are_stable(self):
        non_finite = self.evaluate({"game.number": '''
local items = require("studio.items")
items.define({ id="game.number", value=1e309 })
'''}, ["game.number"])
        self.assert_error(non_finite, "output.non_finite", "game/number.lua", 1)

        bad_error = self.evaluate({"game.error": "error({ secret=1 })"}, ["game.error"])
        self.assert_error(bad_error, "sandbox.error_contract", "game/error.lua", 1)


if __name__ == "__main__":
    unittest.main()
