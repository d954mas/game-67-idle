#!/usr/bin/env python3
"""BiRefNet-general neural cutout: a READY IMAGE on an arbitrary/unknown
background, no chroma key and no trimap. Thin wrapper over rembg's ONNX
runtime bridge to the BiRefNet-general checkpoint.

Niche (bench 2026-07-07, tmp/alpha_bench/final/metrics_table.md): this is the
only path here that works from a single arbitrary-background photo with no
key plate and no aligned pair. It LOSES to CorridorKey/ViTMatte on soft glow
and thin/fur detail (see ``../route/`` and ``../alpha_dualplate/`` for those),
and is markedly weak on flat monochrome line-art over a busy background (the
char2_busy fixture: alpha-MAE 9.92 vs isnet's 3.95 on the same fixture) --
BiRefNet's salient-object-detection training distribution is photographic /
rendered natural objects, not vector line art. Document this as a domain
nuance for routing, not a bug to fix here.

LICENSE (recorded, primary sources, see README for the full chain): rembg
itself is MIT. The "birefnet-general" session in rembg wraps
ZhengPeng7/BiRefNet's own code (MIT LICENSE) and its birefnet-general
checkpoint (HF model card ``license: mit``) converted to ONNX and hosted on
rembg's own GitHub releases. ONLY "birefnet-general" is allowlisted here.
briaai/RMBG-2.0 (a different, look-alike BiRefNet checkpoint that rembg does
NOT ship, but that is the model people mean when they say "RMBG-2.0") carries
a NON-COMMERCIAL license -- it must NEVER be substituted for birefnet-general
in this studio, so any other model id is refused loudly rather than silently
accepted.

CPU onnxruntime; no fallback for a missing rembg install -- that is a loud
error naming the setup command (LAW, lead 2026-07-02).
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Any

from PIL import Image

ROOT = Path(__file__).resolve().parents[5]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from ai_studio.assets.tools.lib.atomic_io import save_image_atomic, write_json_atomic

# ALLOWLIST: keep this a tuple of ONE until a second model is independently
# license-verified (MIT/CC0/Apache weights, not just MIT code) and reviewed.
ALLOWED_MODELS: tuple[str, ...] = ("birefnet-general",)

REPORT_SCHEMA = "ai_studio.image.birefnet_cutout_report.v1"


def _check_model_allowed(model: str) -> None:
    """Loud refusal for any model id outside ``ALLOWED_MODELS``. This is a
    license gate, not a feature gate: the studio has verified MIT provenance
    for birefnet-general's code AND weights (see module docstring); it has NOT
    done that for any other rembg session, and briaai/RMBG-2.0 in particular
    ships NON-COMMERCIAL weights that must never be substituted here."""
    if model not in ALLOWED_MODELS:
        raise ValueError(
            f"model {model!r} is not allowed for birefnet_cutout; only {ALLOWED_MODELS!r} "
            "is license-verified for this studio (ZhengPeng7/BiRefNet code MIT LICENSE + "
            "the birefnet-general checkpoint's HF card 'license: mit'). In particular "
            "briaai/RMBG-2.0 is explicitly FORBIDDEN here -- its weights are "
            "NON-COMMERCIAL-licensed, unlike birefnet-general."
        )


def _require_rembg() -> Any:
    """Import rembg or fail loudly naming the setup command -- no silent
    fallback to a different cutout method (LAW, lead 2026-07-02)."""
    try:
        import rembg
    except ImportError as exc:
        raise RuntimeError(
            "rembg is required for birefnet_cutout but could not be imported; run "
            "node ai_studio/assets/tools/image/_bridge/setup_python.mjs to install the "
            "pinned studio Python deps (rembg, onnxruntime)."
        ) from exc
    return rembg


def birefnet_cutout(
    image: Image.Image,
    *,
    model: str = "birefnet-general",
    session: Any | None = None,
) -> Image.Image:
    """Return an RGBA cutout of ``image`` using rembg's BiRefNet-general
    session. ``session`` is created via ``rembg.new_session(model)`` unless one
    is injected (tests, or a caller reusing one warm session across many
    images to avoid paying model-load cost per call). Native image size is
    preserved -- rembg's own contract, not something this wrapper re-derives."""
    _check_model_allowed(model)
    rembg = _require_rembg()
    if session is None:
        session = rembg.new_session(model)
    rgb = image.convert("RGB")
    result = rembg.remove(rgb, session=session)
    return result.convert("RGBA")


def run(
    source: Path,
    output: Path,
    *,
    model: str = "birefnet-general",
    report: Path | None = None,
) -> dict[str, Any]:
    """CLI-level driver: validate args, run the cutout once, save the PNG, and
    build the provenance report dict (also written to ``report`` if given).
    Validation order is deliberate -- the license allowlist and the source-file
    check both run BEFORE rembg/the model are touched, so a bad ``--model`` or
    a missing ``--in`` never triggers an import or a model download."""
    _check_model_allowed(model)
    if not source.exists():
        raise FileNotFoundError(f"source image missing: {source}")
    rembg = _require_rembg()
    image = Image.open(source).convert("RGB")
    t0 = time.perf_counter()
    result = birefnet_cutout(image, model=model)
    seconds = time.perf_counter() - t0
    save_image_atomic(result, output)
    report_data: dict[str, Any] = {
        "schema": REPORT_SCHEMA,
        "tool": "birefnet_cutout",
        "model": model,
        "device": "cpu-onnxruntime",
        "seconds": round(seconds, 3),
        "in_size": list(image.size),
        "out_size": list(result.size),
        "rembg_version": getattr(rembg, "__version__", None),
    }
    if report is not None:
        write_json_atomic(report, report_data)
    return report_data


def main() -> int:
    parser = argparse.ArgumentParser(
        description="BiRefNet-general cutout: ready image on an arbitrary background, no key/trimap."
    )
    parser.add_argument("--in", dest="source", required=True, type=Path, help="input image path")
    parser.add_argument("--out", dest="output", required=True, type=Path, help="output RGBA PNG path")
    parser.add_argument(
        "--model",
        default="birefnet-general",
        help=f"model id (allowlisted: {ALLOWED_MODELS}; anything else is a licensing refusal)",
    )
    parser.add_argument("--report", type=Path, help="optional provenance report JSON path")
    args = parser.parse_args()

    try:
        report_data = run(args.source, args.output, model=args.model, report=args.report)
    except (RuntimeError, ValueError, FileNotFoundError) as exc:
        # Deliberate refusals travel as ONE clean message -- no raw Python
        # traceback surfaced to the operator. Unexpected bugs still traceback.
        raise SystemExit(str(exc)) from exc

    print(
        f"pass: birefnet_cutout ({report_data['model']}, {report_data['seconds']}s, "
        f"{report_data['in_size']}->{report_data['out_size']})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
