"""The shader's render transfer in numpy: render vertices carried by the sim vertex frames the actor submits.

A row of the `_GARMENT` stream (garment_binding.pack_vertex_binding_u16x4) names up to three 1-based sim vertices and
their weights; their sum is the share of the cloth against plain skinning. Each record is the actor's 3x4
(row 0 = current position and q.x, row 1 = rest position and q.y, row 2 = q.z, q.w): the render vertex keeps its rest
offset from the blended rest point, turned by the blended unit quaternion, as `render/garment.glsl` does."""
import numpy as np


def _rotate(q, d):
    x, y, z, w = q.T
    t = 2.0 * np.column_stack([y * d[:, 2] - z * d[:, 1], z * d[:, 0] - x * d[:, 2], x * d[:, 1] - y * d[:, 0]])
    return d + w[:, None] * t + np.cross(q[:, :3], t)


def render_positions(skinned, rest, binding, records):
    """Drawn positions of one primitive: skinned (n, 3) model space, rest (n, 3) bind positions, binding (n, 4) u16 or
    None, records (sim vertices, 3, 4)."""
    out = np.array(skinned, float)
    if binding is None:
        return out
    g = np.asarray(binding).astype(np.uint32)
    lo, hi = g[:, 0] | (g[:, 1] << 16), g[:, 2] | (g[:, 3] << 16)
    if np.any((g.any(axis=1)) & ((hi >> 31) == 0)):
        raise ValueError("_GARMENT rows other than the 3-vertex form are not drawn by this transfer")
    ids = np.column_stack([lo & 0x7FF, (lo >> 11) & 0x7FF, hi & 0x7FF]).astype(np.intp)
    weights = np.column_stack([(lo >> 22) & 0x3FF, (hi >> 11) & 0x3FF, (hi >> 21) & 0x3FF]) / 1023.0
    used = weights.sum(1)
    rows = np.flatnonzero(used > 0)
    if not len(rows):
        return out
    if ids[rows].max() > len(records):
        raise ValueError("_GARMENT names a sim vertex outside the records")
    w = np.where(ids[rows] > 0, weights[rows], 0.0) / used[rows, None]
    r = records[np.maximum(ids[rows] - 1, 0)]
    q = np.stack([r[:, :, 0, 3], r[:, :, 1, 3], r[:, :, 2, 0], r[:, :, 2, 1]], axis=2)
    # The quaternions join the hemisphere of the first weighted one before they blend.
    first = q[np.arange(len(rows)), np.argmax(w > 0, axis=1)]
    q = q * np.where(np.einsum("nkj,nj->nk", q, first) < 0, -1.0, 1.0)[:, :, None]
    blend = np.einsum("nk,nkj->nj", w, q)
    length = np.linalg.norm(blend, axis=1, keepdims=True)
    blend = np.where(length > 1e-6, blend / np.maximum(length, 1e-6), [0.0, 0.0, 0.0, 1.0])
    current = np.einsum("nk,nkj->nj", w, r[:, :, 0, :3])
    bind = np.einsum("nk,nkj->nj", w, r[:, :, 1, :3])
    cloth = current + _rotate(blend, np.asarray(rest, float)[rows] - bind)
    share = used[rows, None]
    out[rows] = share * cloth + np.maximum(0.0, 1.0 - share) * out[rows]
    return out
