# image/bg_fix — background normalization

Normalize generated or sourced sheet backgrounds before region detection.

Run from the repository root:

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/assets/tools/image/bg_fix/normalize_background.py `
  --source tmp/ai_studio/assets/raster2d/session/source/raw.png `
  --output tmp/ai_studio/assets/raster2d/session/background/normalized.png `
  --key-tolerance 48 `
  --json-output tmp/ai_studio/assets/raster2d/session/background/normalize_report.json
```

By default the key color is estimated from the image border. `--key-color` can
still override it for diagnostics or special cases.

The operation is border-connected: only key-like pixels reachable from the image
edge are normalized. Interior key-colored pixels are preserved for later alpha
handling or manual review.
