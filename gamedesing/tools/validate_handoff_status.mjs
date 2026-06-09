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

const status = await text('handoff_status.md');
const readme = await text('README.md');
const gates = await text('playtest_acceptance_gates.md');
const runtime = await text('runtime_test_plan.md');
const server = await text('server.mjs');

for (const section of [
  'Что Уже Доказано',
  'Что Еще Не Доказано',
  'Next Implementation Step',
  'Handoff Rule',
  'Current No-Go'
]) {
  expect(status.includes(section), `handoff_status.md missing section ${section}`);
}

for (const proof of [
  'validate_all.mjs',
  'data/balance.json',
  'data/reducer_test_vectors.json',
  'data/ui_flow.json',
  'data/analytics_events.json',
  'data/release_readiness.json',
  'data/risk_register.json',
  'data/playtest_observation_schema.json'
]) {
  expect(status.includes(proof), `handoff_status.md missing proof/source ${proof}`);
}

for (const missing of [
  'runtime build',
  'real screenshots',
  'runtime QA evidence',
  'playtest observation rows'
]) {
  expect(status.includes(missing), `handoff_status.md missing no-go item ${missing}`);
}

for (const command of [
  'npm run test',
  'npm run build',
  'node gamedesing/tools/validate_all.mjs'
]) {
  expect(status.includes(command), `handoff_status.md missing command ${command}`);
}

expect(readme.includes('handoff_status.md'), 'README missing handoff_status.md');
expect(gates.includes('handoff_status.md'), 'playtest_acceptance_gates.md missing handoff_status.md');
expect(runtime.includes('validate_handoff_status.mjs'), 'runtime_test_plan missing validate_handoff_status.mjs');
expect(server.includes("['handoff_status.md'"), 'editor whitelist missing handoff_status.md');
expect(server.includes("['tools/validate_handoff_status.mjs'"), 'editor whitelist missing validate_handoff_status.mjs');

console.log(JSON.stringify({
  checks: {
    sections: 5,
    proofSources: 8,
    noGoItems: 4
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
