#!/usr/bin/env python3
from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

from PIL import Image

try:
    import numpy as np
except ImportError:  # pragma: no cover - fallback path is kept for minimal Python installs.
    np = None

try:
    from tools.assets.atomic_io import save_image_atomic
except ModuleNotFoundError:  # pragma: no cover - supports direct script execution by path.
    from atomic_io import save_image_atomic


def parse_color(value: str) -> tuple[int, int, int]:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))


def is_key_like(pixel: tuple[int, int, int, int], key: tuple[int, int, int], tolerance: int) -> bool:
    red, green, blue, alpha = pixel
    return alpha == 0 or max(abs(red - key[0]), abs(green - key[1]), abs(blue - key[2])) <= tolerance


def normalize_background(source: Path, output: Path, key: tuple[int, int, int], tolerance: int) -> int:
    image = Image.open(source).convert("RGBA")
    if np is not None:
        result, changed = normalize_background_numpy(image, key, tolerance)
    else:
        result, changed = normalize_background_python(image, key, tolerance)
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


def normalize_background_python(image: Image.Image, key: tuple[int, int, int], tolerance: int) -> tuple[Image.Image, int]:
    width, height = image.size
    pixels = image.load()
    visited = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def offset(x: int, y: int) -> int:
        return y * width + x

    def push(x: int, y: int) -> None:
        index = offset(x, y)
        if visited[index]:
            return
        visited[index] = 1
        if is_key_like(pixels[x, y], key, tolerance):
            queue.append((x, y))

    for x in range(width):
        push(x, 0)
        push(x, height - 1)
    for y in range(height):
        push(0, y)
        push(width - 1, y)

    changed = 0
    while queue:
        x, y = queue.popleft()
        red, green, blue, alpha = pixels[x, y]
        if (red, green, blue, alpha) != (*key, 255):
            pixels[x, y] = (*key, 255)
            changed += 1
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < width and 0 <= ny < height:
                push(nx, ny)
    return image, changed


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
