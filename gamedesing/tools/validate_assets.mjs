import { readFile } from 'node:fs/promises';
import { stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const balance = JSON.parse(await readFile(path.join(root, 'data/balance.json'), 'utf8'));
const manifest = JSON.parse(await readFile(path.join(root, 'data/asset_manifest.json'), 'utf8'));
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

function ids(items = []) {
  return new Set(items.map((item) => item.id));
}

async function exists(relativePath) {
  return Boolean(await stat(path.join(root, relativePath)).catch(() => null));
}

const manifestStages = ids(manifest.visualStages);
const manifestCompanions = ids(manifest.companions);
const manifestAnimations = ids(manifest.animations);
const manifestDistricts = ids(manifest.districtBackgrounds);
const manifestFakeShots = ids(manifest.fakeShots);

const balanceStages = new Set([
  balance.initialState.visualStage,
  ...balance.housing.map((item) => item.visualStage),
  ...balance.transport.map((item) => item.visualStage),
  ...balance.upgrades.flatMap((item) => item.effects ?? [])
    .filter((effect) => effect.type === 'set' && effect.target === 'visualStage')
    .map((effect) => effect.value),
]);

for (const stage of balanceStages) {
  expect(manifestStages.has(stage), `asset_manifest missing visualStage ${stage}`);
}

const companions = new Set([
  ...balance.upgrades.flatMap((item) => item.effects ?? [])
    .filter((effect) => effect.type === 'set' && effect.target === 'screenCompanion')
    .map((effect) => effect.value),
].filter(Boolean));

for (const companion of companions) {
  expect(manifestCompanions.has(companion), `asset_manifest missing companion ${companion}`);
}

const animations = new Set([
  ...balance.activities.map((item) => item.animation),
  ...balance.microReactions.map((item) => item.animation),
].filter(Boolean));

for (const animation of animations) {
  expect(manifestAnimations.has(animation), `asset_manifest missing animation ${animation}`);
}

for (const district of balance.cityDistricts) {
  expect(manifestDistricts.has(district.id), `asset_manifest missing district background ${district.id}`);
}

for (const shot of [
  'first_screen',
  'first_click',
  'first_purchase',
  'status_up',
  'city_map',
  'deals_timer',
  'event_modal',
  'home_growth',
  'mini_final',
]) {
  expect(manifestFakeShots.has(shot), `asset_manifest missing fake shot ${shot}`);
}

for (const section of ['hero', 'npc', 'icons']) {
  expect(Array.isArray(manifest[section]) && manifest[section].length > 0, `asset_manifest missing section ${section}`);
}

for (const item of manifest.gddSiteArt ?? []) {
  expect(await exists(item.path), `gddSiteArt file does not exist: ${item.path}`);
}

for (const id of [
  'generated_comeback_keyart',
  'generated_gameplay_fakeshot',
  'generated_life_sim_progression',
  'generated_asset_sheet',
]) {
  expect(ids(manifest.gddSiteArt).has(id), `asset_manifest missing generated GDD art ${id}`);
}

console.log(JSON.stringify({
  version: manifest.version,
  checks: {
    visualStages: balanceStages.size,
    companions: companions.size,
    animations: animations.size,
    districtBackgrounds: balance.cityDistricts.length,
    fakeShots: manifest.fakeShots?.length ?? 0,
  },
  errors,
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
