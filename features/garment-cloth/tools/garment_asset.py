"""Read an authored garment independently of the game's inventory and renderer."""
import json
from pathlib import Path


def load_asset(path):
    path = Path(path)
    try:
        asset = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"{path}:{exc.lineno}: {exc.msg}") from exc
    if not isinstance(asset, dict) or set(asset) - {"pieces", "supports"}:
        raise ValueError(f"{path}: expected an object with pieces and optional supports")
    pieces = asset.get("pieces")
    if not isinstance(pieces, list) or not pieces:
        raise ValueError(f"{path}: expected at least one piece")
    names = set()
    for piece in pieces:
        if not isinstance(piece, dict) or not isinstance(piece.get("id"), str) or not piece["id"]:
            raise ValueError(f"{path}: every piece needs an id")
        if piece["id"] in names:
            raise ValueError(f"{path}: duplicate piece {piece['id']}")
        names.add(piece["id"])
    if not isinstance(asset.get("supports", {}), dict):
        raise ValueError(f"{path}: supports must be an object")
    return asset
