#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic

RGB = tuple[int, int, int]


def parse_color(value: str) -> RGB:
    text = value.strip().lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError("color must be #rrggbb")
    try:
        return tuple(int(text[index : index + 2], 16) for index in (0, 2, 4))  # type: ignore[return-value]
    except ValueError as exc:
        raise argparse.ArgumentTypeError("color must be #rrggbb") from exc


def format_color(value: RGB) -> str:
    return "#{:02x}{:02x}{:02x}".format(*value)


def rel(path: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def border_connected(mask: np.ndarray) -> np.ndarray:
    try:
        from scipy.ndimage import label
    except Exception:
        return border_connected_iterative(mask)

    labels, _count = label(mask)
    border = np.concatenate([labels[0, :], labels[-1, :], labels[:, 0], labels[:, -1]])
    border_labels = np.unique(border)
    border_labels = border_labels[border_labels != 0]
    if border_labels.size == 0:
        return np.zeros(mask.shape, dtype=bool)
    return np.isin(labels, border_labels)


def border_connected_iterative(mask: np.ndarray) -> np.ndarray:
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


def estimate_border_key_color(image: Image.Image, *, alpha_threshold: int) -> RGB:
    array = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    border = np.concatenate([array[0, :, :], array[-1, :, :], array[:, 0, :], array[:, -1, :]], axis=0)
    opaque = border[border[:, 3] > alpha_threshold]
    if opaque.size == 0:
        return 255, 0, 255
    rgb = opaque[:, :3]
    colors, counts = np.unique(rgb, axis=0, return_counts=True)
    key = colors[int(np.argmax(counts))]
    return int(key[0]), int(key[1]), int(key[2])


def normalize_background_numpy(
    image: Image.Image,
    *,
    key_color: RGB | None,
    key_tolerance: int,
    alpha_threshold: int,
) -> tuple[Image.Image, dict[str, object]]:
    if key_color is None:
        key_color = estimate_border_key_color(image, alpha_threshold=alpha_threshold)
    array = np.asarray(image.convert("RGBA"), dtype=np.uint8).copy()
    rgb = array[..., :3].astype(np.int16)
    alpha = array[..., 3]
    key = np.asarray(key_color, dtype=np.int16)
    key_like = (alpha <= alpha_threshold) | (np.max(np.abs(rgb - key), axis=2) <= key_tolerance)
    background = border_connected(key_like)
    target = np.array([key_color[0], key_color[1], key_color[2], 255], dtype=np.uint8)
    changed = int(np.count_nonzero(background & np.any(array != target, axis=2)))
    array[background] = target
    report = {
        "schema": "ai_studio.raster2d.background_normalize.v1",
        "mode": "border_connected_key",
        "key_color": format_color(key_color),
        "key_tolerance": key_tolerance,
        "alpha_threshold": alpha_threshold,
        "image": {"width": image.width, "height": image.height},
        "background_pixels": int(np.count_nonzero(background)),
        "changed_pixels": changed,
    }
    return Image.fromarray(array, "RGBA"), report


def normalize_background(
    source: Path,
    output: Path,
    *,
    mode: str,
    key_color: RGB | None,
    key_tolerance: int,
    alpha_threshold: int,
    json_output: Path | None = None,
) -> dict[str, object]:
    image = Image.open(source).convert("RGBA")
    if mode == "none":
        report = {
            "schema": "ai_studio.raster2d.background_normalize.v1",
            "mode": "passthrough_no_background",
            "key_color": "#ff00ff",
            "key_tolerance": key_tolerance,
            "alpha_threshold": alpha_threshold,
            "image": {"width": image.width, "height": image.height},
            "background_pixels": 0,
            "changed_pixels": 0,
            "source": rel(source),
            "output": rel(output),
        }
        save_image_atomic(image, output)
        if json_output:
            write_json_atomic(json_output, report)
        return report

    result, report = normalize_background_numpy(
        image,
        key_color=key_color,
        key_tolerance=key_tolerance,
        alpha_threshold=alpha_threshold,
    )
    report["source"] = rel(source)
    report["output"] = rel(output)
    save_image_atomic(result, output)
    if json_output:
        write_json_atomic(json_output, report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Normalize a border-connected chroma background to exact key color.")
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--mode", choices=["auto", "none"], default="auto")
    parser.add_argument("--key-color", type=parse_color)
    parser.add_argument("--key-tolerance", type=int, default=32)
    parser.add_argument("--alpha-threshold", type=int, default=0)
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()
    if min(args.key_tolerance, args.alpha_threshold) < 0:
        raise SystemExit("numeric thresholds must be >= 0")

    report = normalize_background(
        args.source,
        args.output,
        mode=args.mode,
        key_color=args.key_color,
        key_tolerance=args.key_tolerance,
        alpha_threshold=args.alpha_threshold,
        json_output=args.json_output,
    )
    print(f"pass: normalized {report['changed_pixels']} background pixel(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
