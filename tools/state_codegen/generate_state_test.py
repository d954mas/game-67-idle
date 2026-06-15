import tempfile
import unittest
from pathlib import Path

from tools.state_codegen import generate_state


ROOT = Path(__file__).resolve().parents[2]
CLOSED_TOKENS = ("rune_", "fishing_", "Rune", "Fishing", "rune.", "fishing.")


def generate_variant(schema_name: str, out_dir: Path) -> str:
    rc = generate_state.main(
        [
            "--schema",
            str(ROOT / "state" / schema_name),
            "--out-dir",
            str(out_dir),
        ]
    )
    if rc != 0:
        raise AssertionError(f"generator returned {rc}")
    generated = []
    for name in (
        "game_state.h",
        "game_state.c",
        "game_state_devapi.c",
        "game_state_schema.gen.h",
    ):
        path = out_dir / name
        if not path.exists():
            raise AssertionError(f"missing generated file: {path}")
        generated.append(path.read_text(encoding="utf-8"))
    return "\n".join(generated)


class StateVariantCodegenTests(unittest.TestCase):
    def test_clean_variant_excludes_closed_prototype_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            text = generate_variant("game_state.schema.json", Path(tmp) / "clean")

        for token in CLOSED_TOKENS:
            self.assertNotIn(token, text)

    def test_closed_variant_preserves_legacy_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            text = generate_variant("closed_prototypes_game_state.schema.json", Path(tmp) / "closed")

        self.assertIn("GameStateRuneLocation", text)
        self.assertIn("GAME_STATE_FISHING_PHASE_DEFAULT", text)
        self.assertIn('"rune.location"', text)
        self.assertIn('"fishing.phase"', text)

    def test_variants_do_not_share_output_directories(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            clean_dir = root / "clean" / "generated"
            closed_dir = root / "closed" / "generated"
            generate_variant("game_state.schema.json", clean_dir)
            generate_variant("closed_prototypes_game_state.schema.json", closed_dir)

            self.assertTrue((clean_dir / "game_state.h").exists())
            self.assertTrue((closed_dir / "game_state.h").exists())
            self.assertNotEqual(
                (clean_dir / "game_state.h").read_text(encoding="utf-8"),
                (closed_dir / "game_state.h").read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
