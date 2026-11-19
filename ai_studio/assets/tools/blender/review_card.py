"""Compose one review card per asset: views x passes, numbers, textures, check findings, game crop.

    python review_card.py renders/views.json --report mesh.json --report glb.json --out card.png

Runs in the Studio venv (needs Pillow), not inside Blender. `review.py` calls it for you.
"""
import argparse
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

FONT_DIR = Path(__file__).resolve().parents[2] / "canvas" / "site" / "fonts"
BACKGROUND = (24, 26, 30)
PANEL = (34, 37, 43)
CHECKER = ((46, 49, 56), (40, 43, 49))
TEXT = (232, 234, 238)
MUTED = (150, 156, 166)
SEVERITY_COLOUR = {"high": (235, 87, 87), "medium": (242, 166, 64), "low": (120, 170, 235), "info": MUTED}
PASS_ORDER = ("beauty", "clay", "wire", "silhouette")
VIEW_ORDER = ("front", "back", "left", "right", "top", "three_quarter", "game")


def font(size, bold=False):
    path = FONT_DIR / "Inter" / ("Inter-SemiBold.ttf" if bold else "Inter-Regular.ttf")
    try:
        return ImageFont.truetype(str(path), size)
    except OSError:
        return ImageFont.load_default()


def checkerboard(size, step=12):
    tile = Image.new("RGB", size, CHECKER[0])
    draw = ImageDraw.Draw(tile)
    for y in range(0, size[1], step):
        for x in range(0, size[0], step):
            if (x // step + y // step) % 2:
                draw.rectangle([x, y, x + step - 1, y + step - 1], fill=CHECKER[1])
    return tile


def fit(image, box):
    image = image.copy()
    image.thumbnail(box, Image.LANCZOS)
    return image


def paste_on_cell(card, image_path, origin, cell):
    base = checkerboard(cell)
    if image_path and Path(image_path).is_file():
        with Image.open(image_path) as source:
            image = fit(source.convert("RGBA"), cell)
        offset = ((cell[0] - image.width) // 2, (cell[1] - image.height) // 2)
        base.paste(image, offset, image)
    card.paste(base, origin)


def describe_size(bounds):
    if not bounds:
        return ""
    size = [hi - lo for lo, hi in zip(bounds["min"], bounds["max"])]
    return "size {:.3f} x {:.3f} x {:.3f} m (x, y up, z)".format(*size)


def wrap(draw, text, width, typeface):
    words, lines, line = text.split(), [], ""
    for word in words:
        trial = f"{line} {word}".strip()
        if draw.textlength(trial, font=typeface) <= width or not line:
            line = trial
        else:
            lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


def compose(views, reports=(), out=None, game_crop=None, title=None, cell=320, max_findings=14):
    """views: views.json dict (or path); reports: check report dicts. Returns the card path."""
    if not isinstance(views, dict):
        views_path = Path(views)
        views = json.loads(views_path.read_text(encoding="utf-8"))
        base_dir = views_path.parent
    else:
        base_dir = Path(views.get("out_dir", "."))
    images = views.get("images", [])
    present_views = [v for v in VIEW_ORDER if any(i["view"] == v for i in images)]
    present_views += sorted({i["view"] for i in images} - set(present_views) - {"turntable"})
    present_passes = [p for p in PASS_ORDER if any(i["pass"] == p for i in images)]
    present_passes += sorted({i["pass"] for i in images} - set(present_passes))
    lookup = {(i["view"], i["pass"]): base_dir / i["file"] for i in images}
    turntable = [base_dir / i["file"] for i in images if i["view"].startswith("turntable")]

    stats = views.get("stats", {})
    textures = stats.get("textures", [])
    margin, label_h, header_h = 16, 26, 112
    cols = max(1, len(present_views))
    grid_w = cols * cell + (cols - 1) * 8
    side_w = 420
    width = margin * 3 + grid_w + side_w
    grid_h = len(present_passes) * (cell + label_h + 8)
    strip_h = (cell // 2 + label_h + 8) if turntable or textures else 0
    height = header_h + grid_h + strip_h + margin * 2
    card = Image.new("RGB", (width, max(height, header_h + 480)), BACKGROUND)
    draw = ImageDraw.Draw(card)

    name = title or Path(views.get("asset", "asset")).name
    draw.text((margin, margin), name, fill=TEXT, font=font(26, True))
    facts = [f"{stats.get('triangles', '?')} tris", f"{stats.get('vertices', '?')} verts",
             f"{len(stats.get('materials', []))} materials", f"{len(textures)} textures"]
    if stats.get("armatures"):
        count = len(stats["armatures"]) if isinstance(stats["armatures"], list) else stats["armatures"]
        facts.append(f"{count} armature" + ("s" if count != 1 else ""))
    if stats.get("animations"):
        facts.append(f"{len(stats['animations'])} clips")
    draw.text((margin, margin + 38), "   ".join(facts), fill=TEXT, font=font(17))
    sub = [describe_size(stats.get("bounds")), f"Blender {views.get('blender', '?')}",
           f"sha {str(views.get('sha256', ''))[:12]}"]
    draw.text((margin, margin + 64), "   ".join(s for s in sub if s), fill=MUTED, font=font(15))
    if views.get("error"):
        draw.text((margin, header_h - 22), f"not rendered: {views['error']}", fill=SEVERITY_COLOUR["high"],
                  font=font(15))

    y = header_h
    for render_pass in present_passes:
        for c, view in enumerate(present_views):
            x = margin + c * (cell + 8)
            draw.text((x, y), f"{view} · {render_pass}", fill=MUTED, font=font(14))
            paste_on_cell(card, lookup.get((view, render_pass)), (x, y + label_h - 6), (cell, cell))
        y += cell + label_h + 8

    if strip_h:
        small = cell // 2
        x = margin
        items = [(p, p.stem) for p in turntable] + [(base_dir / t["file"], f"tex {t.get('name', '')} "
                                                     f"{'x'.join(map(str, t.get('size', [])))}")
                                                    for t in textures if t.get("file")]
        for path, label in items:
            if x + small > margin + grid_w:
                break
            draw.text((x, y), label[:24], fill=MUTED, font=font(12))
            paste_on_cell(card, path, (x, y + label_h - 6), (small, small))
            x += small + 8

    sx, sy = margin * 2 + grid_w, header_h
    if game_crop and Path(game_crop).is_file():
        draw.text((sx, sy), "in game", fill=MUTED, font=font(14))
        with Image.open(game_crop) as source:
            crop = fit(source.convert("RGB"), (side_w, side_w))
        card.paste(crop, (sx, sy + label_h - 6))
        sy += crop.height + label_h + 8
    draw.rectangle([sx, sy, sx + side_w, card.height - margin], fill=PANEL)
    ty = sy + 12
    draw.text((sx + 12, ty), "Checks", fill=TEXT, font=font(18, True))
    ty += 30
    findings = []
    for report in reports:
        counts = report.get("counts", {})
        verdict = report.get("status", "?")
        colour = SEVERITY_COLOUR["high"] if counts.get("high") else (
            SEVERITY_COLOUR["medium"] if counts.get("medium") else (110, 200, 140))
        line = f"{report.get('tool', '?')}: {verdict}"
        if verdict != "clean":
            line += f"  (H{counts.get('high', 0)} M{counts.get('medium', 0)} L{counts.get('low', 0)})"
        draw.text((sx + 12, ty), line, fill=colour, font=font(15))
        ty += 22
        findings += [dict(f, tool=report.get("tool")) for f in report.get("findings", [])
                     if f["severity"] != "info"]
    ty += 8
    order = {"high": 0, "medium": 1, "low": 2}
    for f in sorted(findings, key=lambda f: order.get(f["severity"], 3))[:max_findings]:
        for k, line in enumerate(wrap(draw, f"{f['check']}: {f['message']}", side_w - 40, font(13))[:3]):
            draw.text((sx + 12, ty), "●" if k == 0 else " ", fill=SEVERITY_COLOUR[f["severity"]], font=font(13))
            draw.text((sx + 28, ty), line, fill=TEXT, font=font(13))
            ty += 18
        ty += 4
        if ty > card.height - margin - 40:
            break
    hidden = len(findings) - max_findings
    if hidden > 0:
        draw.text((sx + 12, ty), f"+{hidden} more in the JSON reports", fill=MUTED, font=font(13))

    out = Path(out or base_dir / "card.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    card.save(out)
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("views", help="views.json written by render_views.py")
    parser.add_argument("--report", action="append", default=[], help="check report JSON (repeatable)")
    parser.add_argument("--game-crop", help="screenshot crop of the asset in the game")
    parser.add_argument("--title")
    parser.add_argument("--cell", type=int, default=320)
    parser.add_argument("--out")
    args = parser.parse_args(argv)
    reports = []
    for path in args.report:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        reports += data if isinstance(data, list) else [data]
    print(compose(args.views, reports, args.out, args.game_crop, args.title, args.cell))
    return 0


if __name__ == "__main__":
    sys.exit(main())
