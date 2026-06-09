import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

async function text(relPath) {
  return readFile(path.join(root, relPath), 'utf8');
}

const ui = JSON.parse(await text('data/ui_components.json'));
const bible = await text('ui_bible.md');
const handoff = await text('dev_design_handoff_plan.md');
const mockups = await text('screen_mockups_spec.md');
const brief = await text('asset_generation_brief.md');
const server = await text('server.mjs');
const site = await text('index.html');

expect(ui.version, 'ui_components missing version');
expect(ui.rules?.allRepeatedUiIsComponent === true, 'ui_components must require component reuse');
expect(ui.rules?.stretchableFramesUseSlice9 === true, 'ui_components must require slice9');
expect(ui.rules?.minTapTargetPx >= 48, 'minTapTargetPx must be >= 48');
expect(ui.rules?.primaryActionMinHeightPx >= 72, 'primaryActionMinHeightPx must be >= 72');

for (const key of ['button', 'panel', 'card', 'modal', 'badge', 'pill', 'tab', 'progress']) {
  expect(ui.slice9?.[key], `slice9 missing ${key}`);
  expect(Array.isArray(ui.slice9?.[key]?.sourceSize), `slice9 ${key} missing sourceSize`);
  expect(Number.isFinite(ui.slice9?.[key]?.border), `slice9 ${key} missing border`);
}

const componentIds = new Set((ui.components ?? []).map((item) => item.id));
for (const id of ['PrimaryButton', 'Panel', 'Card', 'Modal', 'PowerBadge', 'ResourcePill', 'BottomTab', 'ProgressBar']) {
  expect(componentIds.has(id), `ui_components missing ${id}`);
  expect(bible.includes(id), `ui_bible missing ${id}`);
}

for (const screen of ['main', 'city', 'deals', 'upgrades', 'home', 'event_modal', 'offline_modal', 'mini_final']) {
  expect(Array.isArray(ui.screenComposition?.[screen]), `screenComposition missing ${screen}`);
}

for (const file of ['ui_bible.md', 'screen_mockups_spec.md', 'asset_generation_brief.md', 'dev_design_handoff_plan.md']) {
  expect(server.includes(`['${file}'`), `editor whitelist missing ${file}`);
}
expect(server.includes("['data/ui_components.json'"), 'editor whitelist missing data/ui_components.json');

for (const phrase of ['slice9', '9-slice', 'Сделать 67']) {
  expect(bible.includes(phrase) || handoff.includes(phrase), `handoff/ui bible missing phrase ${phrase}`);
}

for (const shot of ['intro_fall_67_to_1', 'main_first_screen', 'city_map', 'deals_timer', 'home_growth', 'mini_final']) {
  expect(mockups.includes(shot), `screen_mockups_spec missing ${shot}`);
}

for (const asset of ['ui_button_primary_9s', 'ui_panel_dark_9s', 'hero_base_1', 'district_meme_kiosk']) {
  expect(brief.includes(asset), `asset_generation_brief missing ${asset}`);
}

expect(site.includes('gdd-ui-kit-slice9-board.png'), 'site missing UI kit board');
expect(site.includes('ui_bible.md'), 'site missing ui_bible link');

console.log(JSON.stringify({
  version: ui.version,
  checks: {
    components: ui.components?.length ?? 0,
    slice9: Object.keys(ui.slice9 ?? {}).length,
    screenComposition: Object.keys(ui.screenComposition ?? {}).length
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
