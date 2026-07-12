#!/usr/bin/env python3
"""Build a registered garment layer from a locked nude/worn pair and a mask.

This tool owns *semantic doll removal*, not chroma-key alpha extraction.  The
semantic mask says where the garment may exist; a high-confidence RGB-diff
seed rejects unsupported mask pixels.  Every visible output RGB pixel is
copied byte-for-byte from the dressed plate.

Inputs are deliberately strict: PNG only, identical dimensions, no implicit
resize.  A generated mask is guidance, never a source of shipping colour.

Example:
  .venv\\Scripts\\python.exe games/web-dressup/tools/body_peel.py ^
    --nude nude.png --dressed worn.png --semantic-mask garment_mask.png ^
    --slot main --mask-channel luminance --out-plate garment_magenta.png --out garment.png ^
    --report report.json --proof-dir proof
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

SLOTS = ("hair", "main", "bottom", "shoes", "accent")
MAGENTA = np.array((255, 0, 255, 255), dtype=np.uint8)


def load_png(path: Path, *, role: str) -> Image.Image:
    """Load a real PNG and reject lossy/renamed inputs loudly."""
    if path.suffix.lower() != ".png":
        raise ValueError(f"{role}: PNG only; got {path.name}")
    try:
        image = Image.open(path)
        image.load()
    except (FileNotFoundError, OSError) as exc:
        raise ValueError(f"{role}: cannot read PNG {path}: {exc}") from exc
    if image.format != "PNG":
        raise ValueError(f"{role}: PNG only; file content is {image.format or 'unknown'}")
    return image


def _semantic_alpha(image: Image.Image, channel: str) -> np.ndarray:
    """Read the explicitly authored mask channel; never guess its encoding."""
    if channel == "alpha":
        if "A" not in image.getbands():
            raise ValueError("semantic-mask: alpha channel requested but image has no alpha")
        return np.asarray(image.getchannel("A"), dtype=np.uint8)
    if channel == "luminance":
        return np.asarray(image.convert("L"), dtype=np.uint8)
    raise ValueError("mask_channel must be 'alpha' or 'luminance'")


def _composite_rgb(rgba: np.ndarray, background: int = 127) -> np.ndarray:
    """Composite RGBA onto one neutral background before any RGB comparison."""
    rgb = rgba[:, :, :3].astype(np.float32)
    alpha = rgba[:, :, 3:4].astype(np.float32) / 255.0
    return np.rint(rgb * alpha + float(background) * (1.0 - alpha)).astype(np.uint8)


def _dilate(mask: np.ndarray, radius: int) -> np.ndarray:
    if radius < 0:
        raise ValueError("support_radius must be >= 0")
    if radius == 0:
        return mask.copy()
    image = Image.fromarray(mask.astype(np.uint8) * 255, "L")
    return np.asarray(image.filter(ImageFilter.MaxFilter(radius * 2 + 1))) > 0


def peel(
    nude_rgba: np.ndarray,
    dressed_rgba: np.ndarray,
    semantic_alpha: np.ndarray,
    *,
    slot: str,
    seed_threshold: int = 48,
    support_radius: int = 4,
    min_support_ratio: float = 0.85,
    max_alpha_mismatch_ratio: float = 0.01,
    max_exposed_drift_ratio: float = 0.05,
) -> tuple[Image.Image, Image.Image, dict]:
    """Return registered RGBA, opaque magenta plate, and a QA report.

    The semantic mask is intersected with a dilated high-confidence difference
    seed.  This keeps plausible low-difference edge pixels near the garment but
    prevents a semantic edit from reintroducing distant face/body regions.
    """
    if slot not in SLOTS:
        raise ValueError(f"unknown slot {slot!r}; expected one of {', '.join(SLOTS)}")
    if nude_rgba.ndim != 3 or dressed_rgba.ndim != 3:
        raise ValueError("nude and dressed inputs must be HxWxC images")
    if nude_rgba.shape[:2] != dressed_rgba.shape[:2]:
        raise ValueError(
            f"dimension mismatch: nude {nude_rgba.shape[1]}x{nude_rgba.shape[0]} "
            f"vs dressed {dressed_rgba.shape[1]}x{dressed_rgba.shape[0]}"
        )
    if semantic_alpha.shape != nude_rgba.shape[:2]:
        raise ValueError(
            f"dimension mismatch: semantic mask {semantic_alpha.shape[1]}x{semantic_alpha.shape[0]} "
            f"vs plates {nude_rgba.shape[1]}x{nude_rgba.shape[0]}"
        )
    if not 1 <= seed_threshold <= 255:
        raise ValueError("seed_threshold must be in 1..255")
    if not 0.0 <= min_support_ratio <= 1.0:
        raise ValueError("min_support_ratio must be in 0..1")

    if nude_rgba.shape[2] < 4 or dressed_rgba.shape[2] < 4:
        raise ValueError("nude and dressed inputs must be RGBA")
    if not 0.0 <= max_alpha_mismatch_ratio <= 1.0:
        raise ValueError("max_alpha_mismatch_ratio must be in 0..1")
    if not 0.0 <= max_exposed_drift_ratio <= 1.0:
        raise ValueError("max_exposed_drift_ratio must be in 0..1")

    semantic = semantic_alpha.astype(np.uint8)
    semantic_visible = semantic > 0
    semantic_count = int(np.count_nonzero(semantic_visible))
    if semantic_count == 0:
        raise ValueError("semantic mask is empty")

    nude_alpha = nude_rgba[:, :, 3].astype(np.int16)
    dressed_alpha = dressed_rgba[:, :, 3].astype(np.int16)
    outside_semantic = ~semantic_visible
    alpha_mismatch = (np.abs(dressed_alpha - nude_alpha) >= 128) & outside_semantic
    outside_count = int(np.count_nonzero(outside_semantic))
    alpha_mismatch_ratio = (
        float(np.count_nonzero(alpha_mismatch) / outside_count) if outside_count else 0.0
    )
    if alpha_mismatch_ratio > max_alpha_mismatch_ratio:
        raise ValueError(
            "alpha/background mismatch outside garment mask: "
            f"{alpha_mismatch_ratio:.3f} > {max_alpha_mismatch_ratio:.3f}"
        )

    nude_rgb = _composite_rgb(nude_rgba).astype(np.int16)
    dressed_composite = _composite_rgb(dressed_rgba)
    dressed_rgb = dressed_rgba[:, :, :3].astype(np.uint8)
    diff = np.abs(dressed_composite.astype(np.int16) - nude_rgb).max(axis=2)
    shared_body = (nude_alpha >= 128) & (dressed_alpha >= 128) & outside_semantic
    shared_body_count = int(np.count_nonzero(shared_body))
    exposed_drift = (diff >= seed_threshold) & shared_body
    exposed_drift_ratio = (
        float(np.count_nonzero(exposed_drift) / shared_body_count) if shared_body_count else 0.0
    )
    if exposed_drift_ratio > max_exposed_drift_ratio:
        raise ValueError(
            "exposed-body drift outside garment mask: "
            f"{exposed_drift_ratio:.3f} > {max_exposed_drift_ratio:.3f}"
        )

    seed = diff >= seed_threshold
    support = _dilate(seed, support_radius)

    supported_semantic = semantic_visible & support
    support_ratio = float(np.count_nonzero(supported_semantic) / semantic_count)
    if support_ratio < min_support_ratio:
        raise ValueError(
            "semantic mask is not supported by the worn/nude difference: "
            f"{support_ratio:.3f} < {min_support_ratio:.3f}"
        )

    final_alpha = np.where(support, semantic, 0).astype(np.uint8)
    visible = final_alpha > 0
    hard_visible = final_alpha > 128
    if not np.any(visible):
        raise ValueError("combined semantic/difference mask is empty")

    layer = np.zeros((*final_alpha.shape, 4), dtype=np.uint8)
    layer[visible, :3] = dressed_rgb[visible]
    layer[:, :, 3] = final_alpha

    plate = np.empty_like(layer)
    plate[:] = MAGENTA
    plate[visible, :3] = dressed_rgb[visible]

    seed_count = int(np.count_nonzero(seed))
    seed_capture_ratio = (
        float(np.count_nonzero(seed & semantic_visible) / seed_count) if seed_count else 0.0
    )
    rgb_preserved = bool(np.array_equal(layer[visible, :3], dressed_rgb[visible]))
    report = {
        "schema": "web_dressup.body_peel.v3",
        "verdict": "ok",
        "slot": slot,
        "width": int(layer.shape[1]),
        "height": int(layer.shape[0]),
        "seed_threshold": int(seed_threshold),
        "support_radius": int(support_radius),
        "min_support_ratio": float(min_support_ratio),
        "max_alpha_mismatch_ratio": float(max_alpha_mismatch_ratio),
        "alpha_mismatch_ratio": alpha_mismatch_ratio,
        "max_exposed_drift_ratio": float(max_exposed_drift_ratio),
        "exposed_drift_ratio": exposed_drift_ratio,
        "semantic_px": semantic_count,
        "supported_semantic_px": int(np.count_nonzero(supported_semantic)),
        "support_ratio": support_ratio,
        "seed_px": seed_count,
        "seed_capture_ratio": seed_capture_ratio,
        "visible_px": int(np.count_nonzero(visible)),
        "hard_visible_px": int(np.count_nonzero(hard_visible)),
        "diff_mean": float(diff.mean()),
        "diff_p90": float(np.percentile(diff, 90)),
        "rgb_preserved": rgb_preserved,
    }
    return Image.fromarray(layer, "RGBA"), Image.fromarray(plate, "RGBA"), report


def _checkerboard(size: tuple[int, int], cell: int = 12) -> Image.Image:
    w, h = size
    yy, xx = np.indices((h, w))
    checks = ((xx // cell + yy // cell) & 1).astype(bool)
    rgb = np.empty((h, w, 4), dtype=np.uint8)
    rgb[:] = (222, 222, 226, 255)
    rgb[checks] = (170, 170, 178, 255)
    return Image.fromarray(rgb, "RGBA")


def _composite(background: Image.Image, layer: Image.Image) -> Image.Image:
    return Image.alpha_composite(background.convert("RGBA"), layer)


def write_proofs(proof_dir: Path, nude: Image.Image, layer: Image.Image) -> None:
    proof_dir.mkdir(parents=True, exist_ok=True)
    size = layer.size
    _composite(_checkerboard(size), layer).save(proof_dir / "checkerboard.png")
    _composite(Image.new("RGBA", size, "white"), layer).save(proof_dir / "on_white.png")
    _composite(Image.new("RGBA", size, (28, 22, 38, 255)), layer).save(proof_dir / "on_dark.png")
    _composite(nude.convert("RGBA"), layer).save(proof_dir / "on_body.png")


def process_files(
    *,
    nude_path: Path,
    dressed_path: Path,
    semantic_mask_path: Path,
    out_path: Path,
    out_plate_path: Path,
    report_path: Path,
    proof_dir: Path | None,
    slot: str,
    mask_channel: str,
    seed_threshold: int = 48,
    support_radius: int = 4,
    min_support_ratio: float = 0.85,
) -> dict:
    nude_image = load_png(nude_path, role="nude").convert("RGBA")
    dressed_image = load_png(dressed_path, role="dressed").convert("RGBA")
    semantic_image = load_png(semantic_mask_path, role="semantic-mask")
    semantic = _semantic_alpha(semantic_image, mask_channel)

    layer, plate, report = peel(
        np.asarray(nude_image),
        np.asarray(dressed_image),
        semantic,
        slot=slot,
        seed_threshold=seed_threshold,
        support_radius=support_radius,
        min_support_ratio=min_support_ratio,
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_plate_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    layer.save(out_path)
    plate.save(out_plate_path)
    if proof_dir is not None:
        write_proofs(proof_dir, nude_image, layer)

    report.update(
        {
            "mask_channel": mask_channel,
            "nude": str(nude_path),
            "dressed": str(dressed_path),
            "semantic_mask": str(semantic_mask_path),
            "out": str(out_path),
            "out_plate": str(out_plate_path),
            "proof_dir": str(proof_dir) if proof_dir is not None else None,
        }
    )
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Gated semantic body peel: locked nude + worn + mask -> registered garment"
    )
    parser.add_argument("--nude", required=True, type=Path)
    parser.add_argument("--dressed", required=True, type=Path)
    parser.add_argument("--semantic-mask", required=True, type=Path)
    parser.add_argument("--mask-channel", required=True, choices=("alpha", "luminance"))
    parser.add_argument("--slot", required=True, choices=SLOTS)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--out-plate", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--proof-dir", type=Path)
    parser.add_argument("--seed-threshold", type=int, default=48)
    parser.add_argument("--support-radius", type=int, default=4)
    parser.add_argument("--min-support-ratio", type=float, default=0.85)
    args = parser.parse_args(argv)
    try:
        report = process_files(
            nude_path=args.nude,
            dressed_path=args.dressed,
            semantic_mask_path=args.semantic_mask,
            mask_channel=args.mask_channel,
            out_path=args.out,
            out_plate_path=args.out_plate,
            report_path=args.report,
            proof_dir=args.proof_dir,
            slot=args.slot,
            seed_threshold=args.seed_threshold,
            support_radius=args.support_radius,
            min_support_ratio=args.min_support_ratio,
        )
    except ValueError as exc:
        print(f"body_peel: REJECT: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
