#!/usr/bin/env python3
"""Build one quadratic face for the CJK languages from their OTFs, holding only what their copy uses.

Run it after copy in one of those languages changes. A character takes its outline from the first source
that has it, so the order settles the few codepoints Japanese and Simplified Chinese draw differently.
Writes the face, its charset and a provenance manifest into --out-dir; its metrics come from
--metrics-from, the common face it shares one nt_font with.

    python gen_cjk_fonts.py --loc-dir content/loc --out-dir assets/fonts/cjk --family "Dig CJK"
        --metrics-from <common face> --source-dir <dir> --source-root <logical dir>
        --face ja=NotoSansJP-Black.otf --face zh=NotoSansSC-Black.otf --face ko=NotoSansKR-Black.otf
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import fontTools
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

def texts(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for entry in value.values():
            yield from texts(entry)


def copy_characters(language: str, loc_dir: Path) -> set[str]:
    found: set[str] = set()
    for path in sorted(loc_dir.glob("*.json")):
        for key, entry in json.loads(path.read_text(encoding="utf-8"))["strings"].items():
            # An endonym reads the same in every language; only its own language draws it here.
            if language in entry and (not key.startswith("settings.lang_") or key.endswith(f"_{language}")):
                found.update(char for text in texts(entry[language]) for char in text if ord(char) >= 0x2E80)
    return found


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(faces: list[tuple[str, Path]], args, common: TTFont) -> dict:
    fonts = [TTFont(source) for _, source in faces]
    upm, hhea = common["head"].unitsPerEm, common["hhea"]
    ascent, descent, line_gap = hhea.ascent, hhea.descent, hhea.lineGap
    for font, (_, source) in zip(fonts, faces):
        if font["head"].unitsPerEm != upm:
            raise SystemExit(f"{source.name} has {font['head'].unitsPerEm} units per em, the common face {upm}")
    wanted = set().union(*(copy_characters(language, args.loc_dir) for language, _ in faces))
    glyphs = {".notdef": TTGlyphPen(None).glyph()}
    metrics = {".notdef": (upm, 0)}
    cmap = {}
    maps = [font.getBestCmap() for font in fonts]
    for char in sorted(wanted):
        holder = next((index for index, table in enumerate(maps) if ord(char) in table), None)
        if holder is None:
            raise SystemExit(f"no source has {char} U+{ord(char):04X}")
        name = maps[holder][ord(char)]
        pen = TTGlyphPen(None)
        # The engine stores only lines and quadratic contours.
        fonts[holder].getGlyphSet()[name].draw(Cu2QuPen(pen, max_err=1.0, reverse_direction=True))
        glyph_name = f"u{ord(char):05X}"
        glyphs[glyph_name] = pen.glyph()
        metrics[glyph_name] = fonts[holder]["hmtx"][name]
        cmap[ord(char)] = glyph_name
    builder = FontBuilder(upm, isTTF=True)
    builder.setupGlyphOrder(list(glyphs))
    builder.setupCharacterMap(cmap)
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=ascent, descent=descent, lineGap=line_gap)
    family = args.family
    builder.setupNameTable({"familyName": family, "styleName": "Regular", "uniqueFontIdentifier": f"{family} 1.0",
        "fullName": family, "psName": family.replace(" ", ""), "version": "Version 1.0",
        "copyright": fonts[0]["name"].getDebugName(0), "licenseDescription": "SIL Open Font License 1.1",
        "licenseInfoURL": "https://openfontlicense.org/"})
    builder.setupOS2(sTypoAscender=ascent, sTypoDescender=descent, sTypoLineGap=line_gap, usWinAscent=ascent,
        usWinDescent=-descent, usWeightClass=fonts[0]["OS/2"].usWeightClass)
    builder.setupPost()
    builder.setupMaxp()
    builder.font["head"].created = 2082844800
    builder.font["head"].modified = 2082844800
    builder.font.recalcTimestamp = False
    target = args.out_dir / f"{args.family.replace(' ', '')}.ttf"
    builder.save(target)
    charset = args.out_dir / "charset.txt"
    charset.write_text("".join(sorted(wanted)), encoding="utf-8", newline="")
    print(f"{target.name}: {len(wanted)} codepoints, {target.stat().st_size} bytes")
    return {"file": target.name, "languages": [language for language, _ in faces], "sha256": sha256(target),
        "bytes": target.stat().st_size, "codepoints": len(wanted), "charset": charset.name, "origin": "derived", "license": "OFL-1.1",
        "sources": [{"logical_path": f"{args.source_root}/{source.name}", "family": font["name"].getDebugName(1),
            "version": font["name"].getDebugName(5), "bytes": source.stat().st_size, "sha256": sha256(source)}
            for font, (_, source) in zip(fonts, faces)]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--loc-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--family", required=True)
    parser.add_argument("--metrics-from", type=Path, required=True)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--source-root", required=True, help="where the sources are kept, for the manifest")
    parser.add_argument("--face", action="append", required=True, metavar="LANGUAGE=FILE")
    parser.add_argument("--upstream", default="https://github.com/notofonts/noto-cjk")
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    common = TTFont(args.metrics_from)
    faces = [(face.partition("=")[0], args.source_dir / face.partition("=")[2]) for face in args.face]
    assets = [build(faces, args, common)]
    manifest = {"schema": "ai_studio.localization.cjk_fonts.v1", "origin": "derived", "license": "OFL-1.1", "license_file": "OFL.txt",
        "upstream": args.upstream, "tool": "features/localization/scripts/gen_cjk_fonts.py",
        "fonttools_version": fontTools.__version__,
        "conversion": "CFF to quadratic glyf, Cu2Qu max_err=1 font unit; the common face's metrics; renamed subset",
        "assets": assets}
    (args.out_dir / "provenance.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
