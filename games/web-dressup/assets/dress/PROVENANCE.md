# Dress art provenance

- **origin**: ai
- **tool**: xAI Imagine `image_edit` (worn-on-body) + `tools/body_peel.py` (nude vs dressed peel)
- **style lock**: cute fashion illustration, front-view mobile dress-up sprite
- **layer contract** (default outfit, worn-peel path):
  - `body_base` = bald underwear doll only (from locked nude plate)
  - **each clothing layer** = full-frame RGBA peeled from *dressed plate − nude plate* (same pose/camera)
  - no slot stretch: garment is already registered to the body
  - `*_full.png` = catalog thumb = body + layer composite
- **pipeline** (`tmp/dress_gen/worn_peel/`):
  1. Lock `nude_plate.png` (body only)
  2. `image_edit` nude → dressed with ONE garment, pose frozen
  3. `body_peel.py --nude … --dressed …` → `layer_*.png`
  4. Copy layers to `assets/dress/`; rebuild pack
- **legacy**: isolated gray-bg gens + `process_dress_art.py` place (SLOT_FIT) remain for non-default catalog items until re-worn
- **UI**: soft pastel fashion chrome in `assets/ui/` (procedural PIL)
- **date**: 2026-07-09 (worn-peel default: hair_bob, top_tee, bot_jeans, shoe_sneak, acc_glasses)
