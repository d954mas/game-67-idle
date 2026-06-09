import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

async function json(relPath) {
  return JSON.parse(await readFile(path.join(root, relPath), 'utf8'));
}

async function text(relPath) {
  return readFile(path.join(root, relPath), 'utf8');
}

const schema = await json('data/playtest_observation_schema.json');
const script = await text('playtest_script.md');
const runtime = await text('runtime_test_plan.md');
const gates = await text('playtest_acceptance_gates.md');
const release = await json('data/release_readiness.json');
const readme = await text('README.md');
const server = await text('server.mjs');

const fields = schema.fields ?? [];
const fieldIds = new Set(fields.map((field) => field.id));

for (const field of [
  'session_id',
  'build',
  'device',
  'age_group',
  'guardian_present',
  'analytics_enabled',
  'first_click_seconds',
  'first_clicked_element',
  'first_purchase_seconds',
  'first_status_up_seconds',
  'confusion_count',
  'reached_status',
  'final_reached',
  'understood_67',
  'noticed_67_gesture',
  'used_deals',
  'parent_concern_category',
  'best_reaction_category',
  'fix_next_category'
]) {
  expect(fieldIds.has(field), `schema missing field ${field}`);
}

for (const field of fields) {
  expect(field.type, `${field.id} missing type`);
  if (field.type === 'enum') {
    expect(Array.isArray(field.values) && field.values.length > 0, `${field.id} enum missing values`);
  }
  if (field.required) {
    expect(field.required === true, `${field.id} required must be boolean true`);
  }
}

for (const forbidden of [
  'child_name',
  'exact_age',
  'email',
  'phone',
  'geolocation',
  'photo',
  'audio',
  'video',
  'free_text_from_child'
]) {
  expect(schema.privacy?.doNotCollect?.includes(forbidden), `privacy.doNotCollect missing ${forbidden}`);
  expect(!fieldIds.has(forbidden), `forbidden field appears in schema fields: ${forbidden}`);
}

for (const metric of [
  'first_click_seconds',
  'first_purchase_seconds',
  'first_status_up_seconds',
  'confusion_count',
  'reached_status',
  'final_reached',
  'parent_concern'
]) {
  expect(script.includes(metric), `playtest_script.md does not mention ${metric}`);
}

for (const flag of [
  'first_click_late',
  'first_purchase_late',
  'first_status_up_late',
  'high_confusion',
  'core_meme_not_read'
]) {
  expect(schema.derivedFlags?.some((item) => item.id === flag), `derivedFlags missing ${flag}`);
}

expect(schema.rollupTargets?.length >= 5, 'expected at least 5 rollupTargets');
expect(release.requiredEvidenceFiles?.some((item) => item.id === 'playtest_observation_log'), 'release_readiness missing playtest_observation_log evidence');
expect(readme.includes('data/playtest_observation_schema.json'), 'README missing data/playtest_observation_schema.json');
expect(runtime.includes('validate_playtest_observation_schema.mjs'), 'runtime_test_plan missing validate_playtest_observation_schema.mjs');
expect(gates.includes('data/playtest_observation_schema.json'), 'playtest_acceptance_gates.md missing data/playtest_observation_schema.json');
expect(server.includes("['data/playtest_observation_schema.json'"), 'editor whitelist missing data/playtest_observation_schema.json');
expect(server.includes("['tools/validate_playtest_observation_schema.mjs'"), 'editor whitelist missing validate_playtest_observation_schema.mjs');

console.log(JSON.stringify({
  version: schema.version,
  checks: {
    fields: fields.length,
    forbiddenFields: schema.privacy?.doNotCollect?.length ?? 0,
    derivedFlags: schema.derivedFlags?.length ?? 0,
    rollupTargets: schema.rollupTargets?.length ?? 0
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
