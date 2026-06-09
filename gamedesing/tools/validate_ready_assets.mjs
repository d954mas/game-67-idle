import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

async function exists(relPath) {
  return Boolean(await stat(path.join(root, relPath)).catch(() => null));
}

async function pngSize(relPath) {
  const buffer = await readFile(path.join(root, relPath));
  const signature = buffer.subarray(0, 8).toString('hex');
  expect(signature === '89504e470d0a1a0a', `${relPath} is not a PNG`);
  return {
    width: buffer.readUInt32BE(16),
    height: buffer.readUInt32BE(20),
  };
}

const manifestPath = 'assets/asset_pack_manifest.json';
const runtimeManifestPath = 'assets/generated/runtime_asset_manifest.json';
const slice9Path = 'assets/ui/slice9.json';
expect(await exists(manifestPath), 'missing asset_pack_manifest.json');
expect(await exists(runtimeManifestPath), 'missing runtime_asset_manifest.json');
expect(await exists(slice9Path), 'missing ui/slice9.json');
expect(await exists('assets/asset_pack_preview.png'), 'missing asset_pack_preview.png');
expect(await exists('assets/generated/runtime_asset_pack_preview.png'), 'missing runtime_asset_pack_preview.png');
expect(await exists('assets/generated/runtime_composed_screen.png'), 'missing runtime_composed_screen.png');
expect(await exists('art_bible.html'), 'missing runtime art_bible.html');

const manifest = JSON.parse(await readFile(path.join(root, manifestPath), 'utf8'));
const runtimeManifest = JSON.parse(await readFile(path.join(root, runtimeManifestPath), 'utf8'));
const slice9 = JSON.parse(await readFile(path.join(root, slice9Path), 'utf8'));
const uiComponents = JSON.parse(await readFile(path.join(root, 'data/ui_components.json'), 'utf8'));

for (const category of ['ui', 'icons', 'characters', 'backgrounds', 'fx']) {
  expect(Array.isArray(manifest[category]) && manifest[category].length > 0, `manifest missing category ${category}`);
}

expect(manifest.storyAnchor?.includes('67/67'), 'manifest storyAnchor missing 67/67');
expect(manifest.storyAnchor?.includes('betrayed'), 'manifest storyAnchor missing betrayed');
expect(manifest.storyAnchor?.includes('1/67'), 'manifest storyAnchor missing 1/67');
expect(manifest.rules?.tempGenerationFolder === 'tmp/', 'manifest must keep temp generation in tmp/');
expect(manifest.rules?.finalAssetsFolder === 'gamedesing/assets/', 'manifest must keep final assets in gamedesing/assets/');

const requiredFiles = [
  'art/generated-67-comeback-keyart.png',
  'art/generated-67-gameplay-fakeshot.png',
  'art/generated-67-life-sim-progression.png',
  'art/generated-67-asset-sheet.png',
  'assets/generated/runtime_composed_screen.png',
  'assets/generated/runtime_asset_pack_preview.png',
  'assets/generated/characters/hero_1_67_body.png',
  'assets/generated/characters/hero_3_67_cap_body.png',
  'assets/generated/characters/hero_7_67_scooter_body.png',
  'assets/generated/characters/hero_15_67_leader_body.png',
  'assets/generated/characters/banana_rival.png',
  'assets/generated/characters/strawberry_secret.png',
  'assets/generated/ui/button_67_gesture.png',
  'assets/generated/ui/badge_power_1_67.png',
  'assets/generated/ui/card_job_kiosk.png',
  'assets/generated/ui/card_upgrade_tap.png',
  'assets/generated/backgrounds/bg_starter_room_yard.png',
  'assets/generated/backgrounds/bg_meme_kiosk_job.png',
  'assets/generated/backgrounds/bg_upgraded_home_business.png',
  'assets/ui/ui_button_primary_9s.png',
  'assets/ui/ui_button_secondary_9s.png',
  'assets/ui/ui_panel_dark_9s.png',
  'assets/ui/ui_card_default_9s.png',
  'assets/ui/ui_card_locked_9s.png',
  'assets/ui/ui_modal_9s.png',
  'assets/ui/ui_badge_power_9s.png',
  'assets/ui/ui_pill_resource_9s.png',
  'assets/ui/ui_tab_9s.png',
  'assets/ui/ui_progress_frame_9s.png',
  'assets/icons/meme_coin.png',
  'assets/icons/status_67.png',
  'assets/icons/click_power.png',
  'assets/icons/income_per_second.png',
  'assets/icons/hands_skill.png',
  'assets/icons/betrayal_banana.png',
  'assets/icons/betrayal_strawberry.png',
  'assets/characters/hero_base_1.png',
  'assets/characters/hero_cap_2.png',
  'assets/characters/hero_final_15.png',
  'assets/characters/banana_confused.png',
  'assets/characters/strawberry_secret.png',
  'assets/backgrounds/bg_safe_sleep.png',
  'assets/backgrounds/district_yard_1.png',
  'assets/backgrounds/district_meme_kiosk.png',
  'assets/backgrounds/district_school_yard.png',
  'assets/backgrounds/district_mini_business.png',
  'assets/fx/fx_badge_flash_67.png',
  'assets/fx/fx_coin_fly.png',
  'assets/fx/fx_map_ping.png',
  'assets/fx/fx_hands_67.png',
  'assets/fx/fx_betrayal_crack.png',
];

