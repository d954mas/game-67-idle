# Generate a small equirectangular Radiance .hdr "studio" environment used by
# BOTH the Blender thumbnail renderer (world environment texture) and the web
# model-viewer (environment-image). Sharing one HDR means a model's preview PNG
# and its live 3D are lit by the SAME source instead of two different rigs.
#
# The rig: a high, warm, tight KEY (so the top face is brightest), a dim cool
# wide FILL from the opposite side (so the shadow side is not black), and a LOW
# ambient floor. Low ambient is what keeps the vertical faces visibly darker
# than the top, so edges read clearly - the whole point of the preview.
#
#   python ai_studio/assets/backlog/storage/previews/make_studio_hdr.py [out.hdr]
#
# Output is a flat (non-RLE) Radiance RGBE file, read fine by both Blender and
# three.js' RGBELoader. The ambient floor keeps every pixel's first byte well
# above 2, so no scanline is misread as RLE.
import sys
import math
import os
import numpy as np

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "studio_env.hdr")
W, H = 1024, 512


def unit(el_deg, az_deg):
    el, az = math.radians(el_deg), math.radians(az_deg)
    r = math.cos(el)
    return np.array([r * math.sin(az), math.sin(el), r * math.cos(az)], np.float64)


# Y-up directions. Key high and biased to the front-right side the iso camera
# sees; a high elevation makes the TOP brightest regardless of small azimuth
# differences between the two renderers, so the look stays consistent.
KEY = unit(64, 40)
FILL = unit(18, 220)

us = (np.arange(W) + 0.5) / W
vs = (np.arange(H) + 0.5) / H
phi = (2.0 * np.pi * us)[None, :]
theta = (np.pi * vs)[:, None]            # 0 = up (+Y), pi = down
st = np.sin(theta)
dx = st * np.sin(phi) * np.ones((H, W))
dy = np.cos(theta) * np.ones((H, W))
dz = st * np.cos(phi) * np.ones((H, W))


def lobe(L, power):
    d = np.clip(dx * L[0] + dy * L[1] + dz * L[2], 0.0, 1.0)
    return d ** power


img = np.zeros((H, W, 3), np.float32)
# low ambient with a gentle sky(up)->ground gradient
up = np.clip(dy, 0.0, 1.0)
amb = 0.10 + 0.09 * up
img += amb[..., None] * np.array([0.95, 0.97, 1.0], np.float32)
# tight bright warm key (top face) + a softer warm halo so it isn't a hot dot
img += (7.0 * lobe(KEY, 200))[..., None] * np.array([1.0, 0.95, 0.86], np.float32)
img += (1.1 * lobe(KEY, 6))[..., None] * np.array([1.0, 0.97, 0.92], np.float32)
# dim cool broad fill from the opposite side
img += (0.6 * lobe(FILL, 4))[..., None] * np.array([0.86, 0.91, 1.0], np.float32)


def write_hdr(path, rgb):
    h, w, _ = rgb.shape
    maxc = np.maximum.reduce(rgb, axis=2)
    nz = maxc > 1e-32
    safe = np.where(nz, maxc, 1.0)
    m, e = np.frexp(safe)                 # safe = m * 2**e, m in [0.5, 1)
    scale = np.where(nz, m * 256.0 / safe, 0.0)
    rgbe = np.zeros((h, w, 4), np.uint8)
    rgbe[..., 0] = np.clip(rgb[..., 0] * scale, 0, 255).astype(np.uint8)
    rgbe[..., 1] = np.clip(rgb[..., 1] * scale, 0, 255).astype(np.uint8)
    rgbe[..., 2] = np.clip(rgb[..., 2] * scale, 0, 255).astype(np.uint8)
    rgbe[..., 3] = np.where(nz, e + 128, 0).astype(np.uint8)
    header = ("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n" % (h, w)).encode("ascii")
    with open(path, "wb") as f:
        f.write(header)
        f.write(rgbe.tobytes())


write_hdr(OUT, img)
print("wrote", OUT, "%dx%d" % (W, H))
