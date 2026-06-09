import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const balance = JSON.parse(await readFile(path.join(root, 'data/balance.json'), 'utf8'));
const copy = await readFile(path.join(root, 'p0_ui_copy.md'), 'utf8');
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

const requiredIds = [
  ...balance.activities.map((item) => item.id),
  ...balance.cityDistricts.map((item) => item.id),
  ...balance.jobs.map((item) => item.id),
  ...balance.training.map((item) => item.id),
  ...balance.housing.map((item) => item.id),
  ...balance.transport.map((item) => item.id),
  ...balance.upgrades.map((item) => item.id),
  ...balance.events.map((item) => item.id),
  ...balance.events.flatMap((event) => event.choices.map((choice) => choice.id)),
  ...balance.nextGoalTemplates.map((item) => item.id),
  ...balance.microReactions.map((item) => item.id),
];

for (const id of requiredIds) {
  expect(copy.includes(`\`${id}\``), `p0_ui_copy missing id ${id}`);
}

for (const text of ['Сделать 67', 'Начать дело', 'Забрать награду', 'К делу', 'Мечта после 15/67']) {
  expect(copy.includes(text), `p0_ui_copy missing required copy "${text}"`);
}

const forbidden = [
  'Поднять силу',
  'Начать смену',
  'Смена',
  'Работа',
  'ходит на работу',
  'деньги',
  'измена',
  'бомж',
  'ночевка',
  'унижение',
  'месть',
];

for (const word of forbidden) {
  expect(!copy.toLowerCase().includes(word.toLowerCase()), `p0_ui_copy contains forbidden word: ${word}`);
}

console.log(JSON.stringify({
  checks: {
    requiredIds: requiredIds.length,
    forbiddenWords: forbidden.length,
  },
  errors,
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
