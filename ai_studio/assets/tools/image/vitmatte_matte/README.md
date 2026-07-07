# image/vitmatte_matte — ViTMatte alpha matting on a flat-key plate (path 4)

Niche (lead-ratified, alpha-methods-portfolio bench 2026-07-07): **тонкое** --
spider-web / mesh / fur / hair-strand detail on a flat-key plate, where
`alpha_matte`'s bounded key-distance band and CorridorKey's screen-unmix both
under-resolve strand-level structure (alpha ~2x more accurate than
CorridorKey there). **Second priority on glow/soft-bloom** on a flat key:
CorridorKey is first there (native despill, only spill-tested method); ViTMatte
still wins the raw alpha error on glow, but after despill CorridorKey's result
carries less residual background tint -- the lead's own eye ruling on the
glow-wings comparison, so route glow to CorridorKey first and to this tool
only as the fallback / when CorridorKey's stack (own CUDA venv, CC-BY-NC-SA-4.0
asset carve-out) isn't available.

Not for: opaque art / flat-key holes (`../alpha_matte/` is cheaper and
deterministic there), translucent uniform partial alpha like ghost/glass
(mathematically unrecoverable from one plate -- `../alpha_dualplate/`), or an
arbitrary/unknown real background with no flat plate at all (`../birefnet_cutout/`
is the reviewed path there; MIT, weak on line-art/soft-glow). This tool's own
`build_mask_seeded_trimap` busy-bg fallback is unsolved in the bench (GT-trimap
ceiling only 7.39 aMAE) -- treat it as a last resort, not a production path.

## Auto-trimap

Works from a flat chroma plate with ZERO manual annotation, using the SAME
protocol the canvas conveyor already keys against (MAGENTA / GREEN): chroma
distance to the known key colour. Distance below `T1=70` -> sure background;
distance above `T2=150` (then eroded) -> sure foreground; the remaining band,
dilated 7px -> unknown (fed to the model as the trimap's grey region). `T1`/
`T2` were tuned once on the bench's `opaque_hard_scavenger` fixture and then
frozen for every flat-key fixture -- they are not re-tuned per image.

For busy/real backgrounds with no flat plate, `build_mask_seeded_trimap` seeds
the same erode/dilate recipe from a coarse neural mask's alpha (e.g. an
SOD/rembg pass) instead of a chroma distance. The bench found this an open
problem (GT-trimap ceiling only 7.39 aMAE) -- treat it as a fallback, not a
production path; prefer keying on a clean plate whenever capture is
controllable.

## Despill

ViTMatte (like every alpha-only matting model) returns ALPHA ONLY: its output
RGB is the untouched plate RGB, so every fractional-alpha pixel still carries
a visible key-colour halo. Given the compositing equation
`plate = fg*a + key*(1-a)`, the chroma un-blend solves for `fg`:

```
fg = (plate - key * (1 - a)) / a
```

Below alpha `0.02` the divide is numerically degenerate and the pixel is
indistinguishable from background anyway, so it is clamped to fully
transparent black (RGB zeroed; the caller also zeroes alpha there). Recovered
RGB leak 32.1 -> 0.5 (`rgb_leak_mean_abs`) on the `web_synthetic` (spider-web)
bench fixture. Despill is applied by default; pass `--no-despill` to keep raw
plate RGB untouched.

## Entry

- `matte_math.py` -- PURE numpy/scipy/PIL module (no torch import; importable
  by the SHARED repo `.venv`): `build_auto_trimap`, `build_mask_seeded_trimap`,
  `trimap_stats`, `despill`.
- `vitmatte_matte.py` -- the model glue, runs ONLY in this tool's own venv:
  `load_model`, `predict_alpha`, `predict_alpha_with_oom_fallback` (GPU with
  CPU fallback on `torch.cuda.OutOfMemoryError`). CLI:

  ```
  .venv/Scripts/python.exe ai_studio/assets/tools/image/vitmatte_matte/vitmatte_matte.py \
    --in <plate.png> --key R,G,B --out <rgba.png> \
    [--report <json>] [--trimap-out <png>] [--no-despill]
  ```

  (that `.venv` is THIS TOOL's own -- see Setup below, not the shared repo one.)
- `vitmatte_smoke.py` -- live GPU smoke against the bench's glow-wings magenta
  plate; see Tests.

## Setup (own venv)

GPU torch (cu128, ~2.7GB) must NEVER enter the shared repo `.venv/` -- this
tool gets its own, created inside its own folder and gitignored:

```
node ai_studio/assets/tools/image/vitmatte_matte/setup_python.mjs
```

Creates `ai_studio/assets/tools/image/vitmatte_matte/.venv/` from `py -3.12`
and installs the pinned, tool-local `requirements.txt` (torch/torchvision from
the `download.pytorch.org/whl/cu128` index, everything else from PyPI). A
missing venv or dependency is a loud error naming this exact command -- no
silent fallback. Model weights (~700MB, Hugging Face Hub cache) download on
first run, not at setup time.

## Tests

- `matte_math_test.py` -- offline, SHARED repo venv (no GPU, no network, no
  torch):
  ```
  .venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.vitmatte_matte.matte_math_test
  ```
- `vitmatte_smoke.py` -- LIVE, this tool's own venv, real GPU + real model
  download:
  ```
  ai_studio/assets/tools/image/vitmatte_matte/.venv/Scripts/python.exe ai_studio/assets/tools/image/vitmatte_matte/vitmatte_smoke.py
  ```

## License

**ALLOW-WITH-CONDITIONS** (recorded on the taskboard; re-verify before any
change to the allowed checkpoint or a commercial ship decision):

- Code: `hustvl/ViTMatte` is MIT licensed.
- The model card for `hustvl/vitmatte-base-composition-1k` tags itself
  `apache-2.0`, and the author explicitly blessed weight use on the upstream
  repo (issue #9, JingfengYao): *"Our repo is totally under the MIT license.
  Feel free to use our model weights as long as you follow MIT license."*
- BUT the checkpoint is fine-tuned on Composition-1k, which is built on the
  **Adobe Deep Image Matting** dataset, whose terms are **noncommercial**.
  That caveat is not cleared by the author's MIT statement above -- it is a
  separate, unresolved upstream-data restriction.

Conditions this tool enforces / assumes:

- **Weights are LOCAL-ONLY.** They download to the Hugging Face Hub cache on
  first run and are NEVER committed to the repo or redistributed.
- `vitmatte_matte.py` **allowlists** `hustvl/vitmatte-base-composition-1k` as
  the only permitted checkpoint; pointing it at any other model raises a loud
  license-scope error instead of silently running.
- This is the **second-priority** engine (CorridorKey first on glow; see
  Niche above) -- not the sole path, so exposure is bounded.
- **Final commercial-use call belongs to the lead.** This README documents the
  verdict as reviewed; it does not itself authorize shipping a commercial
  product built on these weights.