for (const file of requiredFiles) {
  expect(await exists(file), `missing ready asset ${file}`);
}

expect(runtimeManifest.styleReference === 'art/generated-67-gameplay-fakeshot.png', 'runtime manifest must point to generated gameplay fakeshot style reference');
for (const category of ['characters', 'heroBodyVariants', 'ui', 'backgrounds']) {
  expect(Array.isArray(runtimeManifest.runtimeCategories?.[category]) && runtimeManifest.runtimeCategories[category].length > 0, `runtime manifest missing ${category}`);
}

const expectedUi = new Map([
  ['ui_button_primary_9s.png', 'button'],
  ['ui_button_secondary_9s.png', 'button'],
  ['ui_panel_dark_9s.png', 'panel'],
  ['ui_card_default_9s.png', 'card'],
  ['ui_card_locked_9s.png', 'card'],
  ['ui_modal_9s.png', 'modal'],
  ['ui_badge_power_9s.png', 'badge'],
  ['ui_pill_resource_9s.png', 'pill'],
  ['ui_tab_9s.png', 'tab'],
  ['ui_progress_frame_9s.png', 'progress'],
]);

for (const [fileName, component] of expectedUi) {
  const entry = slice9.assets?.[fileName];
  const base = uiComponents.slice9?.[component];
  expect(Boolean(entry), `slice9 missing ${fileName}`);
  expect(entry?.component === component, `slice9 ${fileName} has wrong component`);
  expect(entry?.border === base?.border, `slice9 ${fileName} border mismatch`);
  expect(entry?.width === base?.sourceSize?.[0], `slice9 ${fileName} width mismatch`);
  expect(entry?.height === base?.sourceSize?.[1], `slice9 ${fileName} height mismatch`);
  const size = await pngSize(`assets/ui/${fileName}`);
  expect(size.width === entry?.width, `PNG width mismatch for ${fileName}`);
  expect(size.height === entry?.height, `PNG height mismatch for ${fileName}`);
}

for (const item of [
  ...manifest.icons,
  ...manifest.characters,
  ...manifest.backgrounds,
  ...manifest.fx,
  ...runtimeManifest.runtimeCategories.characters,
  ...runtimeManifest.runtimeCategories.heroBodyVariants,
  ...runtimeManifest.runtimeCategories.ui,
  ...runtimeManifest.runtimeCategories.backgrounds,
]) {
  const size = await pngSize(item.file);
  expect(size.width === item.width, `${item.file} width mismatch`);
  expect(size.height === item.height, `${item.file} height mismatch`);
}

const artBible = await readFile(path.join(root, 'art_bible.html'), 'utf8');
for (const reference of [
  'assets/generated/runtime_composed_screen.png',
  'assets/generated/characters/hero_1_67_body.png',
  'assets/generated/ui/button_67_gesture.png',
  'assets/generated/backgrounds/bg_starter_room_yard.png',
  'assets/generated/runtime_asset_manifest.json',
]) {
  expect(artBible.includes(reference), `art_bible.html missing ${reference}`);
}

console.log(JSON.stringify({
  version: manifest.version,
  checks: {
    requiredFiles: requiredFiles.length,
    uiSlice9: expectedUi.size,
    runtimeCharacters: runtimeManifest.runtimeCategories.characters.length,
    runtimeUi: runtimeManifest.runtimeCategories.ui.length,
    runtimeBackgrounds: runtimeManifest.runtimeCategories.backgrounds.length,
    categories: ['ui', 'icons', 'characters', 'backgrounds', 'fx'].length,
  },
  errors,
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
