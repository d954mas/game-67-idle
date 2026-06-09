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

const queue = JSON.parse(await text('data/asset_generation_queue.json'));
const brief = await text('asset_generation_brief.md');
const readme = await text('README.md');
const server = await text('server.mjs');

expect(queue.version, 'asset_generation_queue missing version');
expect(queue.rules?.temporaryOutputDir === 'tmp/', 'queue must keep temporary output in tmp/');
expect(queue.rules?.childSafe === true, 'queue must be childSafe');
expect(Array.isArray(queue.batches) && queue.batches.length >= 4, 'queue needs at least 4 batches');

const batchIds = new Set(queue.batches.map((batch) => batch.id));
for (const id of ['ui_slice9_p0', 'characters_p0', 'backgrounds_p0', 'mockups_p0']) {
  expect(batchIds.has(id), `queue missing batch ${id}`);
}

const itemIds = new Set();
for (const batch of queue.batches) {
  expect(batch.outputDir?.startsWith('gamedesing/'), `${batch.id} outputDir must stay in gamedesing`);
  expect(Array.isArray(batch.items) && batch.items.length > 0, `${batch.id} needs items`);
  for (const item of batch.items) {
    expect(item.id, `${batch.id} has item without id`);
    expect(item.targetFile?.endsWith('.png') || item.targetFile?.endsWith('.webp'), `${item.id} targetFile must be png/webp`);
    expect(item.prompt && item.prompt.length > 40, `${item.id} needs useful prompt`);
    expect(!item.prompt.toLowerCase().includes('adult crime'), `${item.id} prompt should avoid adult crime phrase as desired content`);
    itemIds.add(item.id);
  }
}

for (const id of ['ui_button_primary_9s', 'ui_panel_dark_9s', 'hero_base_1', 'district_meme_kiosk', 'mock_02_main_first_screen']) {
  expect(itemIds.has(id), `queue missing item ${id}`);
}

expect(brief.includes('asset_generation_queue.json') || readme.includes('asset_generation_queue.json'), 'queue is not linked from brief/readme');
expect(server.includes("['data/asset_generation_queue.json'"), 'editor whitelist missing data/asset_generation_queue.json');

console.log(JSON.stringify({
  version: queue.version,
  checks: {
    batches: queue.batches.length,
    items: itemIds.size
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
