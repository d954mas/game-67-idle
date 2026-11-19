"""Contact check of a GLB scene: named parts must rest on their receivers within a signed-gap window,
or (contract-free) no part may hang in the air.

    python contact_check.py scene.glb --contract contacts.json
    python contact_check.py scene.glb --floating [--tolerance 0.01] [--floor-y 0] [--split-components]

Contract (glTF space, Y up, metres):
  {"pairs": [{"moving": "Leg", "receiver": "Deck", "direction": [0, -1, 0], "window": [-0.08, 0.001]}],
   "ground": [{"node": "Pot", "window": [-0.01, 0.001]}], "floor_y": 0}
The signed gap is the distance the moving part travels along `direction` before it meets the receiver:
positive is air, negative is overlap. Every contact also gets negative controls: the part lifted by
--margin, moved out past the window, and pushed into the receiver past it must each fail the same
measurement; a contract that still passes is not testing anything.
"""
import json
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

import numpy as np  # noqa: E402

from ai_studio.assets.tools.blender import mesh_metrics as M  # noqa: E402
from ai_studio.assets.tools.blender.lib import glb as G, rays, report as R  # noqa: E402

TOOL = "contact_check"
DEFAULTS = {"contract": None, "floating": False, "floor_y": None, "tolerance": 0.01, "margin": 0.05,
            "max_samples": 2000, "split_components": False}
DOWN = np.array([0.0, -1.0, 0.0])
SIDEWAYS = (np.array([1.0, 0, 0]), np.array([-1.0, 0, 0]), np.array([0, 0, 1.0]), np.array([0, 0, -1.0]),
            np.array([0, 1.0, 0]))


def _r(values, digits=4):
    return [round(float(v), digits) for v in np.ravel(values)]


class Piece:
    def __init__(self, name, points, faces):
        self.name, self.points, self.faces = name, points, faces
        self.lo, self.hi = points.min(axis=0), points.max(axis=0)
        self.tris = rays.triangles(points, faces)


def pieces(glb, split=False):
    by_node = {}
    for part in glb.parts():
        by_node.setdefault(part.name, []).append(part)
    out = {}
    for name, members in by_node.items():
        base, faces = 0, []
        for part in members:
            faces.append(part.faces + base)
            base += len(part.positions)
        points, faces = np.vstack([p.positions for p in members]), np.vstack(faces)
        if not len(faces):
            continue
        if not split:
            out[name] = Piece(name, points, faces)
            continue
        welded, remap = M.weld(points, 1e-6)
        faces = remap[faces]
        label = M.face_components(faces, len(welded))
        for k in range(label.max() + 1):
            sub = faces[label == k]
            used, local = np.unique(sub, return_inverse=True)
            key = f"{name}#{k}" if label.max() else name
            out[key] = Piece(key, welded[used], local.reshape(-1, 3))
    return out


def _samples(points, direction, limit):
    """The points that lead along `direction`: the first to meet whatever lies ahead."""
    along = points @ direction
    order = np.argsort(-along)[:limit]
    return points[order]


def signed_gap(points, direction, receiver_tris, back):
    """Smallest travel along `direction` from any point to the receiver; negative when the receiver
    surface already lies behind the point (overlap, up to `back` deep). None when no ray meets it."""
    t = rays.first_hit(points - direction * back, direction, receiver_tris) - back
    t = t[np.isfinite(t)]
    return float(t.min()) if len(t) else None


def _receiver_near(receiver, points, direction):
    """Receiver triangles whose footprint, seen along `direction`, overlaps the moving points'."""
    axes = np.linalg.svd(direction[None])[2][1:]
    lo, hi = (points @ axes.T).min(axis=0), (points @ axes.T).max(axis=0)
    a, b, c = receiver.tris
    proj = np.stack([a @ axes.T, b @ axes.T, c @ axes.T])
    keep = ((proj.max(axis=0) >= lo - 1e-6) & (proj.min(axis=0) <= hi + 1e-6)).all(axis=1)
    return a[keep], b[keep], c[keep]


def measure_pair(moving, receiver, direction, window, opts):
    direction = np.asarray(direction, float)
    direction = direction / np.linalg.norm(direction)
    samples = _samples(moving.points, direction, opts["max_samples"])
    depth = abs(window[0]) + opts["margin"]
    back = max(0.2, 2.0 * depth)
    tris = _receiver_near(receiver, samples, direction)
    gap = signed_gap(samples, direction, tris, back)

    def passes(shift):
        g = signed_gap(samples + direction * shift, direction, _receiver_near(receiver, samples + direction * shift,
                                                                               direction), back)
        return g is not None and window[0] <= g <= window[1]

    controls = None
    if gap is not None:
        out_shift = gap - (window[1] + opts["margin"])
        in_shift = gap - (window[0] - opts["margin"])
        controls = {"lifted_fails": not passes(-opts["margin"]), "moved_out_fails": not passes(out_shift),
                    "moved_in_fails": not passes(in_shift)}
    return gap, controls


