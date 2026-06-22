# Provenance — Cozy Automation raw art

## 2026-06-22 — Cozy garden first-slice sprite set (AI-generated, project-owned)

- Game: `Cozy Automation` (cozy-automation).
- Status: project-owned original assets. AI-generated; no third-party stock,
  no external license obligations. Free to use, modify, and ship in this project.
- Method: raster images generated with the official Codex CLI image tool
  (`imagegen`, gpt-image-2) via
  `.codex/skills/delegated-image-generation/scripts/codex_imagegen.sh`
  (`codex exec`, ChatGPT-plan auth). Props were generated on a flat pure-magenta
  key (`#FF00FF`) background, then keyed to transparent alpha with the project's
  cutout tooling (`tools/assets/cutout/key_matte.py` `key_matte_cutout`) and
  resized with the premultiplied LANCZOS resize in
  `tools/assets/chroma_key_alpha.py`. `bg.png` was generated as a full opaque
  scene (no key) and resized only.
- Art direction: bright, saturated, friendly, cozy casual-game illustration;
  warm sunny palette; soft rounded shapes; clean readable silhouettes;
  flat-ish shading; not realistic, not pixel-art.
- Raw (pre-cutout) source PNGs kept in `tmp/cozy_gen/` until accepted.

### Files (in this folder)

| file | dims | background |
| --- | --- | --- |
| `bg.png` | 1024x576 | OPAQUE full scene (sky, clouds, hills, fence, grass) |
| `plot.png` | 256x256 | transparent — empty tilled soil bed |
| `bush.png` | 256x256 | transparent — round berry bush producer |
| `greenhouse.png` | 256x256 | transparent — small glass greenhouse |
| `basket.png` | 256x256 | transparent — wicker basket of berries (stockpile) |
| `berry.png` | 128x128 | transparent — single ripe berry + leaf (icon/particle) |
| `lock.png` | 128x128 | transparent — golden padlock (locked plot) |
| `panel.png` | 256x256 | transparent — cozy 9-slice UI card (cream fill, even wood border) |
