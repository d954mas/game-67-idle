#!/usr/bin/env python3
"""Validate 67 World runtime PNGs before atlas packing."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
PACK_BUILDER = ROOT / "tools" / "assets" / "build_67_world_packs.c"
RUNTIME_DIR = ROOT / "assets" / "runtime" / "67-world"


@dataclass(frozen=True)
class PackAsset:
    asset_id: str
    filename: str
    kind: str
    s9_left: int
    s9_top: int
    s9_right: int
    s9_bottom: int


ASSET_RE = re.compile(
    r'\{"(?P<id>[^"]+)",\s*RUNTIME_DIR\s*"/(?P<file>[^"]+)",\s*"(?P<kind>[^"]+)",\s*'
    r"(?P<left>\d+),\s*(?P<top>\d+),\s*(?P<right>\d+),\s*(?P<bottom>\d+),"
)


def read_pack_assets() -> list[PackAsset]:
    text = PACK_BUILDER.read_text(encoding="utf-8")
    assets: list[PackAsset] = []
    for match in ASSET_RE.finditer(text):
        assets.append(
            PackAsset(
                asset_id=match.group("id"),
                filename=match.group("file"),
                kind=match.group("kind"),
                s9_left=int(match.group("left")),
                s9_top=int(match.group("top")),
                s9_right=int(match.group("right")),
                s9_bottom=int(match.group("bottom")),
            )
        )
    return assets


def validate_asset(asset: PackAsset) -> list[str]:
    errors: list[str] = []
    path = RUNTIME_DIR / asset.filename
    if not path.exists():
        return [f"{asset.asset_id}: missing {path}"]

    try:
        image = Image.open(path).convert("RGBA")
    except Exception as exc:  # pragma: no cover - defensive diagnostics
        return [f"{asset.asset_id}: cannot read {path}: {exc}"]

    alpha = image.getchannel("A")
    bbox = alpha.getbbox()
    if bbox is None:
        errors.append(f"{asset.asset_id}: fully transparent alpha")

    if min(image.size) <= 0:
        errors.append(f"{asset.asset_id}: invalid size {image.size}")

    if any((asset.s9_left, asset.s9_top, asset.s9_right, asset.s9_bottom)):
        if asset.s9_left + asset.s9_right >= image.width:
            errors.append(f"{asset.asset_id}: bad slice9 horizontal margins for width {image.width}")
        if asset.s9_top + asset.s9_bottom >= image.height:
            errors.append(f"{asset.asset_id}: bad slice9 vertical margins for height {image.height}")

    return errors


def main() -> int:
    assets = read_pack_assets()
    if not assets:
        print(f"FAIL: no pack assets found in {PACK_BUILDER}", file=sys.stderr)
        return 1

    errors: list[str] = []
    for asset in assets:
        errors.extend(validate_asset(asset))

    if errors:
        for error in errors:
            print(f"FAIL {error}", file=sys.stderr)
        print(f"validated {len(assets)} pack input asset(s), {len(errors)} failure(s)", file=sys.stderr)
        return 1

    print(f"ok: validated {len(assets)} pack input asset(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
