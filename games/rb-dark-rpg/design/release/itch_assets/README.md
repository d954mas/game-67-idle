# Itch.io Asset Set

Game: `rb-dark-rpg`

This folder contains the upload-ready visual set for the itch.io page and
nearby promotional use. All files are derived from the game's own runtime
screenshots and accepted in-repo visual direction. No new external source asset
or AI-generated image was added for this package.

## Primary Uploads

| File | Size | Use |
| --- | ---: | --- |
| `cover_630x500.png` | 630x500 | itch.io cover image |
| `screen_1_hub.png` | 960x540 | screenshot 1, opening hub |
| `screen_2_dialogue.png` | 960x540 | screenshot 2, quest/dialogue state |
| `screen_3_prefight.png` | 960x540 | screenshot 3, prefight/combat prep |
| `screen_4_locked_map.png` | 960x540 | screenshot 4, locked map/progression |

## Prepared Derivatives

| File | Size | Use |
| --- | ---: | --- |
| `prepared/promo_social_1200x630.png` | 1200x630 | social/link preview promo |
| `prepared/promo_keyart_1280x720.png` | 1280x720 | wide promo/key art |
| `prepared/page_header_960x300.png` | 960x300 | page header/decorative strip |
| `prepared/page_background_1600x900.png` | 1600x900 | dark page background/theme image |
| `prepared/thumbnail_315x250.png` | 315x250 | small preview thumbnail |
| `prepared/contact_sheet_1920x1080.png` | 1920x1080 | review sheet, not a page upload |

## Itch Page Styling Notes

- Use a dark page background color close to `#0c0a09`.
- Use warm accent colors around `#e6b15f` / `#f4d59a`.
- Keep the page background fixed or centered if itch.io settings allow it.
- Prefer the four 960x540 screenshots in the listed order: hub, dialogue,
  prefight, locked map.
- Treat `contact_sheet_1920x1080.png` as an internal review artifact only.

## Provenance

- Origin: in-repository game runtime screenshots and accepted `rb-dark-rpg`
  visual direction.
- License/provenance status: owned project output; no new third-party asset
  intake in this package.
- Generation status: no new AI image generation was used for the prepared
  derivative set.
- Integrity: `manifest.sha256` records the upload-set hashes.

## Publish Caveats

- `screen_4_locked_map.png` is valid as a progression screenshot, but it is the
  weakest promo image because it advertises a locked state. If a combat result,
  unlocked map, or finale screenshot exists before publication, prefer that as
  the fourth screenshot.
- The runtime pack
  `assets/packs/rb-dark-rpg-location-art-generated/` is currently marked
  `publish:false` and `redistribution_allowed:false`. Resolve that release
  policy before publishing screenshots that prominently feature those generated
  location backgrounds.
