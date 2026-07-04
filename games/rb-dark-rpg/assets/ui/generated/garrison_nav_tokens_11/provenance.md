# Bottom Nav Tokens 11 Provenance

Date: 2026-07-04

Asset: `bottom_nav_component_sheet_11.png`

Origin: AI generated raster art, built-in Codex image generation.

Purpose: source sheet for `rb-dark-rpg` bottom navigation buttons.

Status: artlead approved for runtime integration; production slices derived locally.

Prompt summary:

- Five equal-size bottom navigation button tiles for Equipment, Journal, Map,
  Inspect Here, and More.
- Same dark blocky Roblox-like fantasy UI family as v10.
- No baked labels; each tile includes an empty lower trough for runtime text.
- Inspect Here icon changed to only one standalone magnifying glass, with no
  stone tiles or props underneath.
- Lens readability requested through transparent tint and highlights, not
  through objects inside the lens.
- More icon remains three physical brass dots.
- Flat chroma green source background for later canvas/alpha cleanup.

Accepted generation source:

`C:\Users\ROG\.codex\generated_images\019f2c62-4c25-7b41-8b29-75c27712dc18\ig_0193374d0c654bdb016a490e4e0df08191bb7c65aad6829120.png`

Workspace copy:

`games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/bottom_nav_component_sheet_11.png`

SHA256:

`A45DEEE7070F9D95B070B2A26B86CA6A3FB4C006B3E5EB87F0267857946EC35C`

Canvas source of truth:

- Project: `canvas://rb-dark-rpg-9874a1`
- Group: `grp_7c4025a4` / `UI direction - standalone glass inspect nav tokens 11`
- Image element: `el_7f7482f8`

Runtime derivatives:

- `slices/nav_v11_equipment.png`
- `slices/nav_v11_journal.png`
- `slices/nav_v11_map.png`
- `slices/nav_v11_place.png`
- `slices/nav_v11_more.png`
- `slices/manifest.json`

Slice method:

- Source rects came from `atlas_manifest.json`.
- Green chroma background was converted to alpha.
- A follow-up edge cleanup removed green antialias fringe pixels.
- Minor source width differences are normalized at runtime by using equal
  button layout boxes/hitboxes.

Runtime notes:

- Labels, selected/pressed states, hitboxes, and compact bottom-sheet hooks are
  owned by runtime UI.
- The source sheet has no baked text in the runtime labels.
- V3/V4/V5/V6/V7/V10 remain historical references, not production sources.
