from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFont

from atomic_io import save_image_atomic, write_json_atomic, write_text_atomic


BACKGROUND_SWATCHES = [
    ("transparent_grid", (32, 32, 36), "checker"),
    ("mine_dark_panel", (22, 18, 22), "solid"),
    ("mine_warm_panel", (87, 66, 45), "solid"),
    ("light_inventory_slot", (198, 180, 142), "solid"),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Render a sprite asset manifest over multiple backgrounds and report "
            "dark translucent edge pixels that can read as baked shadows."
        )
    )
    parser.add_argument("--asset-manifest", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    parser.add_argument("--cell-size", type=int, default=128)
    return parser.parse_args()


def resolve_path(root: Path, value: str) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return root / path


def checker(size: tuple[int, int], color: tuple[int, int, int]) -> Image.Image:
    image = Image.new("RGB", size, color)
    draw = ImageDraw.Draw(image)
    alt = tuple(min(255, c + 34) for c in color)
    step = 16
    for y in range(0, size[1], step):
        for x in range(0, size[0], step):
            if ((x // step) + (y // step)) % 2 == 0:
                draw.rectangle((x, y, x + step - 1, y + step - 1), fill=alt)
    return image


def fit_sprite(sprite: Image.Image, max_size: int) -> Image.Image:
    width, height = sprite.size
    scale = min(max_size / max(width, 1), max_size / max(height, 1), 1.0)
    output_size = (max(1, round(width * scale)), max(1, round(height * scale)))
    if output_size == sprite.size:
        return sprite.copy()
    return sprite.resize(output_size, Image.Resampling.LANCZOS)


def luminance(pixel: tuple[int, int, int, int]) -> float:
    return (0.2126 * pixel[0]) + (0.7152 * pixel[1]) + (0.0722 * pixel[2])


def alpha_shadow_metrics(sprite: Image.Image) -> dict[str, Any]:
    rgba = sprite.convert("RGBA")
    pixels = rgba.load()
    width, height = rgba.size
    visible = 0
    translucent = 0
    dark_translucent = 0
    very_dark_translucent = 0
    for y in range(height):
        for x in range(width):
            pixel = pixels[x, y]
            alpha = pixel[3]
            if alpha == 0:
                continue
            visible += 1
            if alpha < 224:
                translucent += 1
                lum = luminance(pixel)
                if lum < 48:
                    dark_translucent += 1
                if lum < 28:
                    very_dark_translucent += 1

    ratio = dark_translucent / visible if visible else 0.0
    verdict = "pass"
    if ratio > 0.030 or very_dark_translucent > 160:
        verdict = "review"

    return {
        "visible_pixels": visible,
        "translucent_pixels": translucent,
        "dark_translucent_pixels": dark_translucent,
        "very_dark_translucent_pixels": very_dark_translucent,
        "dark_translucent_ratio": round(ratio, 5),
        "verdict": verdict,
    }


def draw_label(draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, font: ImageFont.ImageFont) -> None:
    x, y = xy
    bbox = draw.textbbox((x, y), text, font=font)
    draw.rectangle((bbox[0] - 4, bbox[1] - 2, bbox[2] + 4, bbox[3] + 2), fill=(18, 17, 18))
    draw.text((x, y), text, fill=(238, 232, 214), font=font)


def main() -> int:
    args = parse_args()
    root = Path.cwd()
    manifest_path = resolve_path(root, args.asset_manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assets = [asset for asset in manifest.get("assets", []) if asset.get("kind") in {"sprite", "icon"}]
    if not assets:
        raise SystemExit("manifest has no sprite/icon assets")

    font = ImageFont.load_default()
    cell = args.cell_size
    label_h = 24
    left_w = 184
    header_h = 40
    padding = 12
    cols = len(BACKGROUND_SWATCHES)
    rows = len(assets)
    width = left_w + cols * (cell + padding) + padding
    height = header_h + rows * (cell + label_h + padding) + padding
    proof = Image.new("RGB", (width, height), (16, 14, 16))
    draw = ImageDraw.Draw(proof)

    for col, (name, color, mode) in enumerate(BACKGROUND_SWATCHES):
        x = left_w + col * (cell + padding)
        draw_label(draw, (x, 12), name, font)

    report_assets: list[dict[str, Any]] = []
    for row, asset in enumerate(assets):
        asset_path = resolve_path(root, asset["path"])
        sprite = Image.open(asset_path).convert("RGBA")
        metrics = alpha_shadow_metrics(sprite)
        report_assets.append(
            {
                "id": asset.get("id"),
                "path": asset.get("path"),
                "size": list(sprite.size),
                **metrics,
            }
        )

        y = header_h + row * (cell + label_h + padding)
        draw_label(draw, (padding, y + 6), str(asset.get("id")), font)
        draw.text(
            (padding, y + 24),
            f"dark alpha {metrics['dark_translucent_ratio']:.3f}",
            fill=(196, 189, 166),
            font=font,
        )
        if metrics["verdict"] == "review":
            draw.text((padding, y + 40), "review shadow weight", fill=(255, 190, 96), font=font)

        fitted = fit_sprite(sprite, cell - 28)
        for col, (_, color, mode) in enumerate(BACKGROUND_SWATCHES):
            x = left_w + col * (cell + padding)
            if mode == "checker":
                bg = checker((cell, cell), color)
            else:
                bg = Image.new("RGB", (cell, cell), color)
            composed = bg.convert("RGBA")
            px = (cell - fitted.width) // 2
            py = (cell - fitted.height) // 2
            composed.alpha_composite(fitted, (px, py))
            proof.paste(composed.convert("RGB"), (x, y + label_h))
            draw.rectangle((x, y + label_h, x + cell - 1, y + label_h + cell - 1), outline=(84, 76, 64))

    summary = {
        "schema": "game.sprite_background_proof",
        "asset_manifest": str(manifest_path.relative_to(root)),
        "backgrounds": [{"id": name, "color": color, "mode": mode} for name, color, mode in BACKGROUND_SWATCHES],
        "assets_checked": len(report_assets),
        "review_count": sum(1 for asset in report_assets if asset["verdict"] == "review"),
        "assets": report_assets,
    }

    output_path = resolve_path(root, args.output)
    save_image_atomic(proof, output_path)
    if args.json_output:
        write_json_atomic(resolve_path(root, args.json_output), summary)
    if args.report:
        lines = [
            "---",
            "type: SpriteBackgroundProof",
            f"asset_manifest: {summary['asset_manifest']}",
            f"image_output: {Path(args.output).as_posix()}",
            "---",
            "",
            "# Sprite Background Proof",
            "",
            f"Assets checked: **{summary['assets_checked']}**",
            f"Shadow-weight review flags: **{summary['review_count']}**",
            "",
            "This proof composites each runtime sprite over transparent-grid, dark mine panel, warm mine panel, and light inventory-slot backgrounds. It checks whether alpha cutouts are visually clean and which items have enough dark translucent edge pixels to require art-direction review.",
            "",
            "## Assets",
            "",
        ]
        for asset in report_assets:
            lines.append(
                f"- `{asset['id']}` verdict={asset['verdict']} "
                f"dark_translucent_ratio={asset['dark_translucent_ratio']:.5f} "
                f"very_dark_translucent={asset['very_dark_translucent_pixels']}"
            )
        lines.append("")
        write_text_atomic(resolve_path(root, args.report), "\n".join(lines))

    print(f"wrote {output_path}")
    if args.json_output:
        print(f"wrote {args.json_output}")
    if args.report:
        print(f"wrote {args.report}")
    print(f"review_count={summary['review_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
