# Raster 2D Region Detection

Detect isolated regions on a normalized chroma-key source sheet and emit rects
for slicing.

Current tool:

```powershell
py -3.12 ai_studio/assets/tools/raster2d/regions/detect_regions.py `
  --source tmp/ai_studio/assets/raster2d/session/background/normalized.png `
  --key-color "#ff00ff" `
  --key-tolerance 0 `
  --min-area 256 `
  --merge-distance 0 `
  --padding 8 `
  --json-output tmp/ai_studio/assets/raster2d/session/regions/regions.json `
  --overlay-output tmp/ai_studio/assets/raster2d/session/regions/overlay.png
```

The output schema is `ai_studio.raster2d.detected_regions.v1`.
Each region contains:

- `content_bbox`: tight detected content bounds.
- `rect`: padded crop rect to use for slicing.
- `area_px`: foreground pixel count used to filter noise.

In the site flow, this runs after `../background/normalize_background.py`. The
site then lets the user add/delete rects and writes reviewed rect JSON under
`tmp/`.

This tool only finds initial rects. It does not decide asset names, licenses,
provenance, storage, alpha cleanup, or game-engine import policy.
