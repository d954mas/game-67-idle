"""Mesh topology check of a GLB: openings, non-manifold and miswound edges, degenerate and duplicate
faces, shells and inside-out volume, per mesh node, each finding with a fix hint.

    python mesh_check.py model.glb [--profile auto|volume|surface] [--weld 1e-6] [--tri-budget N] [--inward]

Profiles: `volume` expects closed solids (openings are high findings); `surface` allows openings
(reported as info); `auto` (default) treats a node that is closed after welding as a volume.
Positions are welded first, so hard-edge and UV splits are not counted as openings.
"""
import sys
from collections import Counter
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

import numpy as np  # noqa: E402

from ai_studio.assets.tools.blender import mesh_metrics as M  # noqa: E402
from ai_studio.assets.tools.blender.lib import glb as G, rays, report as R  # noqa: E402

TOOL = "mesh_check"
DEFAULTS = {"profile": "auto", "weld": 1e-6, "area_epsilon": 1e-10, "tri_budget": None,
            "inward": False, "inward_max_faces": 20000, "scope": "node", "include": None}
SEVERITY = {
    "volume": {"boundary": "high", "non_manifold": "high", "touching": "medium", "miswound": "high",
               "inside_out": "high", "inverted_shell": "medium", "degenerate": "medium", "duplicate": "medium",
               "inward": "medium"},
    "surface": {"boundary": "info", "non_manifold": "medium", "touching": "low", "miswound": "medium",
                "inside_out": "low", "inverted_shell": "medium", "degenerate": "low", "duplicate": "medium",
                "inward": "low"},
}
# `auto` judges a part that happens to be closed as a solid, but shells built by touching or sharing
# an edge (box flaps, trim on a panel) are common in game props and render fine.
SEVERITY["auto_volume"] = {**SEVERITY["volume"], "non_manifold": "medium", "touching": "low"}
SAMPLES = 5


def _r(point):
    return [round(float(v), 4) for v in point]


def boundary_loops(points, edges):
    """Openings as (edge count, centre), largest first: one entry per hole, not per edge."""
    if not len(edges):
        return []
    label = M.face_components(np.c_[edges, edges[:, 1]], len(points))
    out = []
    for k in range(label.max() + 1):
        sel = edges[label == k]
        out.append((len(sel), _r(points[sel.reshape(-1)].mean(axis=0))))
    return sorted(out, key=lambda item: -item[0])


def located_edges(points, faces, kind):
    """Midpoints of up to SAMPLES non-manifold (kind 'non_manifold') or miswound edges."""
    a = np.concatenate([faces[:, 0], faces[:, 1], faces[:, 2]])
    b = np.concatenate([faces[:, 1], faces[:, 2], faces[:, 0]])
    real = a != b
    a, b = a[real], b[real]
    undirected, use = np.unique(np.sort(np.stack([a, b], axis=1), axis=1), axis=0, return_counts=True)
    if kind == "miswound":
        pairs, counts = np.unique(np.stack([a, b], axis=1), axis=0, return_counts=True)
        two = {tuple(e) for e in undirected[use == 2]}
        bad = np.array([e for e in pairs[counts > 1] if tuple(sorted(e)) in two]).reshape(-1, 2)
    else:
        bad = undirected[use > 2]
    return [_r((points[i] + points[j]) / 2) for i, j in bad[:SAMPLES]]


