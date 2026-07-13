# Image Asset Tools

`ai_studio/assets/tools/image/` is the media-type umbrella for working with
raster images: sourcing, background normalization, region detection, slicing,
alpha extraction (key-matte and dual-plate), and routing. It is a peer tier to
future `assets/tools/model3d/`, `assets/tools/audio/`, etc.

Each tool is decomposed into its own folder: its Python entry script, its tests,
and a README describing what it does, its entry function, and its Python deps.
Shared plumbing (the Python interpreter, tmp-path confinement, session dirs,
JSON/image helpers) lives once in `_bridge/`.

## Python environment

The tools run against the repo-local, gitignored root `.venv/` resolved from
`studio.config.json`. Create or repair it with the one-shot setup script (from
the repo root):

```
node ai_studio/assets/tools/image/_bridge/setup_python.mjs
```

The interpreter is resolved from `ai_studio/studio.config.json` -> `pythonPath`
(`.venv/Scripts/python.exe`) ONLY. There is no PATH search or fallback chain: a
missing venv or a missing dependency is a loud error naming this setup command.
Pinned dependency versions live in `requirements.txt`.

## Tools

- `_bridge/` — shared Node plumbing (interpreter resolution, tmp confinement,
  session dirs, JSON/image helpers) plus the Python setup script and pinned
  `requirements.txt`. Not a tool itself; imported by the per-tool bridges.
- `alpha_matte/` — single-background known-key cutout (path 1): opaque art and
  flat-key holes. `key_matte.py` + the `chroma_key_alpha.py` keying primitive.
- `alpha_dualplate/` — fractional alpha from an aligned white/black plate pair
  (path 2): glow, glass, soft shadow. `dual_plate_alpha.py` + the pair gate.
- `birefnet_cutout/` — neural cutout (path 3) for a ready image on an
  arbitrary/unknown background, no key or plate pair needed; MIT-licensed
  BiRefNet-general via rembg only, weak on line-art/soft-glow (bench-documented).
- `vitmatte_matte/` — ViTMatte auto-trimap alpha matting (path 4) for thin/fine
  detail (spider-web, mesh, fur, hair) on a flat-key plate; second priority on
  glow/soft-bloom (CorridorKey is first). Runs in its OWN gitignored venv (GPU
  torch, cu128) — never the shared `.venv/`; see its README for setup and the
  mandatory license verdict (weights local-only, Adobe-DIM caveat).
- `route/` — decides key-matte vs dual-plate for a flat-key source crop before a
  generation is spent.
- `bg_fix/` — border-connected background chroma normalization (snap to key).
- `regions/` — region detection over a normalized source.
- `slice/` — slice a source by reviewed regions, apply per-region alpha, build
  review sheets and export ZIPs.
- `quantize/` — palette quantization (RGB to N colors, alpha byte-identical),
  the canvas Cleanup section's interactive quantize tool (T0207).
- `denoise/` — light median denoise (RGB only, alpha never filtered), the
  Cleanup section's second tool (T0207).
- `sources/` — source-image upload conventions for the tools.
