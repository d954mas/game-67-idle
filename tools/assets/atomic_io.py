from __future__ import annotations

import json
import tempfile
from pathlib import Path
from typing import Any

from PIL import Image


def atomic_temp_path(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
        delete=False,
    ) as handle:
        return Path(handle.name)


def write_text_atomic(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = atomic_temp_path(path)
    try:
        tmp_path.write_text(text, encoding="utf-8")
        tmp_path.replace(path)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()


def write_json_atomic(path: Path, data: Any, *, indent: int = 2, trailing_newline: bool = True) -> None:
    text = json.dumps(data, indent=indent)
    if trailing_newline:
        text += "\n"
    write_text_atomic(path, text)


def save_image_atomic(image: Image.Image, path: Path, *, format: str | None = None, **save_kwargs: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    format_name = format
    if format_name is None:
        suffix = path.suffix.lstrip(".").upper()
        format_name = "JPEG" if suffix == "JPG" else (suffix or None)
    tmp_path = atomic_temp_path(path)
    try:
        image.save(tmp_path, format=format_name, **save_kwargs)
        tmp_path.replace(path)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()
