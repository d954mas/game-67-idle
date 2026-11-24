"""Small synthetic bodies and garments for the authoring and cook tests."""
import json
import struct

import numpy as np


def grid(x0, x1, y0, y1, z, nx, ny):
    """A flat panel facing +Z: points and counter-clockwise triangles."""
    xs, ys = np.linspace(x0, x1, nx), np.linspace(y0, y1, ny)
    points = np.array([[x, y, z] for y in ys for x in xs], float)
    faces = []
    for j in range(ny - 1):
        for i in range(nx - 1):
            a, b, c, d = j * nx + i, j * nx + i + 1, (j + 1) * nx + i, (j + 1) * nx + i + 1
            faces += [(a, b, d), (a, d, c)]
    return points, np.array(faces)


def tube(cx, radius, y0, y1, around=12, rows=8, z=0.0):
    """An open cylinder along +Y with outward-facing triangles."""
    points = np.array([[cx + radius * np.cos(t), y, z + radius * np.sin(t)]
                       for y in np.linspace(y0, y1, rows) for t in np.linspace(0, 2 * np.pi, around, endpoint=False)])
    faces = []
    for j in range(rows - 1):
        for i in range(around):
            a, b = j * around + i, j * around + (i + 1) % around
            c, d = a + around, b + around
            faces += [(a, d, b), (a, c, d)]
    return points, np.array(faces)


def legs():
    """Two legs, joint 0 on the left (+X) and joint 1 on the right."""
    left, faces = tube(0.1, 0.06, 0.0, 0.8)
    right, _ = tube(-0.1, 0.06, 0.0, 0.8)
    points = np.concatenate([left, right])
    weights = np.zeros((len(points), 2))
    weights[:len(left), 0] = 1
    weights[len(left):, 1] = 1
    return points, np.concatenate([faces, faces + len(left)]), weights


def glb(primitives, joints, parents=None, animation=None):
    """GLB bytes: one skinned mesh; each primitive is (material, points, faces, joint ids (n, 4), weights (n, 4),
    extra float attributes {name: values}); animation: {joint: (key times (k,), translations (k, 3))}, one clip."""
    parents = parents or [None] * len(joints)
    data = bytearray()
    views, accessors = [], []

    def add(array, component, kind):
        array = np.ascontiguousarray(array)
        data.extend(b"\0" * (-len(data) % 4))
        views.append({"buffer": 0, "byteOffset": len(data), "byteLength": array.nbytes})
        data.extend(array.tobytes())
        accessors.append({"bufferView": len(views) - 1, "componentType": component, "count": len(array), "type": kind})
        return len(accessors) - 1

    ibm = add(np.tile(np.eye(4, dtype="<f4").ravel(), (len(joints), 1)), 5126, "MAT4")
    mesh = []
    materials = []
    for material, points, faces, jids, weights, extra in primitives:
        attributes = {"POSITION": add(np.asarray(points, "<f4"), 5126, "VEC3"),
                      "JOINTS_0": add(np.asarray(jids, "u1"), 5121, "VEC4"),
                      "WEIGHTS_0": add(np.asarray(weights, "<f4"), 5126, "VEC4")}
        for name, values in extra.items():
            attributes[name] = add(np.asarray(values, "<f4"), 5126, "SCALAR")
        materials.append({"name": material})
        mesh.append({"attributes": attributes, "indices": add(np.asarray(faces, "<u4").ravel(), 5125, "SCALAR"),
                     "material": len(materials) - 1})
    nodes = [{"name": name} for name in joints]
    for i, parent in enumerate(parents):
        if parent is not None:
            nodes[parent].setdefault("children", []).append(i)
    roots = [i for i, parent in enumerate(parents) if parent is None]
    nodes.append({"name": "mesh", "mesh": 0, "skin": 0})
    doc = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": roots + [len(nodes) - 1]}], "nodes": nodes,
           "meshes": [{"primitives": mesh}], "materials": materials, "accessors": accessors, "bufferViews": views,
           "skins": [{"joints": list(range(len(joints))), "inverseBindMatrices": ibm}],
           "buffers": [{"byteLength": len(data)}]}
    if animation:
        samplers, channels = [], []
        for name, (times, moves) in animation.items():
            samplers.append({"input": add(np.asarray(times, "<f4"), 5126, "SCALAR"),
                             "output": add(np.asarray(moves, "<f4"), 5126, "VEC3"), "interpolation": "LINEAR"})
            accessors[samplers[-1]["input"]].update(min=[float(min(times))], max=[float(max(times))])
            channels.append({"sampler": len(samplers) - 1, "target": {"node": joints.index(name), "path": "translation"}})
        doc["animations"] = [{"name": "clip", "samplers": samplers, "channels": channels}]
        doc["buffers"] = [{"byteLength": len(data)}]
    data.extend(b"\0" * (-len(data) % 4))
    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    chunks = struct.pack("<I4s", len(text), b"JSON") + text + struct.pack("<I4s", len(data), b"BIN\0") + bytes(data)
    return struct.pack("<4sII", b"glTF", 2, 12 + len(chunks)) + chunks


def one_joint(n, joint=0):
    jids = np.zeros((n, 4), int)
    jids[:, 0] = joint
    weights = np.zeros((n, 4))
    weights[:, 0] = 1
    return jids, weights
