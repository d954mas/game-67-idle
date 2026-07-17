# Post-cutout asset quality gate

`asset_quality_gate.py` is the deterministic T0317 gate for prepared RGBA
cutouts. It runs after matte/cutout work and before an asset is accepted or
promoted. It emits one `PASS|FAIL` line and a machine-readable report. Failed
evaluations also emit a small PNG crop around the problem area; passing assets
do not receive a red problem marker.

```powershell
node ai_studio/dev_environment/python_run.mjs `
  ai_studio/assets/tools/image/quality_gate/asset_quality_gate.py `
  --source tmp/prepared/icon.png `
  --key-color '#FF00FF' `
  --thresholds tmp/icon-thresholds.json `
  --json-output tmp/icon-quality.json `
  --problem-thumbnail tmp/icon-problem.png
```

Omit `--key-color` for art generated with native transparency. Its spill and
halo thresholds must be `null`; alpha, crop, and aspect checks still run. Chroma
mode accepts only the style-lock's canonical magenta or green keys. This
Canvas exposes the trusted evaluator through `asset-status-check`; PASS writes
the report and moves the image to `checked`, while FAIL writes the report plus
problem thumbnail and moves it to `quarantine`. Request callers cannot provide a
verdict. Automatic invocation from alpha outputs and game-asset promotion remain
later T0317 slices.

## Metric formulas

All alpha classification uses the shared visible cutoff `alpha > 12`.

- `spill_edge_ratio`: source-key-like visible edge pixels divided by all
  visible pixels within Chebyshev distance 2 of transparency. Source-key masks
  reuse `alpha_matte/chroma_key_alpha.py`.
- `halo_edge_ratio`: softer edge chroma pointing toward the selected key,
  excluding the stronger spill pixels above, divided by the same visible-edge
  denominator. Opposite-key green/magenta art is a committed negative control.
- `alpha_noise_ratio`: isolated visible pixels with at most one visible
  8-neighbor plus transparent pinholes with at least seven visible 8-neighbors,
  divided by the two-sided one-pixel alpha transition band.
- `empty_margin_ratio`: `1 - visible_bbox_area / canvas_area`; the bbox ignores
  alpha values at or below 12 so faint garbage cannot hide excessive padding.
- `aspect_relative_error`: `abs(actual - expected) / expected`, where actual is
  the file width/height ratio.

An empty cutout and a fully opaque result fail independently of thresholds.
The latter catches the gross failure where a keyed background survived as an
opaque image.

## Style-lock mapping and calibration

The accepted `design/style_lock.json` owns `technical_gate`. A caller maps it to
the evaluator input without inventing values:

```json
{
  "max_spill_edge_ratio": "technical_gate.max_spill_edge_ratio",
  "max_halo_edge_ratio": "technical_gate.max_halo_edge_ratio",
  "max_alpha_noise_ratio": "technical_gate.max_alpha_noise_ratio",
  "max_empty_margin_ratio": "technical_gate.max_empty_margin_ratio",
  "aspect_ratio": {
    "width": "asset_size.width",
    "height": "asset_size.height",
    "max_relative_error": "technical_gate.max_aspect_relative_error"
  }
}
```

The committed example values separate both the synthetic contract corpus and
the preserved VibeJam calibration corpus in `fixtures/jam_corpus/`. The latter
comes from the closed `rb-dark-rpg-vibejam1-2026-07-05` tag with its original
project-internal license, provenance paths, source blob ids, and SHA-256 values
recorded in `manifest.json`. The metric ratio limits remain identical to the
committed example; each fixture supplies its provenance-backed intended width
and height for the aspect check. The known green-spill failure is rejected for
both `key_spill` and `edge_halo`, while the neighboring city-style-v2 accepted
Black Sun prop remains clean. No formula or metric-limit relaxation was needed.
