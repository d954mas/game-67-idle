"""The feature's own runtime offline, through the garment_host library (tools/host/garment_host.c).

Load a cooked blob on a skeleton, feed model-space joint poses, world convexes and corrective weights, store a
corrective shape, step, and read positions, references, render records and metrics. Poses come from glTF files
through the shared GLB library (ai_studio/assets/tools/blender/lib/glb.py). `bake` drapes the cloth into a key pose by
the procedure of the spec (rest, a ramp by local slerp, the convexes raised from below, a settle with the pose frozen)
and `deltas` turns a drape into rest-space corrective deltas; garment.corrective repeats them until the drape is an
equilibrium of the runtime.

Callers put features/garment-cloth/tools on sys.path, as the cook's callers do.
"""
import ctypes
import os
from pathlib import Path

import numpy as np
from scipy.optimize import linprog

FEATURE = Path(__file__).resolve().parents[2]
STUDIO = FEATURE.parents[1]
# The feature-test consumer builds the library (node ai_studio/feature_testing/run.mjs); GARMENT_HOST names another.
LIBRARY = STUDIO / "ai_studio/feature_testing/consumer/build/feature-verify/_garment_cloth/garment_host.dll"
BUILD = "node ai_studio/feature_testing/run.mjs"
# The runtime the default library is built from: a library older than any of them steps another runtime than the game's.
RUNTIME = [*sorted((FEATURE / "src").glob("*.c")), *sorted((FEATURE / "include/garment").glob("*.h")),
           FEATURE / "tools/host/garment_host.c"]


def runtime_sha256():
    """One hash of the runtime sources: a corrective is an equilibrium of this solver, so a change of any of them makes
    every baked corrective stale."""
    import hashlib
    from .build import sha256
    h = hashlib.sha256()
    for path in RUNTIME:
        h.update(path.relative_to(STUDIO).as_posix().encode())
        h.update(sha256(path).encode())
    return h.hexdigest()


SIM_HZ = 180
POSE_HZ = 30
CONVEX_PLANES = 48
METRICS = ("finite", "steps", "max_strain", "max_displacement_m", "max_distance_excess_m", "backstop_depth_m",
           "held_depth_m", "held_vertices")


class Convex(ctypes.Structure):
    _fields_ = [("plane_count", ctypes.c_int), ("planes", ctypes.c_float * (CONVEX_PLANES * 4))]


def convex(planes):
    """A ClothConvex from (n, d) rows, inside where n . x + d <= 0."""
    planes = np.asarray(planes, np.float32).reshape(-1, 4)
    if not 1 <= len(planes) <= CONVEX_PLANES:
        raise ValueError(f"a convex has 1..{CONVEX_PLANES} planes, not {len(planes)}")
    out = Convex(len(planes))
    out.planes[:planes.size] = planes.ravel().tolist()
    return out


def raised(planes, height):
    """The convex moved up by height (m)."""
    planes = np.array(planes, float).reshape(-1, 4)
    planes[:, 3] -= planes[:, 1] * height
    return planes


def top(planes):
    """The highest point's height of a bounded convex."""
    planes = np.asarray(planes, float).reshape(-1, 4)
    found = linprog([0, -1, 0], A_ub=planes[:, :3], b_ub=-planes[:, 3], bounds=[(None, None)] * 3)
    if not found.success:
        raise ValueError("the convex is empty or unbounded upward")
    return float(found.x[1])


class Skeleton:
    """Joint names in preorder with parent indices, as the engine's skeleton orders them; `nodes` are the GLB nodes."""

    def __init__(self, names, parents, nodes=None):
        self.names, self.parents = list(names), np.asarray(parents, np.int64)
        self.nodes = list(nodes) if nodes is not None else None
        if any(p >= j for j, p in enumerate(self.parents)):
            raise ValueError("parents must precede their joints")

    @classmethod
    def from_glb(cls, glb, skin=0):
        joints = set(glb.json["skins"][skin]["joints"])
        names, parents, nodes = [], [], []
        stack = [(r, -1) for r in reversed(glb.roots())]
        while stack:
            node, parent = stack.pop()
            if node in joints:
                nodes.append(node)
                names.append(glb.name(node))
                parents.append(parent)
                parent = len(nodes) - 1
            stack.extend((c, parent) for c in reversed(glb.nodes[node].get("children", [])))
        return cls(names, parents, nodes)

    def pose(self, world):
        """(joints, 4, 4) model-space matrices from a {node: 4x4} world of the GLB."""
        return np.stack([world[n] for n in self.nodes])