def plane_gap(piece, floor_y):
    return float(piece.lo[1] - floor_y)


def check_contract(contract, parts, opts, out, rows):
    floor_y = opts["floor_y"] if opts["floor_y"] is not None else contract.get("floor_y", 0.0)
    for entry in contract.get("pairs", []):
        label = f"{entry['moving']} -> {entry['receiver']}"
        missing = [n for n in (entry["moving"], entry["receiver"]) if n not in parts]
        if missing:
            out.append(R.finding("contract", "high", f"{label}: no mesh node {missing}",
                                 "fix the node names in the contract or the export", {"pair": label}))
            continue
        window = entry.get("window", [-0.08, 0.001])
        gap, controls = measure_pair(parts[entry["moving"]], parts[entry["receiver"]],
                                     entry.get("direction", [0, -1, 0]), window, opts)
        _judge(label, gap, window, controls, out, rows, f"move {entry['moving']} onto {entry['receiver']}")
    for entry in contract.get("ground", []):
        label = f"{entry['node']} -> floor"
        if entry["node"] not in parts:
            out.append(R.finding("contract", "high", f"{label}: no mesh node {entry['node']!r}",
                                 "fix the node name in the contract or the export", {"pair": label}))
            continue
        window = entry.get("window", [-0.01, 0.001])
        gap = plane_gap(parts[entry["node"]], floor_y)
        # A plane is measured exactly, so a shifted part always lands outside the window.
        controls = {"lifted_fails": not window[0] <= gap + opts["margin"] <= window[1],
                    "moved_out_fails": True, "moved_in_fails": True}
        _judge(label, gap, window, controls, out, rows,
               f"move {entry['node']} onto the floor at y {floor_y}")


def _judge(label, gap, window, controls, out, rows, hint):
    rows.append({"contact": label, "signed_gap": None if gap is None else round(gap, 5), "window": window,
                 "controls": controls})
    where = {"pair": label}
    if gap is None:
        out.append(R.finding("contact", "high", f"{label}: the receiver is not ahead of the part at all",
                             "check the direction and node names; " + hint, where))
        return
    if not window[0] <= gap <= window[1]:
        state = "hangs" if gap > window[1] else "sinks"
        out.append(R.finding("contact", "high", f"{label}: {state} {abs(gap):.4f} m (gap {gap:+.4f}, window "
                             f"{window})", hint, where, round(gap, 5), window))
    if controls and not controls["lifted_fails"]:
        out.append(R.finding("negative_control", "medium", f"{label}: the window still passes with the part "
                             "lifted off its receiver; the contract is not testing contact",
                             "narrow the window's upper bound to a few millimetres", {**where, **controls}))
    elif controls and not all(controls.values()):
        out.append(R.finding("negative_control", "medium", f"{label}: still passes with the part moved past "
                             "the window; the measurement meets some other surface",
                             "pick the receiver the part actually rests on, or fix the direction",
                             {**where, **controls}))


