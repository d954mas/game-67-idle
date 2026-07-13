import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import generate_state
from state_codegen.model import build_model
from state_codegen.naming import Ns, c_ident, c_macro, provenance_label
from state_codegen.output import default_out_dir, render_bundle, write_bundle, write_if_changed
from state_codegen.render_events import EventRenderer
from state_codegen.render_state import StateRenderer
from state_codegen.schema import load_schema


ROOT = Path(__file__).resolve().parents[3]
MINI_SCHEMA = ROOT / "features" / "game-state" / "tests" / "mini_state.schema.json"
GENERATOR = SCRIPT_DIR / "generate_state.py"


class StateModuleTests(unittest.TestCase):
    def test_naming_owns_fragment_namespace(self):
        ns = Ns("meta_progression")
        self.assertEqual(ns.type, "MetaProgressionState")
        self.assertEqual(ns.fn, "meta_progression_state_")
        self.assertEqual(c_ident("HeroXP"), "hero_xp")
        self.assertEqual(c_macro("HeroXP"), "HERO_XP")

    def test_schema_and_model_own_explicit_namespace(self):
        schema = load_schema(MINI_SCHEMA)
        model = build_model(schema, provenance_label(MINI_SCHEMA, ROOT))
        self.assertEqual(model.ns, Ns("mini"))
        self.assertEqual(model.schema["fragment"], "mini")

    def test_schema_module_rejects_missing_v2_contract(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "invalid.schema.json"
            path.write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(SystemExit, "v1 schema unsupported"):
                load_schema(path)

    def test_default_output_derives_only_from_explicit_schema(self):
        self.assertEqual(
            default_out_dir(MINI_SCHEMA, ROOT),
            ROOT / "features" / "game-state" / "build" / "generated" / "game-state",
        )

    def test_external_provenance_is_machine_reproducible(self):
        with tempfile.TemporaryDirectory() as tmp:
            schema = Path(tmp) / "private-game" / "state" / "hero.schema.json"
            schema.parent.mkdir(parents=True)
            schema.write_text(json.dumps({}), encoding="utf-8")
            self.assertEqual(provenance_label(schema, ROOT), "state/hero.schema.json")

    def test_in_repo_project_provenance_is_project_relative(self):
        self.assertEqual(
            provenance_label(ROOT / "templates/template/state/game_state.schema.json", ROOT),
            "state/game_state.schema.json",
        )
        self.assertEqual(
            provenance_label(ROOT / "games/web-dressup/state/game_state.schema.json", ROOT),
            "state/game_state.schema.json",
        )

    def test_state_and_event_renderers_use_model_namespace(self):
        schema = load_schema(MINI_SCHEMA)
        label = provenance_label(MINI_SCHEMA, ROOT)
        state_header = StateRenderer(Ns("mini")).render_header(schema, label)
        events_header = EventRenderer(Ns("mini")).render_events_header(schema, label)
        self.assertIn("typedef struct MiniState", state_header)
        self.assertIn("typedef struct MiniEvCellSpawned", events_header)
        self.assertNotIn("GameState", state_header)

    def test_write_if_changed_preserves_unchanged_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "out.c"
            self.assertTrue(write_if_changed(path, "same\n"))
            before = path.stat().st_mtime_ns
            self.assertFalse(write_if_changed(path, "same\n"))
            self.assertEqual(path.stat().st_mtime_ns, before)

    def test_bundle_output_is_byte_identical_across_api_and_cli(self):
        schema = load_schema(MINI_SCHEMA)
        model = build_model(schema, provenance_label(MINI_SCHEMA, ROOT))
        expected = render_bundle(model)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            api_dir = root / "api"
            cli_dir = root / "cli"
            write_bundle(model, api_dir)
            result = subprocess.run(
                [sys.executable, str(GENERATOR), "--schema", str(MINI_SCHEMA), "--out-dir", str(cli_dir)],
                cwd=ROOT,
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            for name, text in expected.items():
                api_bytes = (api_dir / name).read_bytes()
                cli_bytes = (cli_dir / name).read_bytes()
                self.assertEqual(api_bytes, text.encode("utf-8"))
                self.assertEqual(cli_bytes, api_bytes)

    def test_cli_requires_schema(self):
        result = subprocess.run(
            [sys.executable, str(GENERATOR)],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--schema", result.stderr)


if __name__ == "__main__":
    unittest.main()
