# seeker_portrait_hud.png Provenance

- Asset: `games/rb-dark-rpg/assets/ui/seeker_portrait_hud.png`
- Game: `rb-dark-rpg`
- Role: low-noise HUD player portrait for the top character panel.
- Status: accepted generated asset, v2 readability revision.
- Origin: AI-generated with the built-in Codex image generation tool on 2026-07-05.
- License: project-internal generated asset.
- SHA256: `24912175D82A4AD7D204E7A3A07386FB8599A650F460F1FE91FDB9608A7CCA5E`

## Source-First Check

Local shared asset searches were run before generation:

```text
node ai_studio/assets/backlog/storage/search.mjs --query "rb dark rpg seeker player portrait blocky dark fantasy hud avatar"
assets: 0 match(es)

node ai_studio/assets/backlog/storage/search.mjs --query "rb dark rpg clean flat seeker portrait hud icon low noise face"
assets: 0 match(es)
```

No suitable existing shared/library asset was found for a readable player HUD portrait.

## Accepted Generation

- Tool output: `C:\Users\ROG\.codex\generated_images\019f3054-1f6f-76c2-8363-a67f4d402e96\ig_019dd0b49af85a7b016a49f86f3d5c8191a5b8fc53022ebda5.png`
- Workspace raw chroma-key copy: `tmp/imagegen/seeker_portrait_hud_v2_chromakey.png`
- Raw chroma-key SHA256: `64699AFCD09755D9C8142C13485B65F1FEF3D7A645D591BF9901CCC5D33A41FE`
- Workspace cutout: `tmp/imagegen/seeker_portrait_hud_v2_cutout.png`
- Cutout SHA256: `4C360CE50AB22916F872D2D8241B2E2052DE5975409DA9C34A22CC51B7482546`

Prompt:

```text
Use case: stylized-concept
Asset type: low-noise game HUD player portrait icon for rb-dark-rpg top status panel, readable at 48x48 pixels
Primary request: create a clean square portrait icon of the PLAYER character, a blocky hooded seeker, with very large readable face and simple silhouette. This is a small HUD avatar, not detailed splash art.
Scene/backdrop: perfectly flat solid #00ff00 chroma-key background for background removal.
Subject: blocky fantasy seeker / low-status mercenary, dark hood, simple square tan face, two clear dark eyes, one simple mouth, minimal shoulder armor hints. No spear, no plume, no guard helmet, no extra props.
Style/medium: polished 2D game UI icon, flat-shaded Roblox-like dark fantasy, crisp chunky shapes, low texture, low noise, limited color blocks, clean edges.
Composition/framing: centered head-and-shoulders bust, face fills 60 percent of the square, hood and shoulders simplified, generous padding, no cropped edges.
Lighting/mood: clear warm face, dark hood silhouette, enough contrast for tiny HUD use.
Color palette: dark charcoal hood, warm tan face, muted brass/iron accents only in tiny amounts.
Constraints: background must be exactly one uniform #00ff00 color with no shadows, gradients, texture, floor plane, or lighting variation; do not use #00ff00 anywhere in the subject; no text, no watermark, no frame, no UI border, no cast shadow; avoid painterly noise, scratches, tiny stitches, complex texture, busy highlights, photoreal detail, realistic skin pores.
```

## Processing

Background removal used the system imagegen chroma-key helper:

```text
C:\Python312\python.exe C:\Users\ROG\.codex\skills\.system\imagegen\scripts\remove_chroma_key.py --input tmp\imagegen\seeker_portrait_hud_v2_chromakey.png --out tmp\imagegen\seeker_portrait_hud_v2_cutout.png --auto-key border --soft-matte --transparent-threshold 12 --opaque-threshold 220 --despill
```

Helper output:

```text
Key color: #02f604
Transparent pixels: 704032/1572516
Partially transparent pixels: 4488/1572516
```

The accepted cutout was cropped and downscaled to a 256x256 transparent HUD asset to improve small-size readability and reduce atlas noise.

## Superseded Draft

The earlier generated portrait was more detailed and painterly, but it became noisy at HUD size. It is preserved only as intermediate evidence under `tmp/imagegen/` and is superseded by this v2 low-noise icon.

## Canvas Handoff

Canvas project created: `rb-dark-rpg-hud-player-portrait-c2f769`.

The image was not added to the configured canvas root because that root is outside the workspace under YandexDisk-backed storage and needs explicit export approval.