def _spread(points, count=500):
    return points[::max(1, len(points) // count)]


def _attached(piece, other, tol, max_samples):
    """Touching beside or above within tolerance, or one embedded in the other."""
    for d in SIDEWAYS:
        lead = _samples(piece.points, d, max_samples)
        tris = _receiver_near(other, lead, d)
        reach = float(np.ptp(piece.points @ d)) + tol
        g = signed_gap(lead, d, tris, reach) if len(tris[0]) else None
        if g is not None and g <= tol:
            return "side"
    up = np.array([0.0, 1.0, 0.0])
    if (rays.crossings(_spread(piece.points), up, other.tris) % 2).any() or             (rays.crossings(_spread(other.points), up, piece.tris) % 2).any():
        return "embedded"
    return None


def _gap_below(piece, other, sink, max_samples):
    """Air between piece and the other part under it, measured both ways (piece points down onto the
    other, the other's points up into the piece), so a support under a face's middle is not missed."""
    gaps = []
    for source, target, d in ((piece, other, DOWN), (other, piece, -DOWN)):
        points = _samples(source.points, d, max_samples)
        lo, hi = target.lo[[0, 2]], target.hi[[0, 2]]
        points = points[((points[:, [0, 2]] >= lo - 1e-6) & (points[:, [0, 2]] <= hi + 1e-6)).all(axis=1)]
        tris = _receiver_near(target, points, d) if len(points) else ((),)
        g = signed_gap(points, d, tris, sink) if len(tris[0]) else None
        if g is not None:
            gaps.append(g)
    return min(gaps) if gaps else None


def check_floating(parts, opts, out, rows):
    """A part is held when the floor or a held part is under it within tolerance, or when it is
    attached (beside, above, embedded) to a held part. Parts that only hold each other float together."""
    names = list(parts)
    floor_y = 0.0 if opts["floor_y"] is None else opts["floor_y"]
    tol = opts["tolerance"]
    # Down rays start only this far above a sample: a support it is sunk into still counts, but a part
    # above the sample (its own load) is never taken for support.
    sink = max(tol, 0.05)
    below_of, attached_to, best = {}, {n: set() for n in names}, {}
    for name in names:
        piece = parts[name]
        others = [parts[n] for n in names if n != name]
        under = [o for o in others if o.lo[1] < piece.lo[1] + sink and (o.lo[[0, 2]] <= piece.hi[[0, 2]]).all()
                 and (o.hi[[0, 2]] >= piece.lo[[0, 2]]).all()]
        gaps = {"floor": float(piece.lo[1] - floor_y)}
        for other in under:
            g = _gap_below(piece, other, sink, opts["max_samples"])
            if g is not None:
                gaps[other.name] = g
        best[name] = min(gaps.items(), key=lambda item: item[1])
        below_of[name] = {n for n, g in gaps.items() if g <= tol}
        for other in others:
            if other.name in attached_to[name]:
                continue
            if (other.lo - tol <= piece.hi).all() and (other.hi + tol >= piece.lo).all() and                     _attached(piece, other, tol, opts["max_samples"]):
                attached_to[name].add(other.name)
                attached_to[other.name].add(name)
    held = {n for n in names if "floor" in below_of[n]}
    grew = True
    while grew:
        grew = False
        for n in names:
            if n not in held and (below_of[n] & held or attached_to[n] & held):
                held.add(n)
                grew = True
    for name in names:
        piece = parts[name]
        support, gap = best[name]
        rows.append({"part": name, "gap_below": round(gap, 5), "support": support, "held": name in held,
                     "attached": sorted(attached_to[name])})
        if name in held:
            continue
        if len(parts) == 1:
            # A lone part has only the origin floor to rest on; overlay layers and wall or ceiling
            # mounts legitimately sit above it, so this is a pivot note rather than a defect.
            out.append(R.finding("floating", "low", f"{name}: bottom is {gap:.3f} m above the floor at the origin",
                                 "fine for an overlay or a wall/ceiling mount; a floor prop needs its bottom at "
                                 "the origin", {"part": name, "bottom": _r(piece.lo)}, round(gap, 4), tol))
            continue
        chain = "" if support == "floor" else f" (rests on {support}, which is not held either)"
        out.append(R.finding("floating", "medium", f"{name} is not held up: bottom {piece.lo[1] - floor_y:.3f} m "
                             f"above the floor{chain}", f"lower {name} or its assembly onto a support; check the "
                             "origin if it is placed by code", {"part": name, "bottom": _r(piece.lo)},
                             round(float(piece.lo[1] - floor_y), 4), tol))


def check(path_or_glb, **options):
    opts = R.resolve_options(TOOL, DEFAULTS, overrides=options)
    try:
        glb = path_or_glb if isinstance(path_or_glb, G.Glb) else G.Glb.load(path_or_glb)
    except Exception as error:  # noqa: BLE001 - reported as a finding
        return R.unreadable(TOOL, path_or_glb, error)
    parts = R.guarded(findings := [], "geometry", pieces, glb, opts["split_components"]) or {}
    rows = []
    contract = opts["contract"]
    if isinstance(contract, (str, Path)):
        contract = R.guarded(findings, "contract file", lambda: json.loads(Path(opts["contract"]).read_text(
            encoding="utf-8")))
    if contract is None and not opts["floating"]:
        findings.append(R.finding("contract", "info", "no contract and no --floating: nothing measured",
                                  "pass --contract contacts.json or --floating"))
    if contract is not None:
        R.guarded(findings, "contract", check_contract, contract, parts, opts, findings, rows)
    floating = []
    if opts["floating"]:
        R.guarded(findings, "floating", check_floating, parts, opts, findings, floating)
    metrics = {"parts": len(parts), "contacts": rows, "floating": floating}
    shown = {k: v for k, v in opts.items() if k != "contract"}
    shown["contract"] = str(opts["contract"]) if isinstance(opts["contract"], (str, Path)) else bool(contract)
    return R.make_report(TOOL, glb.label, findings, metrics, shown, G.sha256(glb.path) if glb.path else None)


def _configure(parser):
    parser.add_argument("models", nargs="+", help="GLB files")
    parser.add_argument("--contract", help="contact contract JSON")
    parser.add_argument("--floating", action="store_true", default=None, help="find parts hanging in the air")
    parser.add_argument("--floor-y", type=float, help="floor height (default: contract floor_y, else 0)")
    parser.add_argument("--tolerance", type=float, help="floating gap tolerance in metres (default 0.01)")
    parser.add_argument("--margin", type=float, help="negative-control shift past the window (default 0.05)")
    parser.add_argument("--max-samples", type=int, help="leading points cast per part (default 2000)")
    parser.add_argument("--split-components", action="store_true", default=None,
                        help="treat each connected shell of a mesh node as its own part")


def _run(args, config):
    opts = R.resolve_options(TOOL, DEFAULTS, config, R.cli_overrides(args, DEFAULTS))
    return [check(path, **opts) for path in args.models]


main = R.cli(__doc__, _configure, _run)

if __name__ == "__main__":
    sys.exit(main())
