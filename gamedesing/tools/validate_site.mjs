import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const html = await readFile(path.join(root, 'index.html'), 'utf8');
const fakeshotsHtml = await readFile(path.join(root, 'fakeshots.html'), 'utf8');
const css = await readFile(path.join(root, 'site.css'), 'utf8');
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

const requiredShotTexts = [
  'Первый экран',
  'Первый клик',
  'Первая покупка',
  'Status-up',
  'Город',
  'Дело в таймере',
  'Событие',
  'Дом растет',
  'Mini-final',
  'Собери 5 мем-коинов',
  'Сделать 67',
  'Кепка 67',
  'Мечта: 67/67',
];

expect(html.includes('id="fakeshots"'), 'missing #fakeshots section');
expect((html.match(/<article class="game-shot phone-shot">/g) ?? []).length === 9, 'expected 9 fake shot cards');
expect(fakeshotsHtml.includes('id="fakeshots"'), 'fakeshots.html missing #fakeshots section');
expect((fakeshotsHtml.match(/<article class="game-shot phone-shot">/g) ?? []).length === 9, 'fakeshots.html expected 9 fake shot cards');

for (const text of requiredShotTexts) {
  expect(html.includes(text), `fake shots missing text: ${text}`);
  expect(fakeshotsHtml.includes(text), `fakeshots.html missing text: ${text}`);
}

for (const selector of ['.shot-grid--board', '.phone-shot', '.next-goal', '.gesture-hands', '.active-timer']) {
  expect(css.includes(selector), `site.css missing selector ${selector}`);
}

const forbiddenPublicTerms = [
  'Поднять силу',
  'Начать смену',
  'ходит на работу',
  'вкладке `Мемы`',
];

for (const term of forbiddenPublicTerms) {
  expect(!html.includes(term), `public site contains forbidden term: ${term}`);
  expect(!fakeshotsHtml.includes(term), `fakeshots.html contains forbidden term: ${term}`);
}

console.log(JSON.stringify({
  checks: {
    fakeShots: (html.match(/<article class="game-shot phone-shot">/g) ?? []).length,
    fakeshotsPageCards: (fakeshotsHtml.match(/<article class="game-shot phone-shot">/g) ?? []).length,
    requiredTexts: requiredShotTexts.length,
  },
  errors,
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
