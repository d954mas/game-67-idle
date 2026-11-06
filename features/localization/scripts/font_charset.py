#!/usr/bin/env python3
"""The common face's charset: printable ASCII plus the corpus, without the CJK that lives in a language's own pack.

Endonyms stay: a language list has to name a language before its pack is here.

  python font_charset.py --strings <merged strings.json> --out <file> [--endonym-prefix settings.lang_]

Without --split-cjk the whole corpus is written, for a game with one atlas.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import loc  # noqa: E402


def common_codepoints(table: "loc.Table", endonym_prefix: str, split_cjk: bool) -> set[int]:
    corpus = loc.corpus_codepoints(table)
    if not split_cjk:
        return set(range(0x20, 0x7F)) | set(corpus)
    endonyms = {ord(char) for entry in table.entries if entry.key.startswith(endonym_prefix)
                for forms in entry.forms.values() for text in forms.values() for char in text}
    return set(range(0x20, 0x7F)) | {cp for cp in corpus if not loc.is_cjk(cp) or cp in endonyms}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strings", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--split-cjk", action="store_true")
    parser.add_argument("--endonym-prefix", default="settings.lang_")
    args = parser.parse_args()
    codepoints = common_codepoints(loc.load_table(Path(args.strings)), args.endonym_prefix, args.split_cjk)
    text = "".join(chr(cp) for cp in sorted(codepoints))
    out = Path(args.out)
    # Written, or touched when unchanged, on every run: a build compares the
    # output's time with its inputs', so an untouched output reruns forever.
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.read_text(encoding="utf-8") != text:
        out.write_text(text, encoding="utf-8", newline="")
    else:
        out.touch()
    return 0


if __name__ == "__main__":
    sys.exit(main())
