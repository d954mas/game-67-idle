"""Fit an outfit's body shapes (garment.colliders.fit) on the body its script declares and the sidecar it builds, over
the outfit's clips, and write the set (garment.colliders.v1).

    python features/garment-cloth/tools/garment/fit_colliders.py content/garments/<outfit>.py <segments.json> <out.json>

The script runs into scratch first (as check.py does, without its correctives, which are baked on these colliders),
so the fit sees the sim meshes and maps of the stage it builds;
the segments are the rig's body segments (joint pairs); the clips are those the game can show the outfit in
(`Outfit.shown`). Every reference sample that fails G4 is G4's, not a shape's.
The fit needs the declared complete body; the set's asset record (`<out>.provenance.json`) is written beside it and
inherits origin and license from the body's own record."""
import hashlib
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from garment import build as Bd, check, colliders as C  # noqa: E402
from garment.sidecar import read  # noqa: E402


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 3:
        raise SystemExit(__doc__)
    script, segments_path, out = (Path(a) for a in argv)
    # The maps alone: the correctives are baked on the colliders this fit makes.
    outfit, data = check.build(script.resolve(), correctives=False)
    sc = read(data)
    if not outfit.body.complete:
        raise SystemExit(f"{script}: the fit needs the outfit's declared complete body (B.3); the script builds on the "
                         f"fallback body {sc.build['body']}")
    body_file = outfit.root / sc.build["body"]
    Bd.source_record(body_file)
    rig = C.Rig.from_glb(outfit.model.glb)
    pieces = []
    for p, authored in zip(sc.pieces, outfit.pieces):
        w = np.zeros((len(p.points), len(rig.joints)))
        np.add.at(w, (np.arange(len(w))[:, None], p.joints.astype(int)), p.weights.astype(float))
        pieces.append(C.Piece(p.id, p.points.astype(float), p.faces, p.r.astype(float), w, authored.thickness))
    # The clips the game can show the outfit in: the fit holds no reference it never shows.
    clips = C.clip_poses(outfit.model.glb, rig.joints, list(outfit.clip_paths().items()))
    segments = C.segments_from(json.loads(segments_path.read_text(encoding="utf-8")))
    g4 = C.g4_failing(outfit.body, pieces, clips)
    shapes = C.fit(outfit.body, rig, segments, pieces, clips, g4)
    C.write(out, shapes, C.sha256(body_file))
    Bd.write_record(out, outfit.root if out.resolve().is_relative_to(outfit.root) else out.parent,
                    what="The outfit's body shapes for garment-cloth (garment.colliders.v1): spheres on body joints in rest "
                         "model space and the round cones joining them, fitted by garment.colliders.fit.",
                    producer="features/garment-cloth/tools/garment/fit_colliders.py",
                    rebuild=f"python features/garment-cloth/tools/garment/fit_colliders.py {' '.join(argv)}", source=body_file,
                    fitted_on=dict(script=script.as_posix(), body=sc.build["body"], body_sha256=C.sha256(body_file),
                                   segments=segments_path.as_posix(), segments_sha256=C.sha256(segments_path),
                                   sidecar_sha256=hashlib.sha256(data).hexdigest(), library=Bd.LIBRARY_VERSION))
    print(f"{out}: {len(shapes.spheres)} spheres, {len(shapes.cones)} cones on {body_file.name}")


if __name__ == "__main__":
    main()
