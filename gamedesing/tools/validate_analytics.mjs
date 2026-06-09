import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

async function readJson(relPath) {
  return JSON.parse(await readFile(path.join(root, relPath), 'utf8'));
}

async function readText(relPath) {
  return readFile(path.join(root, relPath), 'utf8');
}

const analytics = await readJson('data/analytics_events.json');
const balance = await readJson('data/balance.json');
const spec = await readText('analytics_spec.md');
const handoff = await readText('prototype_build_handoff.md');
const gates = await readText('playtest_acceptance_gates.md');

const commonParams = new Set((analytics.commonParams ?? []).map((param) => param.id));
for (const param of ['session_id', 'build', 'platform', 'session_seconds', 'analytics_enabled']) {
  expect(commonParams.has(param), `commonParams missing ${param}`);
}

const eventIds = new Set((analytics.events ?? []).map((event) => event.id));
for (const eventId of balance.requiredAnalyticsEvents ?? []) {
  const event = (analytics.events ?? []).find((item) => item.id === eventId);
  expect(eventIds.has(eventId), `analytics_events missing required balance event ${eventId}`);
  expect(event?.requiredForP0 === true, `${eventId} must be requiredForP0`);
}

for (const event of analytics.events ?? []) {
  expect(spec.includes(`\`${event.id}\``), `analytics_spec.md does not mention ${event.id}`);
  expect(event.when, `${event.id} missing when`);
  for (const param of event.params ?? []) {
    expect(param.id, `${event.id} has param without id`);
    expect(param.type, `${event.id}.${param.id} missing type`);
    if (param.type === 'enum') {
      expect(Array.isArray(param.values) && param.values.length > 0, `${event.id}.${param.id} enum has no values`);
    }
  }
}

for (const forbidden of analytics.mode?.forbiddenPayloadFields ?? []) {
  expect(!commonParams.has(forbidden), `forbidden field appears in commonParams: ${forbidden}`);
  for (const event of analytics.events ?? []) {
    for (const param of event.params ?? []) {
      expect(param.id !== forbidden, `forbidden field appears in ${event.id}: ${forbidden}`);
    }
  }
}

expect(
  analytics.mode?.defaultExternalChildTest === 'analytics_off',
  'defaultExternalChildTest must be analytics_off'
);
expect(
  analytics.mode?.enabledOnlyAfter === 'guardian_notice_and_consent',
  'analytics must be enabled only after guardian notice and consent'
);
expect(handoff.includes('data/analytics_events.json'), 'handoff does not reference data/analytics_events.json');
expect(gates.includes('data/analytics_events.json'), 'playtest gates do not reference data/analytics_events.json');

console.log(JSON.stringify({
  version: analytics.version,
  checks: {
    events: analytics.events?.length ?? 0,
    requiredForP0: (analytics.events ?? []).filter((event) => event.requiredForP0).length,
    commonParams: commonParams.size,
    forbiddenPayloadFields: analytics.mode?.forbiddenPayloadFields?.length ?? 0
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