def rest_pose(glb, skeleton):
    return skeleton.pose(glb.world_rest())


def clip_pose(glb, clip, t, skeleton, anim=0):
    """The model posed by a clip file's channels (matched by node name) at time t."""
    return skeleton.pose(glb.posed_world(clip.channels(anim), t))


def _quat(m):
    trace = np.trace(m)
    if trace > 0:
        s = 2.0 * np.sqrt(trace + 1.0)
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, 0.25 * s]
    else:
        i = int(np.argmax(np.diag(m)))
        j, k = (i + 1) % 3, (i + 2) % 3
        s = 2.0 * np.sqrt(1.0 + m[i, i] - m[j, j] - m[k, k])
        q = [0.0, 0.0, 0.0, (m[k, j] - m[j, k]) / s]
        q[i], q[j], q[k] = 0.25 * s, (m[j, i] + m[i, j]) / s, (m[k, i] + m[i, k]) / s
    q = np.array(q)
    return q / np.linalg.norm(q)


def _matrix(t, q, s):
    x, y, z, w = q
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4)
    m[:3, :3] = r * s
    m[:3, 3] = t
    return m


def locals_of(pose, skeleton):
    """Parent-local (t, q, s) of every joint of a model-space pose."""
    out = []
    for j, p in enumerate(skeleton.parents):
        local = pose[j] if p < 0 else np.linalg.inv(pose[p]) @ pose[j]
        s = np.linalg.norm(local[:3, :3], axis=0)
        r = local[:3, :3] / s
        if np.linalg.det(r) < 0:
            s[0], r[:, 0] = -s[0], -r[:, 0]
        out.append((local[:3, 3].copy(), _quat(r), s))
    return out


def ramp(a, b, u, skeleton):
    """The pose a fraction u from a to b: local rotations slerped, local translations and scales lerped, then forward
    kinematics, the interpolation the runtime applies between its own samples."""
    out = np.empty_like(a)
    for j, ((ta, qa, sa), (tb, qb, sb)) in enumerate(zip(locals_of(a, skeleton), locals_of(b, skeleton))):
        if np.dot(qa, qb) < 0:
            qb = -qb
        angle = np.arccos(min(1.0, float(np.dot(qa, qb))))
        q = qa + u * (qb - qa) if angle < 1e-6 else (np.sin((1 - u) * angle) * qa + np.sin(u * angle) * qb) / np.sin(angle)
        local = _matrix(ta + u * (tb - ta), q / np.linalg.norm(q), sa + u * (sb - sa))
        p = skeleton.parents[j]
        out[j] = local if p < 0 else out[p] @ local
    return out


