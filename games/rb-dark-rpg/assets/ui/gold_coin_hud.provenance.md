# gold_coin_hud.png

- Origin: project-internal generated HUD icon, AI-assisted design iteration.
- Generated: 2026-07-05.
- Tool: OpenAI image generation, with local raster cleanup for alpha, crop, and HUD-scale smoothing.
- Source-first search: `node ai_studio/assets/backlog/storage/search.mjs --query "gold coin icon ui currency low noise"` returned 0 local matches.
- Rejected raw source: `tmp/imagegen/gold_coin_hud_chromakey.png`.
- Rejected cutout source: `tmp/imagegen/gold_coin_hud_cutout.png`.
- Rejected smooth source: `tmp/imagegen/gold_coin_hud_flat_cutout.png`; it read as a yellow disk rather than a coin.
- Rejected simplified source: `tmp/imagegen/gold_coin_hud_simple_source.png`; it lost the angled coin style.
- Final source: `tmp/imagegen/gold_coin_hud_side_solid_source.png`, regenerated with the same angled coin form, one smooth side band, and no bottom scratches, then cleaned to transparent atlas input, softly denoised, posterized, quantized, and saved as a smaller 128x128 HUD source to reduce gradient noise.
- Final asset: `games/rb-dark-rpg/assets/ui/gold_coin_hud.png`.
- Final SHA-256: `E915864A0FAA01F04144858615D559434B9E85015331EBC70EC19E4BDCB12498`.

Prompt summary:

```text
Clean readable gold coin currency icon for a non-pixel RPG HUD, same angled coin form with visible side thickness, one smooth solid bronze side band with no facets or side notches, clean bottom edge with one broad shadow and no scratches, thick clean dark bronze outline, raised rim, large embossed crown mark, smooth edges, reduced gradients, reduced micro-texture, no letters.
```
