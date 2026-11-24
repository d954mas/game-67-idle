"""Check an outfit script: rebuild its sidecar in scratch and compare the bytes, then run the gates on what it built.

    python features/garment-cloth/tools/garment/check.py content/garments/<outfit>.py
        [--sidecar PATH] [--gates G1,...,G10] [--stages N] [--clips walk,sit] [--cards DIR] [--report FILE]

Prints one JSON finding per gate and piece (see gates.py), then a summary line, and exits 1 when an enforced finding
fails: the same enforcement table the cook applies (gates.ENFORCED_FROM) for the stage the build holds, so for the
declared stage the exit code is the cook's decision. A failing gate never stops the sidecar: the findings are the
content to-do. --sidecar is the committed file to compare (default: beside the model); --cards writes a card of each
failure's worst frame and one per corrective (the reference at its key without and with it, and the cloth the
runtime holds on it). A clip name that is not one of the outfit's is refused.

The rebuild keeps a committed corrective whose recorded inputs all match (no host is needed for it) and names every
input that differs when the committed sidecar is stale. Each corrective is gated at its key with the reference the
game evaluates there (G4-G8 on rest + delta), and a clip whose manifest entry names a corrective the outfit lacks is
reported ("correctives")."""
import argparse
import copy
import json
import os
import runpy
import sys
import tempfile
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from garment import build as Bd, cards, gates as G, model as M  # noqa: E402
from garment.outfit import Outfit  # noqa: E402
from garment.sidecar import read  # noqa: E402



def build(script):
    """Run the script with its sidecar written to scratch: (outfit, bytes)."""
    with tempfile.TemporaryDirectory() as scratch:
        previous = os.environ.get("GARMENT_SIDECAR_DIR")
        os.environ["GARMENT_SIDECAR_DIR"] = scratch
        try:
            runpy.run_path(str(script), run_name="__main__")
        finally:
            if previous is None:
                del os.environ["GARMENT_SIDECAR_DIR"]
            else:
                os.environ["GARMENT_SIDECAR_DIR"] = previous
        outfit = Outfit.instances[-1]
        written = sorted(Path(scratch).glob("*.cloth.glb"))
        if len(written) != 1:
            raise RuntimeError(f"{script}: the script must write one sidecar (o.write()), wrote {len(written)}")
        return outfit, written[0].read_bytes()


def rebuild(fresh, committed_path, root, script):
    finding = dict(gate="rebuild", piece=None, enforced=True, sidecar=str(committed_path))
    if not committed_path.is_file():
        return dict(finding, failed=True, reason="no committed sidecar")
    committed = committed_path.read_bytes()
    if committed == fresh:
        return dict(finding, failed=False)
    from garment.cook import stale
    old, new = read(committed), read(fresh)
    changed = stale(old, root, script) + [f"build record: {d}" for d in Bd.differences(old.build, new.build)]
    if changed:
        return dict(finding, failed=True, reason="stale", inputs=sorted(set(changed)))
    return dict(finding, failed=True, reason="same script and inputs, different bytes: the build is not deterministic")


def run_gates(outfit, data, wanted, clip_names=None, workers=None):
    gd, model, body, sc = G.gate_data(data, outfit.root, outfit.body)
    findings = G.run(gd, model, body, sc, wanted, clip_names, workers)
    return findings + G.corrective_coverage(sc, outfit.clips)


def pose_matrices(outfit, clip, t):
    if clip == G.REST:
        return None
    path = dict(M.clips(outfit.clips))[clip]
    glb = outfit.model.glb
    return M.skin_matrices(glb, outfit.model.joints, glb.inverse_binds(), M.clip_worlds(glb, M.load(path), [t]))[0]


def posed_shapes(outfit, matrices):
    if outfit.colliders is None:
        return None
    doc = json.loads(outfit.colliders.read_text(encoding="utf-8"))
    spheres = []
    for s in doc["spheres"]:
        centre = np.r_[s["center"], 1.0]
        if matrices is not None:
            centre = matrices[outfit.model.joints.index(s["bone"])] @ centre
        spheres.append((centre[:3], s["radius"]))
    return dict(spheres=spheres, cones=[(spheres[a], spheres[b]) for a, b in doc["cones"]])


