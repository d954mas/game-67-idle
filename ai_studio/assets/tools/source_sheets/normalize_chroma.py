#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

import sys

ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic


def parse_color(value: str) -> tuple[int, int, int]:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))


def normalize_background(source: Path, output: Path, key: tuple[int, int, int], tolerance: int) -> int:
    image = Image.open(source).convert("RGBA")
    result, changed = normalize_background_numpy(image, key, tolerance)
    save_image_atomic(result, output)
    return changed


def normalize_background_numpy(image: Image.Image, key: tuple[int, int, int], tolerance: int) -> tuple[Image.Image, int]:
    """Vectorized border-connected chroma normalization. Same result as the BFS:
    fill every key-like pixel reachable from the border with the exact key colour;
    interior key-coloured art is preserved (not border-connected)."""
    array = np.asarray(image, dtype=np.uint8).copy()
    alpha = array[..., 3]
    rgb = array[..., :3].astype(np.int16)
    key_array = np.asarray(key, dtype=np.int16)
    key_like = (alpha == 0) | (np.max(np.abs(rgb - key_array), axis=2) <= tolerance)
    connected = _border_connected(key_like)
    target = np.array([key[0], key[1], key[2], 255], dtype=np.uint8)
    changed = int(np.count_nonzero(connected & np.any(array != target, axis=2)))
    array[connected] = target
    return Image.fromarray(array, "RGBA"), changed


def _border_connected(mask: "np.ndarray") -> "np.ndarray":
    """Pixels of ``mask`` connected (4-neighbour) to the image border. Uses
    scipy's single-pass labelling when available, else a numpy-only iterative
    dilation. Both are deterministic."""
    try:
        from scipy.ndimage import label
    except Exception:
        return _border_connected_iterative(mask)
    labels, _count = label(mask)  # default structure = 4-connectivity
    border = np.concatenate([labels[0, :], labels[-1, :], labels[:, 0], labels[:, -1]])
    border_labels = np.unique(border)
    border_labels = border_labels[border_labels != 0]
    if border_labels.size == 0:
        return np.zeros(mask.shape, dtype=bool)
    return np.isin(labels, border_labels)


def _border_connected_iterative(mask: "np.ndarray") -> "np.ndarray":
    connected = np.zeros(mask.shape, dtype=bool)
    connected[0, :] = mask[0, :]
    connected[-1, :] = mask[-1, :]
    connected[:, 0] = mask[:, 0]
    connected[:, -1] = mask[:, -1]
    previous = -1
    while True:
        current = int(np.count_nonzero(connected))
        if current == previous:
            return connected
        previous = current
        expanded = connected.copy()
        expanded[1:, :] |= connected[:-1, :]
        expanded[:-1, :] |= connected[1:, :]
        expanded[:, 1:] |= connected[:, :-1]
        expanded[:, :-1] |= connected[:, 1:]
        connected = expanded & mask


def main() -> int:
    parser = argparse.ArgumentParser(description="Normalize border-connected chroma background in a generated source sheet.")
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--key-color", type=parse_color, default=parse_color("#ff00ff"))
    parser.add_argument("--key-tolerance", type=int, default=48)
    args = parser.parse_args()

    changed = normalize_background(args.source, args.output, args.key_color, args.key_tolerance)
    print(f"wrote {args.output} ({changed} background pixels normalized)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