def split_non_manifold(faces, count):
    """(touching, genuine) non-manifold edges as vertex pairs, on welded faces.

    Faces are grouped into sheets joined across two-face edges only. An edge is `touching` when
    every sheet meeting there brings exactly two faces (whole shells that meet along it); a fin,
    flap or third face makes it genuine."""
    a = np.concatenate([faces[:, 0], faces[:, 1], faces[:, 2]])
    b = np.concatenate([faces[:, 1], faces[:, 2], faces[:, 0]])
    face = np.tile(np.arange(len(faces)), 3)
    real = a != b
    key = (np.minimum(a, b) * count + np.maximum(a, b))[real]
    face = face[real]
    order = np.argsort(key, kind="stable")
    key, face = key[order], face[order]
    starts = np.r_[0, np.flatnonzero(np.diff(key)) + 1]
    sizes = np.diff(np.r_[starts, len(key)])
    parent = list(range(len(faces)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for s in starts[sizes == 2].tolist():
        ra, rb = find(int(face[s])), find(int(face[s + 1]))
        if ra != rb:
            parent[ra] = rb
    touching, genuine = [], []
    for s, n in zip(starts[sizes > 2].tolist(), sizes[sizes > 2].tolist()):
        sheets = Counter(find(int(f)) for f in face[s:s + n])
        edge = [int(key[s] // count), int(key[s] % count)]
        (touching if len(sheets) > 1 and set(sheets.values()) == {2} else genuine).append(edge)
    return np.array(touching, np.int64).reshape(-1, 2), np.array(genuine, np.int64).reshape(-1, 2)


def inward_faces(points, faces, component, closed, limit):
    """Faces of closed shells whose normal points into their own shell (odd crossings along it).
    Three slightly spread rays vote, since a ray grazing a knife edge counts one crossing for two."""
    flagged, skipped = [], 0
    for k in np.flatnonzero(closed):
        idx = np.flatnonzero(component == k)
        if len(idx) > limit:
            skipped += 1
            continue
        sub = faces[idx]
        tris = rays.triangles(points, sub)
        normal = np.cross(tris[1] - tris[0], tris[2] - tris[0])
        length = np.linalg.norm(normal, axis=1)
        keep = length > 1e-12
        normal = normal[keep] / length[keep, None]
        centre = (tris[0] + tris[1] + tris[2])[keep] / 3.0
        helper = np.where(np.abs(normal[:, :1]) < 0.9, [[1.0, 0, 0]], [[0, 1.0, 0]])
        tangent = np.cross(normal, helper)
        tangent /= np.linalg.norm(tangent, axis=1, keepdims=True)
        votes = np.zeros(len(normal), int)
        extent = float(np.ptp(points[sub.reshape(-1)], axis=0).max()) or 1.0
        for spread in (0.0, 0.05, -0.05):
            d = normal + spread * tangent
            d /= np.linalg.norm(d, axis=1, keepdims=True)
            votes += rays.crossings(centre + d * 1e-5 * extent, d, tris) % 2
        flagged.extend(idx[keep][votes >= 2].tolist())
    return np.array(flagged, np.int64), skipped


def _inside_rest(t, k):
    """Whether shell k lies inside the rest of its part: an inward-facing shell there is a cavity."""
    rest = t["faces"][t["component"] != k]
    if not len(rest):
        return False
    probe = t["points"][t["faces"][t["component"] == k][0, 0]][None]
    return bool(rays.crossings(probe, np.array([0.3, 0.9, 0.1]) / np.linalg.norm([0.3, 0.9, 0.1]),
                               rays.triangles(t["points"], rest))[0] % 2)


def _groups(glb, scope, include):
    parts = glb.parts(include=include)
    groups = {}
    for part in parts:
        groups.setdefault("<file>" if scope == "file" else part.name, []).append(part)
    out = []
    for name, members in groups.items():
        base, faces = 0, []
        for part in members:
            faces.append(part.faces + base)
            base += len(part.positions)
        out.append((name, np.vstack([p.positions for p in members]), np.vstack(faces)))
    return out


def check_part(name, points, faces, opts):
    findings = []
    t = M.triangle_topology(points, faces, weld_tolerance=opts["weld"], area_epsilon=opts["area_epsilon"])
    closed = t["boundary_edges"] == 0 and t["triangles"] > 0
    profile = opts["profile"] if opts["profile"] != "auto" else ("volume" if closed else "surface")
    sev = SEVERITY["auto_volume" if opts["profile"] == "auto" and closed else profile]
    where = {"part": name}
    loops = boundary_loops(t["points"], t["boundary"])
    row = {k: t[k] for k in ("triangles", "vertices", "welded_vertices", "boundary_edges", "non_manifold_edges",
                             "miswound_edges", "degenerate_faces", "duplicate_faces", "components",
                             "open_components")}
    row.update(part=name, profile=profile, boundary_loops=len(loops),
               signed_volume=round(t["signed_volume"], 6) if closed else None)

    if t["boundary_edges"]:
        findings.append(R.finding(
            "boundary", sev["boundary"],
            f"{name}: {t['boundary_edges']} open edges in {len(loops)} opening(s) after welding at {opts['weld']}",
            f"weld vertices (merge by distance) or fill the holes on {name}; if it is meant to be open, "
            "use --profile surface", {**where, "openings": [{"edges": e, "centre": c} for e, c in loops[:SAMPLES]]},
            t["boundary_edges"], 0))
    if t["non_manifold_edges"]:
        touching, genuine = split_non_manifold(t["faces"], len(t["points"]))
        for kind, edges, text, hint in (
                ("non_manifold", genuine, "edges shared by 3+ faces of one sheet",
                 f"split the extra faces off those edges, or delete them, on {name}"),
                ("touching", touching, "edges where separate shells meet",
                 f"fine for a game prop; for a clean solid on {name}, offset the shells or merge them")):
            if len(edges):
                findings.append(R.finding(kind, sev[kind], f"{name}: {len(edges)} {text}", hint, {
                    **where, "edges": [_r(t["points"][e].mean(axis=0)) for e in edges[:SAMPLES]]}, len(edges), 0))
    if t["miswound_edges"]:
        findings.append(R.finding(
            "miswound", sev["miswound"],
            f"{name}: {t['miswound_edges']} edges where neighbouring faces wind the same way (flipped faces)",
            f"recalculate normals outside on {name} (Blender: Mesh > Normals > Recalculate Outside)",
            {**where, "edges": located_edges(t["points"], t["faces"], "miswound")}, t["miswound_edges"], 0))
    if closed and t["signed_volume"] < 0:
        findings.append(R.finding(
            "inside_out", sev["inside_out"], f"{name}: closed but its volume is negative (all faces point inward)",
            f"flip normals on {name}", where, round(t["signed_volume"], 6)))
    else:
        inverted = np.flatnonzero(~t["component_open"] & (t["component_volume"] < 0))
        cavity = [k for k in inverted if _inside_rest(t, k)]
        flipped = [k for k in inverted if k not in cavity]
        for kind, shells, severity, text, hint in (
                ("inverted_shell", flipped, sev["inverted_shell"], "closed shell(s) with negative volume "
                 "(faces point inward)", f"flip normals of those shells on {name}"),
                ("cavity", cavity, "info", "inward-facing shell(s) inside another shell (cavities)", "")):
            if shells:
                findings.append(R.finding(kind, severity, f"{name}: {len(shells)} {text}", hint, {
                    **where, "shell_centres": [_r(t["points"][t["faces"][t["component"] == k].reshape(-1)].mean(0))
                                               for k in shells[:SAMPLES]]}, len(shells), 0))
    if t["degenerate_faces"]:
        findings.append(R.finding(
            "degenerate", sev["degenerate"], f"{name}: {t['degenerate_faces']} zero-area faces",
            f"dissolve degenerate faces on {name} (Mesh > Clean Up > Degenerate Dissolve)", where,
            t["degenerate_faces"], 0))
    if t["duplicate_faces"]:
        findings.append(R.finding(
            "duplicate", sev["duplicate"], f"{name}: {t['duplicate_faces']} faces repeat another face (z-fighting)",
            f"merge by distance and delete the duplicate faces on {name}", where, t["duplicate_faces"], 0))
    if opts["inward"]:
        closed_shells = ~t["component_open"]
        flagged, skipped = inward_faces(t["points"], t["faces"], t["component"], closed_shells,
                                        opts["inward_max_faces"])
        row["inward_faces"] = int(len(flagged))
        if len(flagged):
            centres = t["points"][t["faces"][flagged[:SAMPLES]]].mean(axis=1)
            findings.append(R.finding(
                "inward", sev["inward"], f"{name}: {len(flagged)} faces point into their own closed shell (folds)",
                f"fix folded geometry on {name}: reduce the offset/solidify thickness or recalculate normals",
                {**where, "face_centres": [_r(c) for c in centres]}, int(len(flagged)), 0))
        if skipped:
            findings.append(R.finding("inward", "info", f"{name}: {skipped} shell(s) over "
                                      f"{opts['inward_max_faces']} faces skipped by the inward test", "", where))
    if t["components"] > 1:
        findings.append(R.finding("components", "info", f"{name}: {t['components']} separate shells", "", where,
                                  t["components"]))
    return findings, row


def check(path_or_glb, **options):
    opts = R.resolve_options(TOOL, DEFAULTS, overrides=options)
    if opts["profile"] not in ("auto", "volume", "surface"):
        raise ValueError(f"{TOOL}: profile must be auto, volume or surface, not {opts['profile']!r}")
    try:
        glb = path_or_glb if isinstance(path_or_glb, G.Glb) else G.Glb.load(path_or_glb)
    except Exception as error:  # noqa: BLE001 - reported as a finding
        return R.unreadable(TOOL, path_or_glb, error)
    findings, rows = [], []
    for name, points, faces in R.guarded(findings, "geometry", _groups, glb, opts["scope"], opts["include"]) or []:
        result = R.guarded(findings, f"part {name}", check_part, name, points, faces, opts)
        if result:
            findings += result[0]
            rows.append(result[1])
    triangles = sum(r["triangles"] for r in rows)
    if not rows:
        findings.append(R.finding("empty", "high", "no mesh triangles in the scene", "export the mesh objects"))
    if opts["tri_budget"] is not None and triangles > opts["tri_budget"]:
        findings.append(R.finding("tri_budget", "medium", f"{triangles} triangles over the budget",
                                  "decimate, or delete faces the camera never sees", value=triangles,
                                  limit=opts["tri_budget"]))
    metrics = {"profile": opts["profile"], "parts": len(rows), "triangles": triangles,
               "vertices": sum(r["vertices"] for r in rows), "per_part": rows}
    return R.make_report(TOOL, glb.label, findings, metrics, opts,
                         G.sha256(glb.path) if glb.path else None)


def _configure(parser):
    parser.add_argument("models", nargs="+", help="GLB files")
    parser.add_argument("--profile", choices=("auto", "volume", "surface"))
    parser.add_argument("--weld", type=float, help="weld distance in metres (default 1e-6; 0 disables)")
    parser.add_argument("--area-epsilon", type=float, help="faces at or below this area are degenerate")
    parser.add_argument("--tri-budget", type=int, help="medium finding above this many triangles")
    parser.add_argument("--inward", action="store_true", default=None, help="ray-test faces pointing into their shell")
    parser.add_argument("--inward-max-faces", type=int)
    parser.add_argument("--scope", choices=("node", "file"), help="measure each mesh node, or the file as one mesh")
    parser.add_argument("--include", nargs="+", help="only these node names")


def _run(args, config):
    opts = R.resolve_options(TOOL, DEFAULTS, config, R.cli_overrides(args, DEFAULTS))
    return [check(path, **opts) for path in args.models]


main = R.cli(__doc__, _configure, _run)

if __name__ == "__main__":
    sys.exit(main())
