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
                "kind": "definition",
            },
        })

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
local function recurse() return 1 + recurse() end
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
items.define({
  id="game.sword",
  levels=levels.generate({
    max_level=3,
    row=function(level, calc) return { attack=calc.mul(level, 5) } end,
  }),
})
'''}, ["game.generated"])
        self.assertEqual(generated.returncode, 0, generated.stderr)
        rows = json.loads(generated.stdout)["items"][0]["levels"]["rows"]
        self.assertEqual([row["attack"] for row in rows], [5, 10, 15])

        captured = self.evaluate({"game.captured": '''
local items = require("studio.items")
local levels = require("studio.levels")
local mutable = 5
items.define({ id="game.sword", levels=levels.generate({
  max_level=2, row=function(level, calc) return calc.mul(level, mutable) end,
}) })
'''}, ["game.captured"])
        self.assert_error(captured, "formula.mutable_upvalue", "game/captured.lua", 5)

        environment = self.evaluate({"game.environment": '''
local items = require("studio.items")
local levels = require("studio.levels")
items.define({ id="game.sword", levels=levels.generate({
  max_level=2, row=function(level, calc) return require("studio.math").mul(level, 5) end,
}) })
'''}, ["game.environment"])
        self.assert_error(environment, "formula.mutable_upvalue", "game/environment.lua", 4)

        exponent = self.evaluate({"game.exponent": "local value = 2 ^ 3"}, ["game.exponent"])
        self.assert_error(exponent, "source.raw_exponentiation", "game/exponent.lua", 1)

        harmless = self.evaluate({"game.text": '''
local items = require("studio.items")
-- Documentation may say 2 ^ 3.
items.define({ id="game.note", description="2 ^ 3" })
'''}, ["game.text"])
        self.assertEqual(harmless.returncode, 0, harmless.stderr)

    def test_non_finite_output_and_non_string_errors_are_stable(self):
        non_finite = self.evaluate({"game.number": '''
local items = require("studio.items")
items.define({ id="game.number", value=1/0 })
'''}, ["game.number"])
        self.assert_error(non_finite, "output.non_finite", "game/number.lua", 1)

        bad_error = self.evaluate({"game.error": "error({ secret=1 })"}, ["game.error"])
        self.assert_error(bad_error, "sandbox.error_contract", "game/error.lua", 1)


if __name__ == "__main__":
    unittest.main()
