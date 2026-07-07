# image/birefnet_cutout — BiRefNet-general neural cutout (arbitrary background)

Niche (bench 2026-07-07, `tmp/alpha_bench/final/metrics_table.md`): a READY
IMAGE on an arbitrary/unknown background -- no chroma key, no aligned
white/black plate pair, no trimap. Use this when there is no flat-key source
and no dual-plate pair to route (`../route/`), only a single photo/render.

Thin wrapper over rembg's ONNX runtime bridge to the BiRefNet-general
checkpoint. It does NOT despill or edge-decontaminate (rembg's own
`naive_cutout` just composites image x mask); it is a mask/alpha source, not a
full hygiene pipeline like `../alpha_matte/`.

## Explicit weaknesses (do not use this where these apply)

- Soft glow / thin or fine detail: loses to CorridorKey (`../alpha_matte/`'s
  keyer family) and ViTMatte on the bench's `glow_wings`/`fur_synthetic`
  fixtures (birefnet aMAE 13.72/5.58 vs vitmatte_auto 1.26/0.63). If a flat-key
  plate or plate pair is available, prefer those paths instead.
- Flat monochrome line-art over a busy background: weak. The bench's
  `char2_busy` fixture (line-art "Demon-orc", CC0, rasterized on a busy photo
  background) scored birefnet alpha-MAE **9.92** vs isnet's 3.95 on the SAME
  fixture -- BiRefNet's salient-object-detection training distribution is
  photographic/rendered natural objects, not vector line art. This is a
  documented domain nuance for routing, not a bug to chase here.
- Its ONE clear win in the bench is exactly its niche: `busy_bg_scavenger`
  (opaque art on a busy background, no line-art), where birefnet was best by
  both alpha-MAE (0.62) and edge-MAE (5.8).

## Entry

- `birefnet_cutout.py` -> `birefnet_cutout(image, *, model="birefnet-general",
  session=None) -> Image` (RGBA, native size preserved -- rembg's own
  contract). `session` is created via `rembg.new_session(model)` unless one is
  injected (reuse a warm session across many calls to avoid paying model-load
  cost per image).
- CLI: `--in <image> --out <png> [--model birefnet-general] [--report <json>]`.
- Missing `rembg` import, or any `--model` other than `birefnet-general`, is a
  LOUD error (`RuntimeError`/`ValueError`) -- no silent fallback to a
  different cutout method.

## Report schema

`ai_studio.image.birefnet_cutout_report.v1`:
`{schema, tool, model, device: "cpu-onnxruntime", seconds, in_size, out_size,
rembg_version}`.

## License

Recorded verdict (primary sources, lead-ratified 2026-07-07):

- `rembg` (the wrapper package this tool imports) -- **MIT**.
- The `birefnet-general` session's code AND weights -- **MIT**:
  `ZhengPeng7/BiRefNet`'s own repo LICENSE, and the `birefnet-general`
  checkpoint's Hugging Face model card states `license: mit`. rembg's
  `birefnet-general` session downloads an ONNX conversion of that exact
  checkpoint from rembg's own GitHub releases (`BiRefNet-general-epoch_244.onnx`).
- **NEVER substitute `briaai/RMBG-2.0`** (or any other look-alike BiRefNet
  checkpoint) for `birefnet-general` -- RMBG-2.0's weights are
  **non-commercially licensed**, unlike birefnet-general's MIT weights. This
  is enforced in code: `ALLOWED_MODELS = ("birefnet-general",)`, and any other
  model id raises a `ValueError` naming this exact prohibition.

## Model download

Weights auto-download on first use (~930MB, one-time; cached afterward) to
`U2NET_HOME` if set, else `~/.u2net` (rembg's own convention, honored
unmodified by this wrapper -- see `rembg.sessions.base.BaseSession.u2net_home`).

## Python deps

rembg, onnxruntime, Pillow (studio `.venv`; rembg pulls numpy/scipy/pooch/
scikit-image/pymatting transitively -- only rembg + onnxruntime are pinned as
top-level per this file's existing philosophy, see `../requirements.txt`).

## Tests

- `birefnet_cutout_test.py` -- OFFLINE, no model download, no network:
  model-allowlist refusal (RMBG-2.0 rejected with the license message), CLI
  arg validation refusals (missing `--in`, forbidden `--model`), and a
  session-injection path that exercises rembg's REAL `remove()`/
  `naive_cutout()` compositing against a stub `predict()`-only session.
  (`.venv/Scripts/python.exe -m unittest
  ai_studio.assets.tools.image.birefnet_cutout.birefnet_cutout_test`).
- `birefnet_smoke.py` -- LIVE, runs the real model on two bench fixtures
  (`tmp/alpha_bench/scav_magenta.png` flat-key plate,
  `tmp/alpha_bench/fixtures/char2_busy.png` the busy-bg line-art fixture).
  NOT part of `unittest` discovery (no `TestCase`, no `_test.py` suffix) --
  downloads the model on first run. Run manually:
  `.venv/Scripts/python.exe ai_studio/assets/tools/image/birefnet_cutout/birefnet_smoke.py`.

## Bench timings

CPU onnxruntime, native resolution, no resize: 7-9s/image on the T0265 bench
fixtures (512x704 to ~1400x2000); ~1s/image for the HR variant (see
`tmp/alpha_bench/final/metrics_table.md`, `timings_rembg.json`,
`timings_birefnet_hr.json`). First run additionally pays the one-time ~930MB
model download.