class Host:
    """One cooked outfit on one skeleton in the garment_host library."""

    def __init__(self, blob, skeleton, library=None):
        self.lib = _library(library)
        self.skeleton = skeleton
        names = (ctypes.c_char_p * len(skeleton.names))(*[n.encode("utf-8") for n in skeleton.names])
        parents = np.where(skeleton.parents < 0, 0xFFFF, skeleton.parents).astype(np.uint16)
        self.blob = bytes(blob)
        self.handle = self.lib.gh_load(self.blob, len(self.blob), len(skeleton.names), names,
                                       parents.ctypes.data_as(ctypes.POINTER(ctypes.c_uint16)))
        if not self.handle:
            raise ValueError("garment_host refused the blob or the skeleton (missing joints, or parents out of order)")
        self.vertices = [self.lib.gh_piece_vertices(self.handle, p) for p in range(self.lib.gh_piece_count(self.handle))]

    def close(self):
        if self.handle:
            self.lib.gh_free(self.handle)
            self.handle = None

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    @property
    def corrective_count(self):
        return self.lib.gh_corrective_count(self.handle)

    def step(self, pose, dt, reset=False, convexes=(), render=True, measure=True, world=None, scale=1.0, bias=0.0,
             weights=None):
        """One runtime update: pose (joints, 4, 4) or (joints, 3, 4) model space, dt in s, convexes as plane rows,
        weights one per corrective of the blob (None: all 0)."""
        rows = np.ascontiguousarray(np.asarray(pose, float)[:, :3, :4], np.float32)
        if rows.shape[0] != len(self.skeleton.names):
            raise ValueError("the pose has another joint count than the skeleton")
        shapes = (Convex * max(1, len(convexes)))(*[convex(c) for c in convexes])
        frame = None if world is None else np.ascontiguousarray(world, np.float32).T.ravel()
        if weights is not None:
            weights = np.ascontiguousarray(weights, np.float32)
            if weights.shape != (self.corrective_count,):
                raise ValueError(f"the blob has {self.corrective_count} correctives, not {weights.shape} weights")
        pointer = lambda a: None if a is None else a.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        ok = self.lib.gh_step(self.handle, pointer(rows), pointer(frame), float(dt), bool(reset), bool(render),
                              bool(measure), shapes, len(convexes), float(scale), float(bias), pointer(weights))
        if not ok:
            raise RuntimeError("garment_host: the update failed (non-finite state or invalid input)")

    def positions(self, piece):
        out = np.zeros((self.vertices[piece], 3), np.float32)
        if not self.lib.gh_positions(self.handle, piece, out.ctypes.data_as(ctypes.POINTER(ctypes.c_float))):
            raise RuntimeError("no state before the first update")
        return out.astype(float)

    def reference(self, piece):
        """(x, b) the solver used at the frame pose."""
        out = np.zeros((self.vertices[piece], 6), np.float32)
        if not self.lib.gh_reference(self.handle, piece, out.ctypes.data_as(ctypes.POINTER(ctypes.c_float))):
            raise RuntimeError("no reference before the first update")
        return out[:, :3].astype(float), out[:, 3:].astype(float)

    def backstop(self, piece):
        """(d, R) per vertex the solver used at the frame pose, (vertices, 2): blended by the corrective weights."""
        out = np.zeros((self.vertices[piece], 2), np.float32)
        if not self.lib.gh_backstop(self.handle, piece, out.ctypes.data_as(ctypes.POINTER(ctypes.c_float))):
            raise RuntimeError("no backstop before the first update")
        return out.astype(float)

    def records(self):
        """The shader's transfer records of the last rendered update: (sim vertices, 3, 4)."""
        out = np.zeros((sum(self.vertices), 12), np.float32)
        if self.lib.gh_records(self.handle, out.ctypes.data_as(ctypes.POINTER(ctypes.c_float))) != len(out):
            raise RuntimeError("no records before a rendered update")
        return out.reshape(-1, 3, 4).astype(float)

    def metrics(self):
        out = (ctypes.c_float * len(METRICS))()
        self.lib.gh_metrics(self.handle, out)
        return dict(zip(METRICS, list(out)))

    def set_corrective(self, piece, index, delta, direction, backstop):
        """Store corrective `index` of a piece: rest-space position and body direction deltas (vertices, 3) each and
        the backstop (d, R) at its key (vertices, 2). The runtime evaluates it with its weight from the next update on."""
        shape = np.ascontiguousarray(np.concatenate([np.reshape(delta, (-1, 3)), np.reshape(direction, (-1, 3)),
                                                     np.reshape(backstop, (-1, 2))], 1), np.float32)
        if shape.shape != (self.vertices[piece], 8):
            raise ValueError(f"piece {piece} has {self.vertices[piece]} vertices, the shape {len(shape)}")
        if not self.lib.gh_corrective(self.handle, piece, index, shape.ctypes.data_as(ctypes.POINTER(ctypes.c_float))):
            raise ValueError(f"no piece {piece} or corrective {index}")


def _library(path=None):
    chosen = path or os.environ.get("GARMENT_HOST")
    path = Path(chosen or LIBRARY)
    if not path.is_file():
        raise FileNotFoundError(f"{path}: the garment_host library (the feature's runtime for offline tools) is not built; "
                                f"build it with `{BUILD}`, or set GARMENT_HOST")
    if not chosen:
        newer = [str(f.relative_to(STUDIO)) for f in RUNTIME if f.stat().st_mtime > path.stat().st_mtime]
        if newer:
            raise FileNotFoundError(f"{path} is older than the runtime ({', '.join(newer[:3])}): rebuild it with `{BUILD}`")
    lib = ctypes.CDLL(str(path))
    f, i, b, p = ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_bool, ctypes.c_void_p
    signatures = {
        "gh_load": (p, [ctypes.c_char_p, ctypes.c_uint32, i, ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_uint16)]),
        "gh_free": (None, [p]), "gh_piece_count": (i, [p]), "gh_piece_vertices": (i, [p, i]),
        "gh_step": (b, [p, f, f, ctypes.c_float, b, b, b, ctypes.POINTER(Convex), i, ctypes.c_float, ctypes.c_float, f]),
        "gh_positions": (b, [p, i, f]), "gh_reference": (b, [p, i, f]), "gh_backstop": (b, [p, i, f]),
        "gh_records": (i, [p, f]),
        "gh_metrics": (b, [p, f]), "gh_corrective_count": (i, [p]), "gh_corrective": (b, [p, i, i, f])}
    for name, (restype, argtypes) in signatures.items():
        getattr(lib, name).restype, getattr(lib, name).argtypes = restype, argtypes
    return lib