def failure_card(outfit, finding, folder):
    worst = finding["worst"]
    if finding["gate"] in ("G9", "G10", "bake", "correctives", "sheet") or not worst:
        return None
    piece_id = finding["piece"].split(">")[0].split("/")[0]
    piece = outfit.resolved_piece(piece_id)
    name = f"{finding['gate']}-{Path(outfit.model_path).stem}-{finding['piece'].replace('/', '-').replace('>', '-over-')}"
    if finding.get("corrective"):
        # The corrected reference fails here, so the card draws it: the piece's rest moved by the corrective.
        from garment import corrective as C
        record = next(r for r in outfit.corrective_records.values() if r["name"] == finding["corrective"])
        _, worlds = C.key_of(outfit, record)
        glb = outfit.model.glb
        matrices = M.skin_matrices(glb, outfit.model.joints, glb.inverse_binds(), worlds)[0]
        piece = copy.copy(piece)
        piece.x = piece.x + piece.correctives[finding["corrective"]][0]
        name += f"-{finding['corrective']}"
    else:
        matrices = pose_matrices(outfit, worst["clip"], worst["t"])
    name += ".png"
    value = f"{worst['value_mm']} mm" if worst.get("value_mm") is not None else ""
    if finding.get("corrective"):
        title = (f"{finding['gate']} {finding['piece']} with corrective {finding['corrective']} at its key ({worst['clip']}) "
                 f"{value}; marked: the failing sim vertices")
    elif finding["frames"] > 1:
        title = (f"{finding['gate']} {finding['piece']}: worst at {worst['clip']} @ {worst['t']} s {value}; "
                 f"{finding['frames_failed']} of {finding['frames']} frames fail")
    else:
        counts = ", ".join(f"{k.replace('_', ' ')} {v}" for k, v in finding.get("counts", {}).items())
        title = f"{finding['gate']} {finding['piece']} at rest {value}: {counts}; marked: the sim vertices behind it"
    cards.piece_card(Path(folder) / name, piece, "fail", matrices=matrices, marks=worst["vertices"][:12],
                     shapes=posed_shapes(outfit, matrices) if finding["gate"] in ("G6", "G7") else None, title=title,
                     yaws=(0, 90, 180, 270))
    return str(Path(folder) / name)


def corrective_cards(outfit, data, folder):
    """A card per corrective: the reference at its key without it, with it, and the cloth the runtime holds on it after
    a reset there (heat: distance from the corrected reference). Needs the garment_host library."""
    from garment import corrective as C
    out = []
    sc = read(data)
    for record in sc.correctives:
        path = Path(folder) / f"corrective-{Path(outfit.model_path).stem}-{record['name']}.png"
        C.card(outfit, sc, record, path)
        out.append(str(path))
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("script", type=Path)
    parser.add_argument("--sidecar", type=Path)
    parser.add_argument("--gates", default=",".join(G.ALL_GATES))
    parser.add_argument("--clips")
    parser.add_argument("--cards", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--workers", type=int)
    parser.add_argument("--stages", type=int, help="build the script's content up to this stage")
    args = parser.parse_args(argv)
    if args.stages is not None:
        os.environ["GARMENT_STAGES"] = str(args.stages)
    outfit, data = build(args.script.resolve())
    committed = args.sidecar or outfit.sidecar_path()
    findings = [rebuild(data, committed, outfit.root, args.script.resolve())]
    try:
        findings += run_gates(outfit, data, set(args.gates.split(",")), args.clips and set(args.clips.split(",")),
                              args.workers)
    except ValueError as error:
        raise SystemExit(f"{args.script.name}: {error}")
    if args.cards:
        args.cards.mkdir(parents=True, exist_ok=True)
        for f in findings:
            if f.get("failed") and f["gate"] != "rebuild":
                f["card"] = failure_card(outfit, f, args.cards)
        for card in corrective_cards(outfit, data, args.cards):
            print(json.dumps(dict(card=card)))
    for f in findings:
        print(json.dumps(f))
    failed = [f for f in findings if f.get("failed") and f["enforced"]]
    reported = [f for f in findings if f.get("failed") and not f["enforced"]]
    print(f"{args.script.name}: {len(failed)} enforced failures, {len(reported)} reported", file=sys.stderr)
    if args.report:
        args.report.write_text(json.dumps(findings, indent=1) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
