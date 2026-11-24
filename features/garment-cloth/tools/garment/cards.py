"""Static cards of a piece's authored data for agents without a GUI (spec C.4): the sim wire over the body, each
map as heat with a numeric legend, weights per joint, the body direction, the reference posed in a named clip and
time, the body shapes, and marked vertices (a gate's failures) in red with their ids.

The camera is garment.view's: orthographic, Y up, yaw 0 looks at the front of an actor facing +Z, positive yaw
walks the camera to her left. Faces are drawn far to near; a face seen from behind is darker."""
import numpy as np
from PIL import Image, ImageDraw

from . import model as M
from .view import camera, _font

BACKGROUND = (248, 247, 244)
BODY = (172, 172, 168)
CLOTH = (150, 180, 220)
WIRE = (40, 60, 110)
PIN = (20, 20, 24)
OFF = (205, 205, 200)
PLANE = (120, 60, 160)
MARK = (230, 30, 30)
INK = (24, 24, 28)
SHAPE = (200, 90, 20)
# Sequential ramp (low to high): readable on the grey body and in print.
RAMP = np.array([(48, 18, 120), (40, 100, 190), (30, 170, 160), (130, 200, 80), (250, 220, 40)], float)


def heat(values, low, high):
    """RGB of each value on the ramp between low and high."""
    t = np.clip((np.asarray(values, float) - low) / max(high - low, 1e-12), 0, 1) * (len(RAMP) - 1)
    k = np.minimum(t.astype(int), len(RAMP) - 2)
    u = (t - k)[:, None]
    return (1 - u) * RAMP[k] + u * RAMP[k + 1]


def _project(points, right, up, bounds, scale, height):
    x0, _, y0, _ = bounds
    return np.stack([(points @ right - x0) * scale, height - (points @ up - y0) * scale], axis=-1)


def _panel(size, yaw, pitch, bounds, scale, body, cloth, cloth_rgb, wire, dots, arrows, shapes, labels):
    right, up, toward = camera(yaw, pitch)
    light = -0.35 * right + 0.55 * up + 0.75 * toward
    light /= np.linalg.norm(light)
    image = Image.new("RGB", size, BACKGROUND)
    draw = ImageDraw.Draw(image)
    height = size[1]
    tris, rgb, outline = [], [], []
    for triangles, colours, edge in ((body, np.tile(BODY, (len(body), 1)), None), (cloth, cloth_rgb, wire)):
        if not len(triangles):
            continue
        n = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        lit = 0.45 + 0.55 * np.abs(n @ light)
        lit = np.where(n @ toward >= 0, lit, 0.6 * lit)
        tris.append(triangles)
        rgb.append(np.clip(lit[:, None] * colours, 0, 255).astype(int))
        outline += [edge] * len(triangles)
    if tris:
        tris, rgb = np.concatenate(tris), np.concatenate(rgb)
        pixels = _project(tris, right, up, bounds, scale, height)
        for i in np.argsort(tris.mean(1) @ toward):
            draw.polygon([tuple(v) for v in pixels[i]], fill=tuple(rgb[i]), outline=outline[i])
    for centre, radius in shapes.get("spheres", []):
        (x, y), = _project(np.asarray(centre)[None], right, up, bounds, scale, height)
        draw.ellipse([x - radius * scale, y - radius * scale, x + radius * scale, y + radius * scale], outline=SHAPE, width=2)
    for (ca, ra), (cb, rb) in shapes.get("cones", []):
        (xa, ya), (xb, yb) = _project(np.array([ca, cb]), right, up, bounds, scale, height)
        axis = np.array([xb - xa, yb - ya])
        side = np.array([-axis[1], axis[0]]) / max(np.linalg.norm(axis), 1e-9)
        for s in (-1, 1):
            draw.line([tuple(np.array([xa, ya]) + s * side * ra * scale), tuple(np.array([xb, yb]) + s * side * rb * scale)],
                      fill=SHAPE, width=2)
    if arrows is not None:
        start, end = (_project(a, right, up, bounds, scale, height) for a in arrows)
        for a, b in zip(start, end):
            draw.line([tuple(a), tuple(b)], fill=MARK, width=1)
    for points, colour, r in dots:
        for x, y in _project(np.asarray(points, float).reshape(-1, 3), right, up, bounds, scale, height):
            draw.ellipse([x - r, y - r, x + r, y + r], fill=colour)
    font = _font(13)
    for point, text in labels:
        (x, y), = _project(np.asarray(point)[None], right, up, bounds, scale, height)
        draw.text((x + 5, y - 7), text, font=font, fill=MARK)
    return image