def run(host, poses, convexes, dt=1 / POSE_HZ, reset=False, bias=0.0, weights=None):
    """Step through a sequence of poses, each with its convex list; the first update resets when asked."""
    for k, (pose, shapes) in enumerate(zip(poses, convexes)):
        host.step(pose, 0.0 if reset and k == 0 else dt, reset=reset and k == 0, convexes=shapes, bias=bias,
                  weights=weights)


def bake(host, rest, key, rising=(), fixed=(), settle_in_s=0.5, ramp_s=1.0, rise_s=0.5, settle_s=2.0, hz=POSE_HZ,
         thickness=0.0, max_distance_bias=0.0):
    """The settled cloth of each piece in the key pose (spec C.2): reset at rest and settle, ramp the joints from rest
    to the key by local slerp, raise the rising convexes from below the lowest cloth vertex to their place, settle
    with the pose frozen. fixed convexes (a floor) are present throughout. max_distance_bias is the runtime lever
    (r' = r + bias on free vertices) for a drape the reference does not lead."""
    bias = max_distance_bias
    fixed = [np.asarray(c, float) for c in fixed]
    frames = lambda seconds: max(1, int(round(seconds * hz)))
    run(host, [rest] * (frames(settle_in_s) + 1), [fixed] * (frames(settle_in_s) + 1), 1 / hz, reset=True, bias=bias)
    n = frames(ramp_s)
    run(host, [ramp(rest, key, (k + 1) / n, host.skeleton) for k in range(n)], [fixed] * n, 1 / hz, bias=bias)
    if rising:
        lowest = min(host.positions(p)[:, 1].min() for p in range(len(host.vertices)))
        drop = [max(0.0, top(c) - lowest + thickness) for c in rising]
        n = frames(rise_s)
        for k in range(n):
            u = (k + 1) / n
            host.step(key, 1 / hz, convexes=fixed + [raised(c, -(1 - u) * d) for c, d in zip(rising, drop)], bias=bias)
    n = frames(settle_s)
    run(host, [key] * n, [fixed + [np.asarray(c, float) for c in rising]] * n, 1 / hz, bias=bias)
    return [host.positions(p) for p in range(len(host.vertices))]


def skin_matrices(decoded, pose, skeleton):
    """Per blob bone: joint model matrix x inverse bind, (bones, 4, 4)."""
    index = {name: j for j, name in enumerate(skeleton.names)}
    out = []
    for name, inverse in decoded["bones"]:
        m = np.eye(4)
        m[:3] = np.asarray(inverse, float).reshape(3, 4)
        out.append(np.asarray(pose[index[name]], float) @ m)
    return np.stack(out)


def blended(piece, matrices):
    """Each vertex's linear-blend skinning matrix sum_j w_j S_j, (vertices, 4, 4)."""
    skin = piece["skin"]
    return np.einsum("vk,vkij->vij", skin["weight"].astype(float), matrices[skin["bone"].astype(int)])


def deltas(decoded, pose, skeleton, settled, directions):
    """Per piece the rest-space corrective deltas of a drape (spec B.5), (position (V, 3), body direction (V, 3)):
    rest + delta = (sum_j w_j S_j)^-1 x_settled, and the direction at rest that the vertex's blended linear part turns
    into the settled cloth's own direction toward the body (directions: per piece, unit, posed), normalized as the
    runtime normalizes after skinning. Kinematic vertices keep delta = 0."""
    matrices = skin_matrices(decoded, pose, skeleton)
    out = []
    for p, piece in enumerate(decoded["pieces"]):
        m = blended(piece, matrices)
        rest = np.asarray(piece["mesh"]["rest"], float)
        direction = np.asarray(piece["direction"], float)
        free = (np.asarray(piece["mesh"]["max_distance"]) > 0)[:, None]
        linear = np.linalg.inv(m[:, :3, :3])
        local = np.einsum("vij,vj->vi", linear, np.asarray(settled[p], float) - m[:, :3, 3])
        turned = np.einsum("vij,vj->vi", linear, np.asarray(directions[p], float))
        turned /= np.linalg.norm(turned, axis=1, keepdims=True)
        out.append((np.where(free, local - rest, 0.0), np.where(free, turned - direction, 0.0)))
    return out
