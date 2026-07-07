#!/usr/bin/env python3
"""ViTMatte alpha matting on a flat-key plate (path 4): thin/fine detail
(spider-web, mesh, fur, hair strands) where key_matte's bounded alpha band and
corridorkey's screen-unmix both under-resolve the strand-level structure; also
the SECOND-priority engine for glow/soft-bloom on a flat key (CorridorKey is
first there -- after despill it still leaves a slight background tint, the
lead's own eye ruling on the glow-wings comparison, alpha-methods-portfolio
bench 2026-07-07).

Runs ONLY in this tool's OWN venv (ai_studio/assets/tools/image/vitmatte_matte/
.venv/), never the shared repo .venv -- GPU torch (cu128, ~2.7GB) must not enter
the shared venv. Missing/broken venv is a LOUD error naming the setup script
(no-fallback law), not a silent skip.

Model: hustvl/vitmatte-base-composition-1k is the ONLY allowed checkpoint --
see the README License section before pointing this at any other checkpoint
(the Adobe Deep Image Matting noncommercial caveat was reviewed for THIS
checkpoint only).

Pipeline: trimap (matte_math.build_auto_trimap, chroma distance to the given
flat key) -> VitMatteImageProcessor/VitMatteForImageMatting -> alpha at native
resolution (the processor only pads to a size divisible by 32; the padding is
cropped back off) -> despill (matte_math.despill) by default, with the
sub-clamp region's alpha also zeroed (``--no-despill`` keeps raw plate RGB, no
alpha changes either).
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[5]  # vitmatte_matte -> image -> tools -> assets -> ai_studio -> repo root
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.image.vitmatte_matte.matte_math import (  # noqa: E402
    build_auto_trimap,
    despill,
    trimap_stats,
    DESPILL_ALPHA_CLAMP,
)

SETUP_COMMAND = "node ai_studio/assets/tools/image/vitmatte_matte/setup_python.mjs"

try:
    import truststore
except ImportError as exc:  # pragma: no cover - exercised only on a broken/missing venv
    raise SystemExit(
        f"vitmatte_matte requires its OWN GPU-torch venv, not the shared repo .venv. "
        f"Missing dependency 'truststore' ({exc}). Fix: run `{SETUP_COMMAND}` from the "
        f"repo root, then invoke this script with THAT venv's python.exe."
    ) from exc

# Avast Web Shield does TLS MITM on this box; inject the OS trust store into ssl
# BEFORE transformers (and its first HF Hub download) ever opens a connection.
truststore.inject_into_ssl()

try:
    import torch
    import transformers
    from transformers import VitMatteForImageMatting, VitMatteImageProcessor
except ImportError as exc:  # pragma: no cover - exercised only on a broken/missing venv
    raise SystemExit(
        f"vitmatte_matte requires its OWN GPU-torch venv, not the shared repo .venv. "
        f"Missing dependency ({exc}). Fix: run `{SETUP_COMMAND}` from the repo root, "
        f"then invoke this script with THAT venv's python.exe (.venv/Scripts/python.exe)."
    ) from exc

# The ONLY checkpoint reviewed and cleared for use (README License section).
# hustvl code is MIT and the author explicitly blessed weight use (issue #9),
# but the checkpoint is fine-tuned on Composition-1k / Adobe Deep Image
# Matting data whose terms are noncommercial -- ALLOW-WITH-CONDITIONS,
# local-only weights, second-priority engine. Any other checkpoint has not
# been through that review.
ALLOWED_MODEL = "hustvl/vitmatte-base-composition-1k"


def _check_model_allowlist(model_id: str) -> None:
    if model_id != ALLOWED_MODEL:
        raise SystemExit(
            f"vitmatte_matte only allows the reviewed checkpoint {ALLOWED_MODEL!r} "
            f"(see README.md License section) -- {model_id!r} is NOT license-cleared. "
            "The Composition-1k / Adobe Deep Image Matting noncommercial-terms caveat "
            "and the hustvl MIT-weights blessing (github.com/hustvl/ViTMatte issue #9) "
            "were only checked for the allowed checkpoint. Get the lead to ratify a new "
            "checkpoint's license before adding it here."
        )


def load_model(model_id: str = ALLOWED_MODEL):
    """Load the processor + model once. Returns (processor, model, device)."""
    _check_model_allowlist(model_id)
    processor = VitMatteImageProcessor.from_pretrained(model_id)
    model = VitMatteForImageMatting.from_pretrained(model_id)
    model.eval()
    device = "cuda" if torch.cuda.is_available() else "cpu"
    model.to(device)
    return processor, model, device


def predict_alpha(processor, model, device: str, image_rgb: Image.Image, trimap_l: Image.Image) -> np.ndarray:
    """Run the model on ``image_rgb``/``trimap_l`` (both PIL, native size).
    Returns an HxW float64 alpha in [0, 1] at the ORIGINAL native resolution
    (the processor pads to a size divisible by 32; the pad is cropped back
    off here, never left in the output)."""
    width, height = image_rgb.size
    inputs = processor(images=image_rgb, trimaps=trimap_l, return_tensors="pt")
    inputs = {key: value.to(device) for key, value in inputs.items()}
    with torch.inference_mode():
        alphas = model(**inputs).alphas
    alpha = alphas[0, 0].detach().float().cpu().numpy()
    return alpha[:height, :width].astype(np.float64)


def predict_alpha_with_oom_fallback(processor, model, device: str, image_rgb: Image.Image, trimap_l: Image.Image):
    """``predict_alpha`` on ``device``, falling back to CPU on a CUDA OOM
    (matches the bench precedent: OOM is common on native-size high-res
    plates on a shared/loaded GPU). Returns (alpha, device_used).

    Deliberately does NOT move the model back to the GPU after a CPU fallback:
    this is a one-shot CLI (the process exits right after), and the move-back
    itself can raise a SECOND OutOfMemoryError on the still-pressured GPU —
    which would discard the alpha that was just successfully computed on CPU
    (review T0335 finding 6)."""
    try:
        return predict_alpha(processor, model, device, image_rgb, trimap_l), device
    except torch.cuda.OutOfMemoryError:  # noqa: E722 - narrow, matches bench precedent
        torch.cuda.empty_cache()
        model.to("cpu")
        alpha = predict_alpha(processor, model, "cpu", image_rgb, trimap_l)
        return alpha, "cpu (cuda OOM)"


def _parse_rgb(text: str) -> tuple[int, int, int]:
    parts = [int(p) for p in text.replace(" ", "").split(",")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("expected r,g,b")
    return (parts[0], parts[1], parts[2])


def run(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="ViTMatte alpha matting on a flat-key plate (auto chroma trimap).")
    parser.add_argument("--in", dest="in_path", required=True, type=Path, help="input flat-key plate PNG")
    parser.add_argument("--key", required=True, type=_parse_rgb, help="flat key colour r,g,b, e.g. 255,0,255")
    parser.add_argument("--out", required=True, type=Path, help="output RGBA PNG")
    parser.add_argument("--report", type=Path, default=None, help="optional report JSON output path")
    parser.add_argument("--trimap-out", type=Path, default=None, help="optional: save the auto-trimap used")
    parser.add_argument("--no-despill", action="store_true", help="keep raw plate RGB (skip the despill un-blend)")
    parser.add_argument("--model", default=ALLOWED_MODEL, help=f"checkpoint (allowlisted; default {ALLOWED_MODEL})")
    args = parser.parse_args(argv)

    _check_model_allowlist(args.model)
    plate = Image.open(args.in_path).convert("RGB")
    trimap = build_auto_trimap(plate, key=args.key)
    if args.trimap_out is not None:
        trimap.save(args.trimap_out)

    processor, model, device = load_model(args.model)
    start = time.perf_counter()
    alpha, device_used = predict_alpha_with_oom_fallback(processor, model, device, plate, trimap)
    seconds = time.perf_counter() - start

    plate_rgb_u8 = np.asarray(plate, dtype=np.uint8)
    despill_applied = not args.no_despill
    if despill_applied:
        rgb_out = despill(plate_rgb_u8, alpha, args.key)
        alpha_out = np.where(alpha < DESPILL_ALPHA_CLAMP, 0.0, alpha)
    else:
        rgb_out = plate_rgb_u8
        alpha_out = alpha

    alpha_u8 = np.clip(np.rint(alpha_out * 255.0), 0, 255).astype(np.uint8)
    result = Image.fromarray(np.dstack([rgb_out, alpha_u8]), "RGBA")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    result.save(args.out)

    stats = trimap_stats(trimap)
    print(
        f"[vitmatte_matte] {args.in_path.name} {plate.size} device={device_used} "
        f"{seconds:.2f}s despill={despill_applied} trimap={stats}"
    )

    if args.report is not None:
        report = {
            "schema": "ai_studio.image.vitmatte_matte_report.v1",
            "tool": "vitmatte_matte",
            "model": args.model,
            "device": device_used,
            "seconds": round(seconds, 3),
            "in_size": list(plate.size),
            "trimap": stats,
            "despill": despill_applied,
            "transformers_version": transformers.__version__,
            "torch_version": torch.__version__,
        }
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
