#!/usr/bin/env python3
"""Expand a declarative pack config into a gen_batch jobs.json (T0330).

gen_batch.py can run a jobs.json, but nothing generated one from a flat
declarative config: style + subject template + wildcard axes (material,
shape, grade...). This is that missing piece, sheet-first (not one prompt per
asset): `sheet.vary` is the ONLY axis that varies within a sheet (per cell,
row-major); every OTHER axis's cartesian product picks the sheets. See
../references/build_spec_pack_expander_2026-07-07.md for the full spec (this
script implements it exactly; that doc is LAW).

`expand(config) -> list[dict]` is a pure function: no network/API calls, no
subprocess, deterministic (same config -> byte-identical jobs.json). It reads
the anchor file's existence (if set) so a bad path fails loud BEFORE any paid
call, but nothing else touches the filesystem. All config problems raise
SystemExit with a clear message, same idiom as generate_image.py.

Usage:
  py -3.12 expand_jobs.py --config pack_config.json [--out tmp/packs/x/jobs.json]
"""
from __future__ import annotations

import argparse
import itertools
import json
import re
import sys
from pathlib import Path
from typing import Any

MAX_VARY = 9  # readability ceiling: ~9 tiles on a 1024^2 sheet is the legibility limit
MAX_PROMPT_BYTES = 20 * 1024  # Windows argv headroom (~32k); a normal sheet prompt is 3-5 KB
DEFAULT_MAX_JOBS = 12  # no-scale-to-1000 law: gen is 30-60s/call and billed
SIZE_WHITELIST = {"1024x1024", "1536x1024", "1024x1536"}
BACKGROUND_VALUES = {"magenta", "green", "transparent"}
BACKGROUND_HEX = {"magenta": "#FF00FF", "green": "#00FF00"}

REPO_ROOT = Path(__file__).resolve().parents[4]  # same derivation as slice_pack.py's REPO_ROOT


def slugify(value: Any, fallback: str = "x") -> str:
    """Path-safe slug: lowercase [a-z0-9_-], runs of anything else collapsed to
    one '-', <=80 chars. Same spirit as slice_regions.py's safe_name -- raw
    axis values are never lost, they stay in cells[].axes; this is ONLY for
    building filesystem paths (Cyrillic/spaces/slashes must not break them)."""
    text = str(value).strip().lower()
    text = re.sub(r"[^a-z0-9_-]+", "-", text)
    text = re.sub(r"-{2,}", "-", text).strip("-")
    return text[:80] or fallback


def axes_slug(axes: dict[str, Any]) -> str:
    """`<axis>-<slug(value)>` per axis joined by '__', in dict (JSON) order.
    Shared with slice_pack.py so sheet paths and cut names use one convention."""
    return "__".join(f"{axis}-{slugify(value)}" for axis, value in axes.items())


def _require(config: dict, key: str) -> Any:
    if key not in config:
        raise SystemExit(f"expand_jobs: config.{key} is required")
    return config[key]


def _sheet_stem(pack: str, big_axis_names: list[str], combo: tuple) -> str:
    if not big_axis_names:
        return slugify(pack)
    return "__".join(f"{axis}-{slugify(value)}" for axis, value in zip(big_axis_names, combo))


def _candidate_out(out: str, index: int) -> str:
    path = Path(out)
    return path.with_name(f"{path.stem}__c{index}{path.suffix}").as_posix()


def _build_prompt(*, rows: int, cols: int, subject_line: str, vary_axis: str,
                   vary_values: list, style_prefix: str, background: str) -> str:
    cell_lines = []
    for idx, value in enumerate(vary_values, start=1):
        tag = " (top-left)" if idx == 1 else ""
        cell_lines.append(f"cell {idx}{tag}: {vary_axis}={value}")
    subject = f"{subject_line} " + "; ".join(cell_lines) + "."

    composition = [
        "Single consistent scale across all cells.",
        "Each object centered within its cell with generous gutter margins.",
        "No overlapping between cells, no borders, no grid lines.",
    ]
    if len(vary_values) < rows * cols:
        composition.append("Remaining cells: empty background.")

    background_text = f"Solid uniform {BACKGROUND_HEX[background]} background, no gradients, no texture."

    sections = [
        ("TASK", f"Sheet generation: {rows}x{cols} grid, one object per filled cell."),
        ("SUBJECT", subject),
        ("STYLE", style_prefix),
        ("COMPOSITION", " ".join(composition)),
        ("BACKGROUND", background_text),
        ("CONSTRAINTS", "No text, no labels, no watermark, no grid lines. "
                        "Identical, consistent lighting across all cells."),
        ("OUTPUT", f"One sheet image, {rows}x{cols} grid, {len(vary_values)} filled "
                   f"cell(s) as described; any remaining cells left as empty background."),
    ]
    return "\n\n".join(f"[{name}]\n{body}" for name, body in sections)


