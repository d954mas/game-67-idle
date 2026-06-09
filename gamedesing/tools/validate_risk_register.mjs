import { access, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

async function exists(relPath) {
  try {
    await access(path.join(root, relPath));
    return true;
  } catch {
    return false;
  }
}

async function json(relPath) {
  return JSON.parse(await readFile(path.join(root, relPath), 'utf8'));
}

async function text(relPath) {
  return readFile(path.join(root, relPath), 'utf8');
}

const register = await json('data/risk_register.json');
const readme = await text('README.md');
const gates = await text('playtest_acceptance_gates.md');
const runtime = await text('runtime_test_plan.md');
const server = await text('server.mjs');

const risks = register.risks ?? [];
const ids = new Set();
expect(risks.length >= 8, `expected at least 8 risks, got ${risks.length}`);

for (const risk of risks) {
  expect(risk.id, 'risk without id');
  expect(!ids.has(risk.id), `duplicate risk id ${risk.id}`);
  ids.add(risk.id);
  expect(['critical', 'high', 'medium', 'low'].includes(risk.severity), `${risk.id} invalid severity ${risk.severity}`);
  expect(risk.owner, `${risk.id} missing owner`);
  expect(risk.statement, `${risk.id} missing statement`);
  expect(Array.isArray(risk.detection) && risk.detection.length >= 2, `${risk.id} needs at least 2 detection signals`);
  expect(Array.isArray(risk.mitigation) && risk.mitigation.length >= 2, `${risk.id} needs at least 2 mitigations`);
  expect(Array.isArray(risk.sourceDocs) && risk.sourceDocs.length >= 2, `${risk.id} needs at least 2 source docs`);
  expect(Array.isArray(risk.gateRefs) && risk.gateRefs.length >= 1, `${risk.id} needs gateRefs`);
  for (const doc of risk.sourceDocs ?? []) {
    expect(await exists(doc), `${risk.id} references missing source doc ${doc}`);
  }
}

for (const required of [
  'r_67_not_understood',
  'r_67_gesture_not_readable',
  'r_deals_read_as_child_labor',
  'r_low_engagement_stalls',
  'r_locked_cards_confuse',
  'r_privacy_release_mode_misconfigured'
]) {
  expect(ids.has(required), `risk register missing ${required}`);
}

expect(readme.includes('data/risk_register.json'), 'README missing data/risk_register.json');
expect(gates.includes('data/risk_register.json'), 'playtest_acceptance_gates.md missing data/risk_register.json');
expect(runtime.includes('validate_risk_register.mjs'), 'runtime_test_plan missing validate_risk_register.mjs');
expect(server.includes("['data/risk_register.json'"), 'editor whitelist missing data/risk_register.json');
expect(server.includes("['tools/validate_risk_register.mjs'"), 'editor whitelist missing validate_risk_register.mjs');

console.log(JSON.stringify({
  version: register.version,
  checks: {
    risks: risks.length,
    critical: risks.filter((risk) => risk.severity === 'critical').length,
    high: risks.filter((risk) => risk.severity === 'high').length
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
