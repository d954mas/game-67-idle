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


class StateCodegenTests(unittest.TestCase):
    def test_clean_schema_excludes_closed_prototype_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            text = generate_variant("game_state.schema.json", Path(tmp) / "clean")

        for token in CLOSED_TOKENS:
            self.assertNotIn(token, text)

    def test_clean_schema_generates_all_outputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "generated"
            # generate_variant already asserts every expected file exists.
            generate_variant("game_state.schema.json", out_dir)
            self.assertTrue((out_dir / "game_state.h").exists())
            self.assertTrue((out_dir / "game_state.c").exists())
            self.assertTrue((out_dir / "game_state_devapi.c").exists())
            self.assertTrue((out_dir / "game_state_schema.gen.h").exists())


if __name__ == "__main__":
    unittest.main()
