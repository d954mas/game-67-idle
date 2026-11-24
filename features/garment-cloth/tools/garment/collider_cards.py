"""Cards of a body's collider set for agents without a GUI (spec C.4, the colliders layer): the body coloured by the
segment of the shape that holds it, the skin left outside every shape in red where it lies under loose cloth (light
where no cloth needs it), each shape's silhouette in its segment's colour, and the reference vertices a shape holds
deeper than their max distance as black dots. The camera is garment.view's; faces are drawn far to near."""
import numpy as np
from PIL import Image, ImageDraw

from . import colliders as C
from . import model as M
from .view import camera, _font

BACKGROUND = (248, 247, 244)
OUTSIDE = (225, 35, 35)
BARE = (214, 212, 206)
HELD = (10, 10, 12)
INK = (24, 24, 28)
# Qualitative, distinct on the grey of uncovered skin and from the red of skin left outside.
PALETTE = [(46, 110, 200), (230, 150, 30), (40, 160, 110), (150, 80, 190), (30, 170, 200), (190, 160, 40),
           (110, 70, 40), (230, 120, 170), (70, 90, 120), (120, 190, 60), (200, 90, 60), (90, 60, 150),
           (0, 120, 120), (170, 120, 90), (60, 60, 60), (250, 200, 120), (100, 140, 230), (160, 30, 110)]


def loose_faces(body, pieces):
    """Per body face: True where its centroid lies under the loose zone of any piece (colliders.footprint)."""
    centroids = body.points[body.faces].mean(1)
    out = np.zeros(len(body.faces), bool)
    for p in pieces:
        out |= C.footprint(body, p, centroids)[1]
    return out


def held_points(shapes, rig, pieces, matrices):
    """Posed reference points (n, 3) a shape holds deeper than their max distance in one pose (J, 4, 4)."""
    centres = shapes.posed(rig, matrices[None])
    out = []
    for p in pieces:
        ref = M.skin_points(p.rest, p.weights, matrices[None])
        sd, _ = shapes.distance(ref, centres)
        out.append(ref[0][(p.thickness - sd[0] - p.r) > 0])
    return np.concatenate(out) if out else np.zeros((0, 3))


def colours(shapes):
    names = list(dict.fromkeys(shapes.segment(s) for s in shapes.shapes()))
    return {n: PALETTE[k % len(PALETTE)] for k, n in enumerate(names)}


def _panel(size, scale, bounds, yaw, pitch, tri, rgb, shapes, centres, palette, held):
    right, up, toward = camera(yaw, pitch)
    light = -0.35 * right + 0.55 * up + 0.75 * toward
    light /= np.linalg.norm(light)
    x0, _, y0, _ = bounds
    image = Image.new("RGB", size, BACKGROUND)
    draw = ImageDraw.Draw(image)

    def px(p):
        p = np.asarray(p, float).reshape(-1, 3)
        return np.stack([(p @ right - x0) * scale, size[1] - (p @ up - y0) * scale], axis=-1)

    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    lit = 0.45 + 0.55 * np.abs(n @ light)
    lit = np.where(n @ toward >= 0, lit, 0.7 * lit)
    fill = np.clip(lit[:, None] * rgb, 0, 255).astype(int)
    corners = px(tri.reshape(-1, 3)).reshape(-1, 3, 2)
    for i in np.argsort(tri.mean(1) @ toward):
        draw.polygon([tuple(v) for v in corners[i]], fill=tuple(fill[i]))
    for i, j in shapes.shapes():
        colour = tuple(int(0.7 * c) for c in palette[shapes.segment((i, j))])
        (xa, ya), (xb, yb) = px(np.stack([centres[i], centres[j]]))
        ra, rb = shapes.spheres[i].radius * scale, shapes.spheres[j].radius * scale
        for (x, y), r in (((xa, ya), ra), ((xb, yb), rb)):
            draw.ellipse([x - r, y - r, x + r, y + r], outline=colour, width=3)
        axis = np.array([xb - xa, yb - ya])
        if np.linalg.norm(axis) > 1e-6:
            side = np.array([-axis[1], axis[0]]) / np.linalg.norm(axis)
            for s in (-1, 1):
                draw.line([tuple(np.array([xa, ya]) + s * side * ra), tuple(np.array([xb, yb]) + s * side * rb)],
                          fill=colour, width=3)
    for x, y in px(held):
        draw.ellipse([x - 6, y - 6, x + 6, y + 6], fill=HELD, outline=(255, 255, 255), width=2)
    return image


