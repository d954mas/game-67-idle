"""Static cards of a posed garment for agents that have no GUI: shaded triangle layers seen from a yaw, the sim mesh
with its wire beside the drawn mesh, marked vertices and measure lines printed on the card.

Orthographic, Y up; yaw 0 looks at the front of an actor facing +Z, positive yaw walks the camera to her left, and the
camera looks down by `pitch_deg`. Faces are drawn far to near; a face seen from behind is darker, so the inside of a
skirt reads as inside."""
from dataclasses import dataclass, field

import numpy as np
from PIL import Image, ImageDraw, ImageFont

BODY = (172, 172, 168)
SEAT = (92, 74, 62)
INK = (24, 24, 28)
MARK = (230, 40, 40)
PALETTE = ((70, 110, 200), (200, 120, 60), (90, 160, 90), (170, 80, 170), (60, 160, 170), (180, 160, 60))


@dataclass
class Layer:
    triangles: np.ndarray  # (n, 3, 3) model space
    colour: tuple
    wire: tuple = None     # outline colour of each face, None for none


@dataclass
class Panel:
    title: str
    layers: list
    marks: np.ndarray = field(default_factory=lambda: np.zeros((0, 3)))


def camera(yaw_deg, pitch_deg=12.0):
    """Right, up and toward-camera unit vectors."""
    a, p = -np.deg2rad(yaw_deg), np.deg2rad(pitch_deg)
    right = np.array([np.cos(a), 0.0, -np.sin(a)])
    forward = np.array([np.sin(a), 0.0, np.cos(a)])
    toward = forward * np.cos(p) + np.array([0.0, 1.0, 0.0]) * np.sin(p)
    return right, np.cross(toward, right), toward


def frame(points, yaw_deg, pitch_deg=12.0, margin=0.04):
    """Screen bounds (x0, x1, y0, y1) in metres that hold the points from this yaw."""
    right, up, _ = camera(yaw_deg, pitch_deg)
    x, y = points @ right, points @ up
    return x.min() - margin, x.max() + margin, y.min() - margin, y.max() + margin


def _draw(image, panel, yaw_deg, pitch_deg, bounds, scale):
    right, up, toward = camera(yaw_deg, pitch_deg)
    light = -0.35 * right + 0.55 * up + 0.75 * toward
    light /= np.linalg.norm(light)
    draw = ImageDraw.Draw(image)
    x0, x1, y0, y1 = bounds
    width, height = image.size

    def pixels(p):
        return np.stack([(p @ right - x0) * scale, height - (p @ up - y0) * scale], axis=-1)
    faces, depth, fills, outlines = [], [], [], []
    for layer in panel.layers:
        tri = np.asarray(layer.triangles, float).reshape(-1, 3, 3)
        if not len(tri):
            continue
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        front = n @ toward >= 0
        lit = 0.38 + 0.62 * np.abs(n @ light)
        lit = np.where(front, lit, 0.55 * lit)
        colour = np.asarray(layer.colour, float)
        fills.append(np.clip(lit[:, None] * colour, 0, 255).astype(int))
        outlines += [layer.wire] * len(tri)
        faces.append(pixels(tri))
        depth.append(tri.mean(axis=1) @ toward)
    if faces:
        faces, depth, fills = np.concatenate(faces), np.concatenate(depth), np.concatenate(fills)
        for i in np.argsort(depth):
            draw.polygon([tuple(v) for v in faces[i]], fill=tuple(fills[i]), outline=outlines[i])
    for x, y in pixels(np.asarray(panel.marks, float).reshape(-1, 3)):
        draw.ellipse([x - 3, y - 3, x + 3, y + 3], fill=MARK)


def card(path, panels, yaw_deg, bounds, lines=(), panel_px=(520, 640), pitch_deg=12.0):
    """One card: the panels side by side from one yaw within the same screen bounds, the lines below them; saved as PNG
    when a path is given."""
    x0, x1, y0, y1 = bounds
    w, h = panel_px
    scale = min(w / (x1 - x0), h / (y1 - y0))
    w, h = int(round((x1 - x0) * scale)), int(round((y1 - y0) * scale))
    font = _font(15)
    text_h = 22 * (len(lines) + 1) + 8
    sheet = Image.new("RGB", (w * len(panels), h + text_h), (248, 247, 244))
    draw = ImageDraw.Draw(sheet)
    for k, panel in enumerate(panels):
        # Drawn at twice the size and reduced, so edges are smooth.
        big = Image.new("RGB", (2 * w, 2 * h), (248, 247, 244))
        _draw(big, panel, yaw_deg, pitch_deg, bounds, 2 * scale)
        sheet.paste(big.resize((w, h), Image.LANCZOS), (k * w, 0))
        draw.text((k * w + 8, 6), f"{panel.title}   yaw {yaw_deg}°", font=font, fill=INK)
    for i, line in enumerate(lines):
        draw.text((8, h + 6 + 22 * i), line, font=font, fill=INK)
    if path:
        sheet.save(path)
    return sheet


def _font(size):
    for name in ("arial.ttf", "DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()
