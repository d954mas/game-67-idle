# matte/ — stage 3: RGB frames -> RGBA (alpha)

Two extractors, chosen by the asset (T0257 R2 verdict).

- **Entry:** `matte.mjs` — `runMatte({runDir, tool, screenColor, key})` and a CLI:
  `node matte.mjs --run-dir <dir> [--tool corridorkey|key_matte] [--screen-color green|blue] [--key 0,255,0]`.

### `--tool corridorkey` (default) — glow / translucent / soft-edge
Wraps the **exact invocation T0257 R2 used** against its venv under
`corridorKeyRoot` (studio config / env `CORRIDOR_KEY_ROOT`; the permanent
install at `C:\projects\ai_studio_tools\CorridorKey` since T0335 — no longer
inside the deletable video-gen experiment; commit `97e55a4`):
1. `corridorkey_prep.py` (repo `.venv`) builds `Input/` (raw frames) + a coarse
   green-dominance `AlphaHint/` per frame into a fresh `ClipsForInference/<shot>`.
2. `corridorkey_cli.py run-inference --backend torch --srgb --despill 0
   --no-despeckle --refiner 1.0 --comp --cpu-post --screen-color green
   --image-size 2048 --skip-existing` (CorridorKey venv, invoked DIRECTLY — not
   `uv run`, which would re-sync and could strip the cuda extra). Forced EAGER
   via `CORRIDORKEY_SKIP_COMPILE=1` (the Windows CUDA `max-autotune` path errors
   at cudagraph replay); `PYTHONUTF8=1` so its `->`-glyph logging survives the
   captured cp1251 pipe.
3. `ck_exr_to_rgba.py` (CorridorKey venv, has cv2/OpenEXR) converts the FG+Matte
   EXR pair to straight 8-bit RGBA.

**Licence: CC-BY-NC-SA-4.0** — README carve-out allows processing images for
commercial projects; NO repackaging/reselling the tool or a paid inference API.
A missing venv is a LOUD error pointing at `Install_CorridorKey_Windows.bat`.

### `--tool key_matte` — opaque / non-glow
`run_key_matte.py` (repo `.venv`, `PYTHONPATH=<repo>`) applies the in-repo
`ai_studio.assets.tools.image.alpha_matte.key_matte` per frame against a flat
key colour. Fast, no licence constraint; cannot recover soft fractional alpha.

- **Output:** `<runDir>/matte/frame_%03d.png` (RGBA) + `report.json` (tool,
  settings, commit/licence, per-frame + wall timing).
- **Reserve:** `MatAnyone` (R2 PARTIAL — green-fringes glow) is NOT wired into v1.

### Reused by the canvas alpha op (T0261)
`runCorridorKey()` is exported and imported cross-module by the canvas alpha op
(`ai_studio/assets/canvas/ops.mjs`) as the **one source of truth** for the CorridorKey
invocation: the canvas `"corridorkey"` alpha method stages a single element's pixels as a
1-frame `ClipsForInference` shot, calls `runCorridorKey({framesDir, outDir, screenColor:"green"})`,
and swaps the element to the returned RGBA. The video pipeline behaviour here is unchanged —
only `runCorridorKey`'s export visibility was widened.
