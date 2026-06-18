# Mine Cards Art Inventory

## Local Source Folder

`C:\Users\ROG\YandexDisk\gamedev\assets\my\Mine Cards`

Do not treat this folder as repo-owned runtime assets. Copying, flattening,
packing, license cleanup, and provenance checks are separate asset-pipeline
tasks.

Current observed contents are PSD screen comps and old card/UI screens, not
ready 3D models, rigs, or animation clips. Use them as visual/reference input
for Mine Cards identity, item silhouettes, UI mood, and fake shots.

## Contact Sheet

Temporary preview generated during import:

`tmp/mine_cards_contact_sheet.png`

## Readable PSDs In The Import Preview

- `screen_1/screen_1_final_x2.psd`
- `screen_2_equipments_x2/screen_2_equipments_x2.psd`
- `screen_3_skills_x2/screen_3_skills_x2.psd`
- `screen_battle_card_open/screen_battle_card_open.psd`
- `screen_battle_choose_card/screen_battle_choose_card.psd`
- `screen_battle_monsters/screen_battle_monsters.psd`
- `screen_battle_success_failure/screen_battle_success_failure.psd`
- `screen_battle_tab_cards/screen_battle_tab_cards.psd`
- `screen_character_equipment.psd`
- `screen_loading.psd`
- `screen_WALLS.psd`

## PSDs Not Readable By Pillow In This Import

- `screen_chest_close/screen_chest_close.psd`
- `screen_chest_open/screen_chest_open.psd`
- `screen_defeat/screen_defeat.psd`
- `screen_exit_location.psd`
- `screen_level_up/screen_level_up.psd`
- `screen_not_enough_food.psd`
- `screen_win.psd`

## Runtime Asset Risks

- The preview hero is strongly Minecraft/Steve-adjacent. Before shipping or
  using in a final public build, create a legally safer original voxel hero.
- PSDs are screen comps, not engine-ready atlases.
- The saved equipment sheet
  `art/candidates/mine-cards-equipment-source-sheet-shadow-problem-v001.png`
  is reference/probe material only, not accepted runtime source art. Its white
  background conflicts with metal highlights, gutters are too tight, and baked
  shadows require regeneration or a separate shadow layer before production
  cutting. Review:
  `reviews/equipment_source_shadow_cutout_review_2026-06-18.md`.
- Production equipment generation is tracked by
  `art_requests/mine-cards-equipment-source-v001.json`. The current prompt
  packet now routes to flat `#00ff00` or true transparency after a blue-key
  candidate failed intake. Accepted draft production source:
  `art/candidates/mine-cards-equipment-source-v001-candidate-b-resheet.png`.
  Source intake, semantic/style audit, runtime crop plan, 12 runtime PNGs,
  contact sheet, pixel audit, and strict art-job validation pass. Runtime
  sprites live in `assets/runtime/mine-cards-equipment-source-v001/`. This is
  ready for lead visual acceptance before later inventory/equipment integration;
  it is not wired into the native Mining first screen yet.
- Future 3D character assets need mesh provenance, scale/origin checks, and
  attachment/pivot rules; generated raster art is not a runtime 3D model.
  Custom miner production prep now lives in
  `visual/custom_voxel_miner_source_packet_v001.md` and is tracked by T0010.
  It stays gated until T0001 is accepted or the lead explicitly prioritizes
  character production.
- Some UI uses small pixel-font labels; runtime text must pass the repository
  readability gate on zoomed crops.
- The old UI includes gems and food, but the first slice should avoid reviving
  those currencies until the core loop needs them.

## First Runtime Asset Set

Chosen first source path:

`visual/runtime_asset_plan_v001.md`

The first native proof may use project-owned procedural/blockout GLB parts. This
is a runtime integration scaffold, not final generated/artist art.

Minimum first-slice runtime set:

- original modular 3D voxel miner body/head/arms;
- original voxel hero idle transform animation;
- original voxel hero mining transform animation;
- worn pickaxe mesh;
- copper pickaxe mesh;
- cozy mine/workshop background;
- stone node mesh or sprite;
- copper vein node mesh or sprite;
- one locked/deeper node plate;
- pickaxe icon and copper pickaxe upgrade icon;
- stone, copper ore, coin, XP, mastery, and geode icons;
- progress bar UI pieces;
- reward log row UI pieces;
- node list UI pieces;
- upgrade panel UI pieces;
- minimal navigation icons for Mining, Upgrades, Inventory.

Locked v0.01 3D subset:

- `miner_body_blockout`;
- `miner_head_blockout`;
- `miner_left_arm_blockout`;
- `miner_right_arm_blockout`;
- `pickaxe_worn_blockout`;
- `pickaxe_copper_blockout`;
- `node_surface_stone_blockout`;
- `node_copper_vein_blockout`.

## Skeletal Spike Assets

Current local proof:

- `visual/skeletal_spike/minecards_skeletal_miner_probe.glb`
- `visual/skeletal_spike/minecards_skeletal_miner_probe.blend`
- `visual/skeletal_spike/minecards_skeletal_miner_probe_preview.png`
- `visual/skeletal_spike/minecards_skeletal_miner_probe_manifest.json`

These are technical proof assets, not final runtime art. They prove Blender can
produce a rigged/animated GLB for Mine Cards. The sidecar inspector
`tools/assets/inspect_skeletal_glb.mjs` also proves that the project can parse
the GLB, sample one pose, read 8 inverse bind matrices, and locate attachment
nodes such as `pickaxe`. The native `mine_cards_skeletal_glb_probe` target can
also parse the GLB via `cgltf` and see the same skin/joint/animation metadata.
They do not prove native clip playback, native gear rendering, skinning, or
final visual quality.

External candidate matrix:

`data/asset_candidates.json`

Deferred archived set:

- closed card back;
- weak spider monster card;
- combat HP/attack icons;
- sword and armor upgrade icons;
- victory/defeat/chest modals.