def draw(path, body, points, faces, *, values=None, low=None, high=None, unit="", special=None, pins=None,
         marks=(), arrows=None, shapes=None, title="", lines=(), yaws=(0, 90, 180), pitch=12.0, panel_px=(420, 620)):
    """Write one card: a panel per yaw of the body (triangles (F, 3, 3)) with the sim mesh (points, faces) on it.

    values: per sim vertex, drawn as heat between low and high (their range by default), with a legend in `unit`;
    special: {name: (mask, rgb)} vertices drawn in a fixed colour instead (a pin, a backstop that is off, the plane
    form); pins: a mask drawn as black dots; marks: vertex ids drawn red with their ids; arrows: (start, end) points;
    shapes: {"spheres": [(centre, radius)], "cones": [((ca, ra), (cb, rb))]}; lines: text under the panels."""
    points = np.asarray(points, float)
    faces = np.asarray(faces, np.int64)
    body = np.asarray(body, float).reshape(-1, 3, 3)
    rgb = np.tile(np.asarray(CLOTH, float), (len(points), 1))
    legend = None
    if values is not None:
        values = np.asarray(values, float)
        plain = np.isfinite(values)
        for mask, _ in (special or {}).values():
            plain &= ~np.asarray(mask, bool)
        low = float(np.min(values[plain])) if low is None and plain.any() else (low or 0.0)
        high = float(np.max(values[plain])) if high is None and plain.any() else (high or 1.0)
        rgb[plain] = heat(values[plain], low, high)
        legend = (low, high)
        clipped = bool(np.any(values[plain] > high))
    for mask, colour in (special or {}).values():
        rgb[np.asarray(mask, bool)] = colour
    face_rgb = rgb[faces].mean(1)
    everything = np.concatenate([body.reshape(-1, 3), points])
    marks = np.asarray(marks, int).ravel()
    dots = []
    if pins is not None and np.any(pins):
        dots.append((points[np.asarray(pins, bool)], PIN, 2))
    if len(marks):
        dots.append((points[marks], MARK, 4))
    labels = [(points[m], str(m)) for m in marks[:16]]
    w, h = panel_px
    panels = []
    for yaw in yaws:
        right, up, _ = camera(yaw, pitch)
        x, y = everything @ right, everything @ up
        bounds = (x.min() - 0.03, x.max() + 0.03, y.min() - 0.03, y.max() + 0.03)
        scale = min(w / (bounds[1] - bounds[0]), h / (bounds[3] - bounds[2]))
        size = (int((bounds[1] - bounds[0]) * scale), int((bounds[3] - bounds[2]) * scale))
        panels.append((yaw, _panel(size, yaw, pitch, bounds, scale, body, points[faces], face_rgb, WIRE, dots,
                                   arrows, shapes or {}, labels)))
    font = _font(15)
    width = max(sum(p.size[0] for _, p in panels), 720)
    text = [piece for line in [title] + list(lines) for piece in _wrap(line, font, width - 16)]
    legend_h = 44 if legend or special else 0
    height = max(p.size[1] for _, p in panels)
    sheet = Image.new("RGB", (width, height + legend_h + 20 * len(text) + 10), BACKGROUND)
    d = ImageDraw.Draw(sheet)
    x = 0
    for yaw, panel in panels:
        sheet.paste(panel, (x, 0))
        d.text((x + 6, 4), f"yaw {yaw}", font=font, fill=INK)
        x += panel.size[0]
    y = height + 6
    cursor = 10
    if legend:
        low, high = legend
        bar = heat(np.linspace(low, high, 240), low, high).astype(int)
        for i, c in enumerate(bar):
            d.line([(cursor + i, y), (cursor + i, y + 14)], fill=tuple(c))
        for k in range(5):
            v = low + (high - low) * k / 4
            label = f"{v:.3g}" + ("+" if k == 4 and clipped else "")
            d.text((cursor + 60 * k - 6, y + 16), label, font=_font(12), fill=INK)
        cursor += 252
        d.text((cursor, y), unit, font=font, fill=INK)
        cursor += int(font.getlength(unit)) + 24
    for name, (mask, colour) in (special or {}).items():
        label = f"{name} ({int(np.sum(mask))})"
        d.rectangle([cursor, y, cursor + 14, y + 14], fill=colour)
        d.text((cursor + 18, y), label, font=font, fill=INK)
        cursor += 18 + int(font.getlength(label)) + 18
    y += legend_h
    for i, line in enumerate(text):
        d.text((8, y + 20 * i), line, font=font, fill=INK)
    if path:
        sheet.save(path)
    return sheet