def expand(config: dict) -> list[dict]:
    """Config (v1, flat JSON) -> list of gen_batch job dicts. Pure and
    deterministic: same config in, byte-identical jobs out. See module
    docstring + build_spec_pack_expander_2026-07-07.md for the contract."""
    pack = _require(config, "pack")
    style_prefix = _require(config, "style_prefix")
    subject_template = _require(config, "subject_template")
    axes = _require(config, "axes")
    sheet = _require(config, "sheet")
    out_dir = _require(config, "out_dir")
    background = _require(config, "background")

    if not isinstance(axes, dict) or not axes:
        raise SystemExit("expand_jobs: config.axes must be a non-empty object")

    vary_axis = sheet.get("vary")
    if vary_axis not in axes:
        raise SystemExit(f"expand_jobs: sheet.vary '{vary_axis}' is not a key of config.axes (known axes: {list(axes)})")
    grid = sheet.get("grid")
    if not isinstance(grid, (list, tuple)) or len(grid) != 2:
        raise SystemExit("expand_jobs: sheet.grid must be [rows, cols]")
    rows, cols = int(grid[0]), int(grid[1])
    if rows <= 0 or cols <= 0:
        raise SystemExit("expand_jobs: sheet.grid must be positive [rows, cols]")

    vary_values = axes[vary_axis]
    if not isinstance(vary_values, list) or not vary_values:
        raise SystemExit(f"expand_jobs: axes.{vary_axis} must be a non-empty list")
    n_vary = len(vary_values)
    if n_vary > MAX_VARY:
        raise SystemExit(
            f"expand_jobs: sheet.vary '{vary_axis}' has {n_vary} values > hard ceiling {MAX_VARY} "
            f"(per-sheet readability limit); trim axes.{vary_axis} or split the pack"
        )
    if n_vary > rows * cols:
        raise SystemExit(
            f"expand_jobs: sheet.vary '{vary_axis}' has {n_vary} values but grid {rows}x{cols} "
            f"only holds {rows * cols} cell(s); enlarge sheet.grid or trim axes.{vary_axis}"
        )

    # Two raw vary values that slugify identically would collide in slice_pack's
    # per-cell region naming -- silently suffixed _002 there, i.e. a mislabel, not a crash.
    vary_slugs: dict[str, Any] = {}
    for value in vary_values:
        slug = slugify(value)
        if slug in vary_slugs:
            raise SystemExit(
                f"expand_jobs: axes.{vary_axis} values {vary_slugs[slug]!r} and {value!r} both "
                f"slugify to '{slug}' -- slice_pack would silently mislabel cells; rename one value"
            )
        vary_slugs[slug] = value

    big_axis_names = [name for name in axes if name != vary_axis]
    for axis in big_axis_names:
        values = axes[axis]
        if not isinstance(values, list) or not values:
            raise SystemExit(f"expand_jobs: axes.{axis} must be a non-empty list")
        if f"{{{axis}}}" not in subject_template:
            raise SystemExit(
                f"expand_jobs: big axis '{axis}' has no {{{axis}}} slot in subject_template "
                f"(an axis that doesn't affect the prompt is a lying config)"
            )

    if background not in BACKGROUND_VALUES:
        raise SystemExit(f"expand_jobs: config.background '{background}' must be one of {sorted(BACKGROUND_VALUES)}")
    if background == "transparent":
        raise SystemExit(
            "expand_jobs: config.background 'transparent' requires the REST (sk-) path; the codex "
            "backend rejects transparent on every model and v1 of this expander only targets the "
            "codex path. Use magenta or green + key_matte cutout instead."
        )

    gen = config.get("gen") or {}
    size = gen.get("size", "1024x1024")
    if size not in SIZE_WHITELIST:
        raise SystemExit(f"expand_jobs: gen.size '{size}' not in whitelist {sorted(SIZE_WHITELIST)}")
    quality = gen.get("quality", "high")
    model = gen.get("model", "gpt-image-2")

    candidates = int(config.get("candidates", 1))
    if candidates < 1:
        raise SystemExit("expand_jobs: config.candidates must be a positive integer")
    max_jobs = int(config.get("max_jobs", DEFAULT_MAX_JOBS))

    big_value_lists = [axes[name] for name in big_axis_names]
    combos = list(itertools.product(*big_value_lists))  # [] big axes -> product() -> one empty combo
    sheets_count = len(combos)
    jobs_total = sheets_count * candidates
    if jobs_total > max_jobs:
        cardinalities = ", ".join(
            f"{name}={len(axes[name])}{'(vary)' if name == vary_axis else '(big)'}" for name in axes
        )
        raise SystemExit(
            f"expand_jobs: {sheets_count} sheet(s) x {candidates} candidate(s) = {jobs_total} jobs "
            f"> max_jobs={max_jobs}; axes: {cardinalities}"
        )

    anchor = config.get("anchor")
    if anchor:
        anchor_path = Path(anchor)
        if not anchor_path.is_absolute():
            # CWD-independent contract: resolve relative anchors against the repo root
            # (same derivation as slice_pack.py's REPO_ROOT), not the caller's CWD.
            anchor_path = (REPO_ROOT / anchor_path).resolve()
        if not anchor_path.exists():
            raise SystemExit(f"expand_jobs: config.anchor '{anchor}' does not exist (checked before any paid call)")
        anchor = anchor_path.as_posix()

    out_dir_posix = str(out_dir).rstrip("/\\").replace("\\", "/")

    jobs: list[dict] = []
    seen_stems: dict[str, dict] = {}
    for combo in combos:
        big_values = dict(zip(big_axis_names, combo))
        try:
            subject_line = subject_template.format(**big_values)
        except KeyError as exc:
            raise SystemExit(f"expand_jobs: subject_template references unknown axis {exc}") from exc

        prompt = _build_prompt(
            rows=rows, cols=cols, subject_line=subject_line, vary_axis=vary_axis,
            vary_values=vary_values, style_prefix=style_prefix, background=background,
        )
        prompt_bytes = len(prompt.encode("utf-8"))
        if prompt_bytes > MAX_PROMPT_BYTES:
            raise SystemExit(
                f"expand_jobs: assembled prompt is {prompt_bytes} bytes > {MAX_PROMPT_BYTES} byte "
                f"guard; shorten style_prefix/subject_template"
            )

        stem = _sheet_stem(pack, big_axis_names, combo)
        # Two raw big-axis value combos that slugify identically would silently
        # overwrite each other's out path (two jobs writing the same PNG under
        # concurrent gen_batch).
        if stem in seen_stems:
            raise SystemExit(
                f"expand_jobs: big-axis values {seen_stems[stem]!r} and {big_values!r} both "
                f"slugify to sheet stem '{stem}' -- rename one so out paths do not collide"
            )
        seen_stems[stem] = big_values
        out = f"{out_dir_posix}/{stem}.png"
        label = pack if not big_axis_names else f"{pack}: " + " ".join(str(v) for v in combo)

        cells = [
            {
                "cell": [idx // cols, idx % cols],
                "axes": {axis: (value if axis == vary_axis else big_values[axis]) for axis in axes},
            }
            for idx, value in enumerate(vary_values)
        ]

        for c in range(1, candidates + 1):
            job_out = out if candidates == 1 else _candidate_out(out, c)
            job_name = label if candidates == 1 else f"{label} (c{c})"
            job: dict = {
                "prompt": prompt,
                "out": job_out,
                "name": job_name,
                "size": size,
                "quality": quality,
                "model": model,
            }
            if anchor:
                job["input_image"] = [anchor]
            job["pack"] = pack
            job["cells"] = cells
            jobs.append(job)

    return jobs


def _summary(config: dict, jobs: list[dict]) -> str:
    axes = config["axes"]
    vary_axis = config["sheet"]["vary"]
    candidates = int(config.get("candidates", 1))
    sheets_count = len(jobs) // candidates if candidates else len(jobs)
    cardinalities = ", ".join(
        f"{name}={len(values)}{'(vary)' if name == vary_axis else '(big)'}" for name, values in axes.items()
    )
    return (
        f"expand_jobs: pack='{config['pack']}' sheets={sheets_count} candidates={candidates} "
        f"jobs={len(jobs)} (max_jobs={config.get('max_jobs', DEFAULT_MAX_JOBS)})\n"
        f"axes: {cardinalities}"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description="Expand a declarative pack config into a gen_batch jobs.json.")
    ap.add_argument("--config", required=True, help="pack config JSON (see build_spec_pack_expander)")
    ap.add_argument("--out", help="output jobs.json path (default: <out_dir>/jobs.json)")
    a = ap.parse_args()

    config_path = Path(a.config)
    try:
        config = json.loads(config_path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise SystemExit(f"expand_jobs: cannot read --config {config_path}: {exc}")
    except json.JSONDecodeError as exc:
        raise SystemExit(f"expand_jobs: --config {config_path} is not valid JSON: {exc}")

    jobs = expand(config)

    out_path = Path(a.out) if a.out else Path(str(config["out_dir"]).rstrip("/\\")) / "jobs.json"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(jobs, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")

    print(_summary(config, jobs))
    print(f"wrote {out_path.as_posix()} ({len(jobs)} job(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
