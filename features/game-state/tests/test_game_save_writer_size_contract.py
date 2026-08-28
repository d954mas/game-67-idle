from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[3]
WRITER_SOURCE = ROOT / "features/game-state/src/game_save_writer.c"


class GameSaveWriterSizeContractTests(unittest.TestCase):
    def test_writer_does_not_link_formatted_input(self) -> None:
        source = WRITER_SOURCE.read_text(encoding="utf-8")
        self.assertIsNone(re.search(r"\b(?:f|s|v)?scanf\s*\(", source))


if __name__ == "__main__":
    unittest.main()
