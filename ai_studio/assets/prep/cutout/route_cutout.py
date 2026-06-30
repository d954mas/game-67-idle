#!/usr/bin/env python3
"""Auto-route a flat-key source crop to the correct cutout path.

Two cutout paths, disjoint problems:
  * key_matte (path 1) -- opaque art with a thin anti-aliased edge. The alpha is
    ~binary with a 1-2px transition; a single flat-key background is enough.
  * dual_plate (path 2) -- art with a WIDE soft/semi-transparent zone (glow,
    aura, smoke, glass, soft cast shadow). That fractional alpha is
    mathematically unrecoverable from ONE background, so it needs a white+black
    plate pair (gen_dual_plate.sh) instead.

This module decides which, FROM THE SINGLE FLAT-KEY SOURCE, so the choice can be
made before a generation is spent. Discriminator: the fraction of "partial"
keyness mass -- pixels that are a blend of key + foreground spread over a wide
band -- plus how DEEP that transition runs. Opaque art has a bimodal keyness
histogram (a tiny partial ring); soft art has a fat middle / deep gradient.

Calibrated 2026-06-18 on mine-cards sources (key from the crop plan):
  hard/opaque icons/coins  soft_score <= 0.06,  depth90 <= 11px
  soft FX / glow / ring     soft_score >= 0.17,  ring depth90 = 22px
=> route to dual_plate when soft_score >= 0.11 OR depth90 >= 14px (the OR catches
the thin-but-deep glow ring that has a modest area score but a long gradient).

CPU + numpy only; no native code, no ML. The cutout is ~0.3% of one gpt-image-2
generation, so this stays cheap by design.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = Path(__file__).resolve().parents[4]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

# --- routing constants (calibrated; see module docstring) -----------------
KEY_TOLERANCE = 80.0  # color-distance scale that maps to alpha in [0,1]
PARTIAL_LO = 0.15     # keyness band that counts as "soft/semi-transparent"
PARTIAL_HI = 0.85
SCORE_THRESHOLD = 0.11   # >= -> wide soft zone -> dual_plate
DEPTH90_THRESHOLD = 14.0  # >= -> deep gradient (thin glow ring) -> dual_plate

RGB = tuple[int, int, int]


@dataclass(frozen=True)
class RouteDecision:
    method: str  # "key_matte" | "dual_plate"
    needs_dual: bool
    soft_score: float
    depth90: float
    depth_median: float
    key: RGB
    n_core: int
    n_partial: int
    reason: str


def _key_from_border(rgb: np.ndarray) -> RGB:
    band = np.concatenate([rgb[0], rgb[-1], rgb[:, 0], rgb[:, -1]], axis=0)
    median = np.median(band, axis=0)
    return (int(median[0]), int(median[1]), int(median[2]))


def soft_metrics(
    image: Image.Image,
    key: RGB | None = None,
    *,
    tolerance: float = KEY_TOLERANCE,
    lo: float = PARTIAL_LO,
    hi: float = PARTIAL_HI,
) -> dict:
    """Per-source softness metrics used by the router (also handy standalone)."""
    rgb = np.asarray(image.convert("RGB"), dtype=np.float64)
    if key is None:
        key = _key_from_border(rgb)
    key_arr = np.asarray(key, dtype=np.float64)
    distance = np.sqrt(((rgb - key_arr) ** 2).sum(axis=2))
    # Cheap per-pixel alpha proxy: 0 at the key, 1 far from it.
    alpha = np.clip(distance / tolerance, 0.0, 1.0)
    partial = (alpha > lo) & (alpha < hi)
    core = alpha >= hi
    n_partial = int(partial.sum())
    n_core = int(core.sum())
    # Soft fraction of the NON-key content: opaque art ~0, glow/soft art high.
    score = n_partial / max(1, n_partial + n_core)
    if n_core > 0 and n_partial > 0:
        depth_map = ndimage.distance_transform_edt(~core)
        depth_median = float(np.median(depth_map[partial]))
        depth90 = float(np.percentile(depth_map[partial], 90))
    elif n_partial > 0:
        # All soft, no opaque core (pure glow) -> definitively soft.
        depth_median = depth90 = float(max(image.size))
    else:
        depth_median = depth90 = 0.0
    return {
        "soft_score": round(score, 4),
        "depth_median": round(depth_median, 2),
        "depth90": round(depth90, 2),
        "n_core": n_core,
        "n_partial": n_partial,
        "key": (int(key_arr[0]), int(key_arr[1]), int(key_arr[2])),
    }


def route_cutout(
    image: Image.Image,
    key: RGB | None = None,
    *,
    score_threshold: float = SCORE_THRESHOLD,
    depth90_threshold: float = DEPTH90_THRESHOLD,
) -> RouteDecision:
    """Decide key_matte vs dual_plate for one flat-key source crop."""
    m = soft_metrics(image, key)
    by_area = m["soft_score"] >= score_threshold
    by_depth = m["depth90"] >= depth90_threshold
    needs_dual = by_area or by_depth
    if needs_dual:
        bits = []
        if by_area:
            bits.append(f"soft_score {m['soft_score']:.3f} >= {score_threshold}")
        if by_depth:
            bits.append(f"depth90 {m['depth90']:.1f}px >= {depth90_threshold}")
        reason = "wide/deep semi-transparent zone (" + " and ".join(bits) + ") -> dual_plate"
    else:
        reason = (
            f"opaque: soft_score {m['soft_score']:.3f} < {score_threshold} and "
            f"depth90 {m['depth90']:.1f}px < {depth90_threshold} -> key_matte"
        )
    return RouteDecision(
        method="dual_plate" if needs_dual else "key_matte",
        needs_dual=needs_dual,
        soft_score=m["soft_score"],
        depth90=m["depth90"],
        depth_median=m["depth_median"],
        key=m["key"],
        n_core=m["n_core"],
        n_partial=m["n_partial"],
        reason=reason,
    )


def _parse_rgb(text: str) -> RGB:
    parts = [int(p) for p in text.replace(" ", "").split(",")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("expected r,g,b")
    return (parts[0], parts[1], parts[2])


def _parse_crop(text: str) -> tuple[int, int, int, int]:
    parts = [int(p) for p in text.replace(" ", "").split(",")]
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("expected x,y,w,h")
    return (parts[0], parts[1], parts[2], parts[3])


def _launch_dual_plate(source: Path, name: str, out_dir: Path) -> int:
    script = ROOT / ".codex" / "skills" / "delegated-image-generation" / "scripts" / "gen_dual_plate.sh"
    cmd = ["bash", str(script), "--source", str(source), "--name", name, "--out-dir", str(out_dir)]
    print(">>> auto-trigger:", " ".join(cmd))
    try:
        return subprocess.run(cmd, check=False).returncode
    except FileNotFoundError:
        print("error: could not launch bash; run the command above manually", file=sys.stderr)
        return 127


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Route a flat-key source to key_matte or dual_plate.")
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--key", type=_parse_rgb, default=None, help="flat-key color r,g,b (default: auto from border)")
    parser.add_argument("--crop", type=_parse_crop, default=None, help="x,y,w,h sub-crop to route")
    parser.add_argument("--json", action="store_true", help="emit the decision as JSON")
    parser.add_argument("--auto-dual", action="store_true", help="if soft, launch gen_dual_plate.sh automatically")
    parser.add_argument("--name", default=None, help="asset name for --auto-dual (default: image stem)")
    parser.add_argument("--out-dir", type=Path, default=Path("tmp/dual"), help="--auto-dual output dir")
    args = parser.parse_args(argv)

    image = Image.open(args.image).convert("RGBA")
    if args.crop:
        x, y, w, h = args.crop
        image = image.crop((x, y, x + w, y + h))
    decision = route_cutout(image, args.key)

    if args.json:
        print(json.dumps(asdict(decision)))
    else:
        print(f"method   : {decision.method}")
        print(f"reason   : {decision.reason}")
        print(f"metrics  : soft_score={decision.soft_score} depth90={decision.depth90} "
              f"core={decision.n_core} partial={decision.n_partial} key={decision.key}")

    if decision.needs_dual and args.auto_dual:
        name = args.name or args.image.stem
        return _launch_dual_plate(args.image, name, args.out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