def card(path, body, shapes, rig, matrices=None, loose=None, held=None, yaws=(0, 90, 180, 270), title="", lines=(),
         focus=None, pitch=12.0, panel_px=(330, 560)):
    """Write one card: a panel per yaw. matrices: skin matrices (J, 4, 4) of one pose, None for rest; loose: per body
    face (loose_faces); held: posed points to mark (held_points); focus: points the panels frame (default the body)."""
    matrices = np.stack([np.eye(4)] * len(rig.joints)) if matrices is None else np.asarray(matrices, float)
    points = M.skin_points(body.points, body.weights, matrices[None])[0]
    tri = points[body.faces]
    centres = shapes.posed(rig, matrices[None])[0] if shapes.spheres else np.zeros((0, 3))
    palette = colours(shapes)
    rgb = np.tile(np.asarray(BARE, float), (len(tri), 1))
    if loose is not None:
        rgb[np.asarray(loose, bool)] = OUTSIDE
    if shapes.spheres:
        sd, which = shapes.distance(tri.mean(1)[None], centres[None])
        names = [shapes.segment(s) for s in shapes.shapes()]
        for f in np.flatnonzero(sd[0] <= 0):
            rgb[f] = palette[names[which[0, f]]]
    held = np.zeros((0, 3)) if held is None else np.asarray(held, float).reshape(-1, 3)
    focus = points if focus is None else np.asarray(focus, float).reshape(-1, 3)
    w, h = panel_px
    panels = []
    for yaw in yaws:
        right, up, _ = camera(yaw, pitch)
        x, y = focus @ right, focus @ up
        bounds = (x.min() - 0.04, x.max() + 0.04, y.min() - 0.04, y.max() + 0.04)
        scale = min(w / (bounds[1] - bounds[0]), h / (bounds[3] - bounds[2]))
        size = (int((bounds[1] - bounds[0]) * scale), int((bounds[3] - bounds[2]) * scale))
        # Drawn at twice the size and reduced, so edges are smooth.
        big = _panel((2 * size[0], 2 * size[1]), 2 * scale, bounds, yaw, pitch, tri, rgb, shapes, centres, palette, held)
        panels.append((yaw, big.resize(size, Image.LANCZOS)))
    font = _font(14)
    legend = [(name, colour) for name, colour in palette.items()] + [("skin outside, under loose cloth", OUTSIDE),
                                                                   ("held reference vertex", HELD)]
    width = max(sum(p.size[0] for _, p in panels), 760)
    text = [title] + list(lines)
    rows = 1 + (len(legend) * 150) // width
    height = max(p.size[1] for _, p in panels)
    sheet = Image.new("RGB", (width, height + 22 * rows + 20 * len(text) + 14), BACKGROUND)
    d = ImageDraw.Draw(sheet)
    x = 0
    for yaw, panel in panels:
        sheet.paste(panel, (x, 0))
        d.text((x + 6, 4), f"yaw {yaw}", font=font, fill=INK)
        x += panel.size[0]
    cx, cy = 8, height + 6
    for name, colour in legend:
        label_w = int(font.getlength(name)) + 30
        if cx + label_w > width:
            cx, cy = 8, cy + 22
        d.rectangle([cx, cy, cx + 14, cy + 14], fill=colour)
        d.text((cx + 18, cy), name, font=font, fill=INK)
        cx += label_w
    cy += 24
    for i, line in enumerate(text):
        d.text((8, cy + 20 * i), line, font=font, fill=INK)
    if path:
        sheet.save(path)
    return sheet
