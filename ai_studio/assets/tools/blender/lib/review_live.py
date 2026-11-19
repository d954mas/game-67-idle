"""Review what a build script just made, inside the same Blender run: export, render, check, card.

    from ai_studio.assets.tools.blender.lib import review_live
    ...build the model...
    print(review_live.review_scene("tmp/review/crate", name="crate"))

One Blender start builds and reviews, and the scene comes out unchanged, so a script can review
several stages. Renders use the current pose and modifiers (what the scene shows); checks run on
the exported GLB (what the engine loads). The card needs Pillow, which Blender lacks, so it is
composed by the Studio venv Python; without a venv the card is skipped and the result says so.
"""
import json
import os
import subprocess
import time
from pathlib import Path

import bpy

from ai_studio.assets.tools.blender import glb_check, mesh_check
from ai_studio.assets.tools.blender import render_views as RV
from ai_studio.assets.tools.blender.lib import bpy_io
from ai_studio.assets.tools.blender.lib.glb import Glb
from ai_studio.assets.tools.blender.lib.studio_root import find_studio_root


def studio_python(root=None):
    """The Studio venv interpreter, or None when the checkout has no venv."""
    root = Path(root or find_studio_root(Path(__file__).parent))
    for candidate in (root / ".venv" / "Scripts" / "python.exe", root / ".venv" / "bin" / "python"):
        if candidate.is_file():
            return candidate
    return None


def _default_objects():
    return [o for o in bpy.context.scene.objects if o.visible_get() and o.type in ("MESH", "ARMATURE", "EMPTY")]


def _write(path, data):
    Path(path).write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def run_checks(glb_path):
    """mesh_check and glb_check on the written file; a check that crashes is reported, not raised,
    because a review must never cost the build that called it."""
    reports, errors = [], {}
    glb = Glb.load(glb_path)
    tools = [glb_check] if not any("mesh" in n for n in glb.nodes) else [mesh_check, glb_check]
    for tool in tools:
        try:
            reports.append(tool.check(glb))
        except Exception as error:  # noqa: BLE001 - surfaced in the result
            errors[tool.__name__.rsplit(".", 1)[-1]] = f"{type(error).__name__}: {error}"
    return reports, errors


def compose_card(out, views_path, report_paths, timeout=120):
    """card.png through review_card.py in the venv; returns (path or None, reason when None)."""
    python = studio_python()
    if python is None:
        return None, "no Studio venv (.venv) found; run review_card.py on views.json with Pillow"
    script = Path(__file__).resolve().parents[1] / "review_card.py"
    card = out / "card.png"
    command = [str(python), str(script), str(views_path), "--out", str(card)]
    for path in report_paths:
        command += ["--report", str(path)]
    # Blender's own Python variables would make the venv interpreter load Blender's stdlib.
    env = {k: v for k, v in os.environ.items() if k not in ("PYTHONHOME", "PYTHONPATH")}
    result = subprocess.run(command, capture_output=True, text=True, timeout=timeout, env=env)
    if result.returncode != 0 or not card.is_file():
        return None, f"review_card.py exited {result.returncode}: {(result.stderr or result.stdout)[-600:]}"
    return card, None


def review_scene(out_dir, objects=None, name="model", job=None, export_preset="static", checks=True, card=True):
    """Export `objects` (default: every visible mesh/armature/empty) to <out>/<name>.glb, render
    them in-process (render_views job keys: views, passes, beside, poses, ...), run the checks on
    the GLB and compose card.png. Returns paths, check summaries and timings (seconds)."""
    started = time.monotonic()
    out = Path(out_dir).resolve()
    out.mkdir(parents=True, exist_ok=True)
    objects = list(objects) if objects is not None else _default_objects()
    if not objects:
        raise ValueError("review_scene: nothing to review (no objects given and none visible)")
    layer = bpy.context.view_layer
    selected = {o: o.select_get() for o in layer.objects}
    active = layer.objects.active
    timings = {}

    glb_path = out / f"{name}.glb"
    export = bpy_io.export_glb(glb_path, objects, preset=export_preset)
    for obj, was in selected.items():
        obj.select_set(was)
    layer.objects.active = active
    timings["export"] = round(time.monotonic() - started, 3)

    mark = time.monotonic()
    views = RV.render_objects(objects, {**(job or {}), "out": out}, asset=glb_path, name=name)
    timings["render"] = round(time.monotonic() - mark, 3)

    reports, check_errors, report_paths = [], {}, []
    if checks:
        mark = time.monotonic()
        reports, check_errors = run_checks(glb_path)
        for report in reports:
            path = out / f"{report['tool']}.json"
            _write(path, report)
            report_paths.append(path)
        timings["checks"] = round(time.monotonic() - mark, 3)

    card_path, card_note = None, "card=False"
    if card:
        mark = time.monotonic()
        card_path, card_note = compose_card(out, out / "views.json", report_paths)
        timings["card"] = round(time.monotonic() - mark, 3)
    timings["total"] = round(time.monotonic() - started, 3)

    result = {"asset": glb_path.as_posix(), "out": out.as_posix(), "glb": export,
              "views": (out / "views.json").as_posix(), "images": len(views["images"]),
              "card": card_path.as_posix() if card_path else None, "card_note": None if card_path else card_note,
              "render_error": None,
              "checks": {r["tool"]: {"status": r["status"], "counts": r["counts"]} for r in reports},
              "check_errors": check_errors, "reports": [p.as_posix() for p in report_paths],
              "blender": bpy.app.version_string, "stats": views["stats"], "seconds": timings}
    _write(out / "review.json", result)
    return result
