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

const blueprint = await text('prototype_technical_blueprint.md');
const handoff = await text('prototype_build_handoff.md');
const backlog = await text('implementation_backlog.md');
const runtime = await text('runtime_test_plan.md');
const readme = await text('README.md');
const server = await text('server.mjs');

for (const section of [
  'Recommended Stack',
  'Suggested File Map',
  'Data Import Rule',
  'Core Runtime Modules',
  'UI Runtime',
  'Save And Offline',
  'Analytics Stub',
  'Required Tests',
  'Build Cut Line',
  'First Implementation Order'
]) {
  expect(blueprint.includes(`## `) && blueprint.includes(section), `blueprint missing section ${section}`);
}

for (const contract of [
  'data/balance.json',
  'data/reducer_test_vectors.json',
  'data/ui_flow.json',
  'data/analytics_events.json',
  'data/asset_manifest.json'
]) {
  expect(blueprint.includes(contract), `blueprint missing contract ${contract}`);
}

for (const runtimeModule of [
  'state.ts',
  'reducer.ts',
  'effects.ts',
  'selectors.ts',
  'requirements.ts',
  'storage.ts',
  'offline.ts',
  'analytics.ts'
]) {
  expect(blueprint.includes(runtimeModule), `blueprint missing runtime module ${runtimeModule}`);
}

for (const command of ['npm run test', 'npm run build']) {
  expect(blueprint.includes(command), `blueprint missing command ${command}`);
}

for (const target of ['360x640', '390x844', 'desktop uses centered portrait frame']) {
  expect(blueprint.includes(target), `blueprint missing target ${target}`);
}

expect(blueprint.includes('analyticsEnabled=false'), 'blueprint must set analytics off by default');
expect(blueprint.includes('game67:p0:save'), 'blueprint missing save key');
expect(blueprint.includes('unknown effect/target throws in dev'), 'blueprint missing unknown effect dev failure');
expect(blueprint.includes('Do not build in P0'), 'blueprint missing P0 negative scope');

for (const doc of [
  ['prototype_build_handoff.md', handoff],
  ['implementation_backlog.md', backlog],
  ['runtime_test_plan.md', runtime],
  ['README.md', readme]
]) {
  expect(doc[1].includes('prototype_technical_blueprint.md'), `${doc[0]} does not reference prototype_technical_blueprint.md`);
}

expect(server.includes("['prototype_technical_blueprint.md'"), 'editor whitelist missing prototype_technical_blueprint.md');
expect(server.includes("['tools/validate_technical_blueprint.mjs'"), 'editor whitelist missing validate_technical_blueprint.mjs');

console.log(JSON.stringify({
  checks: {
    sections: 10,
    contracts: 5,
    modules: 8
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