def _wrap(line, font, width):
    words, out = line.split(" "), [""]
    for word in words:
        trial = f"{out[-1]} {word}".strip()
        if font.getlength(trial) <= width or not out[-1]:
            out[-1] = trial
        else:
            out.append("    " + word)
    return out


def body_triangles(body, matrices=None):
    """The body's triangles at rest or skinned by one frame of matrices (J, 4, 4)."""
    if matrices is None:
        return body.points[body.faces]
    return M.skin_points(body.points, body.weights, matrices[None])[0][body.faces]


def piece_card(path, piece, layer="wire", matrices=None, marks=(), lines=(), yaws=(0, 90, 180), shapes=None, title=None):
    """A card of one authored piece (outfit.Piece). layer: wire, r, d, R, b, weights:<joint>, or fail (the marks
    only); weights:A+B shows the sum of joints' weights; matrices: (J, 4, 4) skin matrices of a pose (model.skin_matrices of one frame), None for rest: the sim
    mesh is then its reference in that pose. The why-lines of the marked vertices go under the panels."""
    body = body_triangles(piece.outfit.body, matrices)
    x = piece.x if matrices is None else M.skin_points(piece.x, piece.w, matrices[None])[0]
    pins = piece.pins
    values, unit, special, arrows, low, high = None, "", {}, None, None, None
    if layer in ("r", "d"):
        values, unit = getattr(piece, layer) * 1000, f"{layer} mm"
        special = {"pin": (pins, PIN)}
        low = 0.0
    elif layer == "R":
        # Above a body's scale the sphere is all but a plane; the ramp spends its colours on limbs and hips.
        values, unit, low, high = piece.R * 1000, "R mm (body radius behind)", 0.0, 300.0
        special = {"pin": (pins, PIN), "off": ((piece.R == 0) & ~pins, OFF), "plane": (np.isinf(piece.R), PLANE)}
    elif layer.startswith("weights:"):
        joints = layer.split(":", 1)[1]
        values = sum(piece.f.w[j] for j in joints.split("+"))
        unit, low, high = f"weight of {joints}", 0.0, 1.0
    elif layer == "b":
        b = piece.b if matrices is None else M.skin_directions(piece.b, piece.w, matrices[None])[0]
        arrows = (x, x + 0.03 * b)
    elif layer not in ("wire", "fail"):
        raise ValueError(f"unknown layer {layer!r}")
    groups = {}
    for m in np.asarray(marks, int).ravel()[:12]:
        text = "; ".join(f"{k} <- {v}" for k, v in piece.why(int(m)).items() if k != "values")
        groups.setdefault(text, []).append(f"v{m}")
    why = [f"{', '.join(ids)}: {text}" for text, ids in groups.items()]
    return draw(path, body, x, piece.faces, values=values, low=low, high=high, unit=unit, special=special,
                pins=pins if layer in ("wire", "fail", "b") or layer.startswith("weights") else None, marks=marks,
                arrows=arrows, shapes=shapes, title=title or f"{piece.id}: {layer}", lines=list(lines) + why, yaws=yaws)
