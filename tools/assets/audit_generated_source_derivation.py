#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image

ROOT = Path.cwd()
REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.assets.chroma_key_alpha import key_to_alpha


def project_path(path: str) -> Path:
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    return ROOT / candidate


def iter_crops(manifest: dict[str, Any]) -> list[tuple[dict[str, Any], dict[str, Any]]]:
    entries: list[tuple[dict[str, Any], dict[str, Any]]] = []
    for source in manifest.get("sources", []):
        if not isinstance(source, dict):
            continue
        for crop in source.get("crops", []):
            if isinstance(crop, dict):
                entries.append((source, crop))
    return entries


def parse_hex_color(value: Any) -> tuple[int, int, int] | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if text.startswith("#"):
        text = text[1:]
    if len(text) != 6:
        return None
    try:
        return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))
    except ValueError:
        return None


def compare_images(reference: Image.Image, output: Image.Image, diff_threshold: int) -> dict[str, Any]:
    ref = reference.convert("RGBA")
    out = output.convert("RGBA")
    if ref.size != out.size:
        return {
            "size_match": False,
            "reference_size": [ref.width, ref.height],
            "output_size": [out.width, out.height],
            "visible_pixels": 0,
            "changed_pixels": 0,
            "changed_ratio": 1.0,
            "mean_rgb_delta": 255.0,
        }

    ref_pixels = ref.load()
    out_pixels = out.load()
    visible = 0
    changed = 0
    rgb_delta_sum = 0
    for y in range(ref.height):
        for x in range(ref.width):
            rr, rg, rb, ra = ref_pixels[x, y]
            or_, og, ob, oa = out_pixels[x, y]
            if ra <= 12 and oa <= 12:
                continue
            visible += 1
            rgb_delta = abs(rr - or_) + abs(rg - og) + abs(rb - ob)
            alpha_delta = abs(ra - oa)
            rgb_delta_sum += rgb_delta
            if rgb_delta + alpha_delta > diff_threshold:
                changed += 1

    changed_ratio = changed / max(1, visible)
    mean_rgb_delta = rgb_delta_sum / max(1, visible) / 3.0
    return {
        "size_match": True,
        "reference_size": [ref.width, ref.height],
        "output_size": [out.width, out.height],
        "visible_pixels": visible,
        "changed_pixels": changed,
        "changed_ratio": changed_ratio,
        "mean_rgb_delta": mean_rgb_delta,
    }


def audit_crop(
    source: dict[str, Any],
    crop: dict[str, Any],
    *,
    max_changed_ratio: float,
    max_mean_rgb_delta: float,
    diff_threshold: int,
    source_key: tuple[int, int, int] | None,
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "id": crop.get("id", ""),
        "kind": crop.get("kind", ""),
        "source": source.get("path", ""),
        "output": crop.get("output", ""),
        "problems": [],
    }
    if crop.get("allow_procedural_redraw") is True:
        result["skipped"] = "allow_procedural_redraw"
        return result
    if crop.get("kind") not in ("slice9", "border", "tile", "sprite"):
        result["skipped"] = "unsupported_kind"
        return result
    if not isinstance(source.get("path"), str) or not source["path"]:
        result["problems"].append("missing source path")
        return result
    if not isinstance(crop.get("output"), str) or not crop["output"]:
        result["problems"].append("missing output path")
        return result
    rect = crop.get("rect")
    if not isinstance(rect, list) or len(rect) != 4:
        result["problems"].append("missing rect [x,y,w,h]")
        return result

    source_path = project_path(source["path"])
    output_path = project_path(crop["output"])
    if not source_path.exists():
        result["problems"].append(f"missing source file: {source['path']}")
        return result
    if not output_path.exists():
        result["problems"].append(f"missing output file: {crop['output']}")
        return result

    x, y, width, height = [int(value) for value in rect]
    source_image = Image.open(source_path).convert("RGBA")
    key = source_key if source_key is not None else (255, 0, 255)
    reference = key_to_alpha(source_image.crop((x, y, x + width, y + height)), key=key)
    output = Image.open(output_path).convert("RGBA")
    stats = compare_images(reference, output, diff_threshold)
    result.update(stats)

    if not stats["size_match"]:
        result["problems"].append("output dimensions do not match source crop; derivation is not directly auditable")
        return result
    if stats["changed_ratio"] > max_changed_ratio:
        result["problems"].append(
            f"too many changed visible pixels: {stats['changed_ratio']:.3f} > {max_changed_ratio:.3f}"
        )
    if stats["mean_rgb_delta"] > max_mean_rgb_delta:
        result["problems"].append(
            f"mean RGB delta too high: {stats['mean_rgb_delta']:.2f} > {max_mean_rgb_delta:.2f}"
        )
    return result


