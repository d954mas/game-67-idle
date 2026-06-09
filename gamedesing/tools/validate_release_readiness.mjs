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

const release = await json('data/release_readiness.json');
const compliance = await text('compliance_checklist.md');
const gates = await text('playtest_acceptance_gates.md');
const runtime = await text('runtime_test_plan.md');
const readme = await text('README.md');
const server = await text('server.mjs');

for (const sourceId of ['ftc_coppa_rule', 'ftc_coppa_faq', 'google_play_families', 'apple_app_review']) {
  const source = release.sourceRefs?.find((item) => item.id === sourceId);
  expect(source?.url?.startsWith('https://'), `release_readiness missing source ${sourceId}`);
}

for (const modeId of ['internal_web_supervised', 'mobile_store_internal_test', 'public_web_link']) {
  const mode = release.releaseModes?.find((item) => item.id === modeId);
  expect(mode, `releaseModes missing ${modeId}`);
  expect(Array.isArray(mode?.requiredBeforeInvite) && mode.requiredBeforeInvite.length > 0, `${modeId} missing requiredBeforeInvite`);
  expect(Array.isArray(mode?.evidence) && mode.evidence.length > 0, `${modeId} missing evidence`);
}

const flagMap = new Map((release.implementationFlags ?? []).map((flag) => [flag.id, flag.requiredValue]));
for (const [flag, value] of [
  ['child_test_safe', true],
  ['analyticsEnabled', false],
  ['adsEnabled', false],
  ['iapEnabled', false],
  ['accountEnabled', false],
  ['chatEnabled', false],
  ['freeTextEnabled', false],
  ['externalLinksEnabled', false]
]) {
  expect(flagMap.has(flag), `implementationFlags missing ${flag}`);
  expect(flagMap.get(flag) === value, `${flag} must be ${value}`);
}

for (const evidence of release.requiredEvidenceFiles ?? []) {
  expect(['tmp', 'gamedesing'].includes(evidence.store), `${evidence.id} has invalid store ${evidence.store}`);
  expect(evidence.description, `${evidence.id} missing description`);
}

for (const noGo of [
  'analytics_sends_before_guardian_notice',
  'ads_iap_or_rewarded_video_present',
  'free_text_or_chat_enabled',
  'parent_note_missing',
  'reset_progress_missing',
  'build_cannot_reach_15_67'
]) {
  expect(release.noGoIf?.includes(noGo), `noGoIf missing ${noGo}`);
}

for (const doc of [
  ['compliance_checklist.md', compliance],
  ['playtest_acceptance_gates.md', gates],
  ['runtime_test_plan.md', runtime],
  ['README.md', readme]
]) {
  expect(doc[1].includes('data/release_readiness.json'), `${doc[0]} does not reference data/release_readiness.json`);
}

expect(server.includes("['data/release_readiness.json'"), 'editor whitelist missing data/release_readiness.json');
expect(server.includes("['tools/validate_release_readiness.mjs'"), 'editor whitelist missing validate_release_readiness.mjs');

console.log(JSON.stringify({
  version: release.version,
  checks: {
    sourceRefs: release.sourceRefs?.length ?? 0,
    releaseModes: release.releaseModes?.length ?? 0,
    implementationFlags: release.implementationFlags?.length ?? 0,
    noGoIf: release.noGoIf?.length ?? 0
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
