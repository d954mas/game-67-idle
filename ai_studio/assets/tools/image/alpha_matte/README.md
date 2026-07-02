# image/alpha_matte — single-background key-matte alpha (path 1)

Deterministic cutout for OPAQUE art and flat-key holes against a single flat key
colour: known-key trimap -> bounded alpha band -> edge-colour decontamination.
Use this when the alpha is essentially binary with a thin (1-2px) anti-aliased
edge. Soft/fractional alpha (glow, glass, soft shadow) is unrecoverable from one
background -> use `../alpha_dualplate/` instead; `../route/` decides which.

## Entry

- `key_matte.py` -> `key_matte_cutout(image, key, *, exact_tolerance=12,
  foreground_tolerance=80, max_dim=512, timings=None) -> Image` — run PER CROP.
- `chroma_key_alpha.py` — the shared keying primitive (spill masks, despill,
  transparent-edge hygiene, premultiplied resize). Imported by `key_matte.py`
  and cross-imported by `../../source_sheets/audit_intake.py`.

## Python deps

numpy, scipy, Pillow (studio `.venv`; scipy is a hard import — no fallback).

## Tests

`key_matte_test.py`, `chroma_key_alpha_test.py`
(`.venv/Scripts/python.exe -m unittest ai_studio.assets.tools.image.alpha_matte.key_matte_test`).