def render_markdown(report: dict[str, Any]) -> str:
    lines = [
        "---",
        "type: GeneratedSourceDerivationAudit",
        f"crop_manifest: {report['crop_manifest']}",
        f"verdict: {report['verdict']}",
        "---",
        "",
        "# Generated Source Derivation Audit",
        "",
        f"Verdict: **{report['verdict'].upper()}**",
        "",
        f"Assets checked: {report['assets_checked']}",
        f"Assets skipped: {report['assets_skipped']}",
        f"Problems: {len(report['problems'])}",
        "",
    ]
    if report["problems"]:
        lines.extend(["## Problems", ""])
        for problem in report["problems"]:
            lines.append(f"- {problem}")
        lines.append("")
    lines.extend(["## Asset Summary", ""])
    for asset in report["assets"]:
        status = "SKIP" if asset.get("skipped") else ("FAIL" if asset.get("problems") else "PASS")
        if asset.get("skipped"):
            lines.append(f"- {status} `{asset.get('id')}` ({asset.get('kind')}): {asset.get('skipped')}")
        else:
            lines.append(
                f"- {status} `{asset.get('id')}` ({asset.get('kind')}): "
                f"size={asset.get('output_size', '-')}, changed={asset.get('changed_ratio', '-')}, "
                f"mean_rgb_delta={asset.get('mean_rgb_delta', '-')}"
            )
    lines.append("")
    return "\n".join(lines)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit that runtime UI PNGs are directly derived from generated source-sheet crops, not redrawn procedurally."
    )
    parser.add_argument("--crop-manifest", required=True)
    parser.add_argument("--json-output")
    parser.add_argument("--report")
    parser.add_argument("--max-changed-ratio", type=float, default=0.08)
    parser.add_argument("--max-mean-rgb-delta", type=float, default=10.0)
    parser.add_argument("--diff-threshold", type=int, default=36)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    manifest_path = project_path(args.crop_manifest)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    source_key = parse_hex_color(manifest.get("green_screen", {}).get("key"))
    assets = [
        audit_crop(
            source,
            crop,
            max_changed_ratio=args.max_changed_ratio,
            max_mean_rgb_delta=args.max_mean_rgb_delta,
            diff_threshold=args.diff_threshold,
            source_key=source_key,
        )
        for source, crop in iter_crops(manifest)
    ]
    checked = [asset for asset in assets if not asset.get("skipped")]
    skipped = [asset for asset in assets if asset.get("skipped")]
    problems = [f"{asset['id']}: {problem}" for asset in checked for problem in asset.get("problems", [])]
    report = {
        "schema": "game.generated_source_derivation_audit",
        "version": 1,
        "crop_manifest": args.crop_manifest.replace("\\", "/"),
        "verdict": "fail" if problems else "pass",
        "assets_checked": len(checked),
        "assets_skipped": len(skipped),
        "problems": problems,
        "assets": assets,
        "thresholds": {
            "max_changed_ratio": args.max_changed_ratio,
            "max_mean_rgb_delta": args.max_mean_rgb_delta,
            "diff_threshold": args.diff_threshold,
        },
    }
    if args.json_output:
        json_path = project_path(args.json_output)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if args.report:
        report_path = project_path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(render_markdown(report), encoding="utf-8")

    print(f"{report['verdict']}: checked {len(checked)} generated-source asset(s), skipped {len(skipped)}")
    for problem in problems:
        print(f"problem: {problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
