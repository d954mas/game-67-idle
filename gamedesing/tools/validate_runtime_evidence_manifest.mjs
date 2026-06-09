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

const manifest = await json('data/runtime_evidence_manifest.json');
const status = await text('handoff_status.md');
const runtime = await text('runtime_test_plan.md');
const gates = await text('playtest_acceptance_gates.md');
const release = await json('data/release_readiness.json');
const readme = await text('README.md');
const server = await text('server.mjs');

const evidence = manifest.evidence ?? [];
const evidenceIds = new Set(evidence.map((item) => item.id));
const canonicalFiles = manifest.canonicalFiles ?? [];

expect(manifest.storageRules?.temporaryEvidenceDir === 'tmp', 'temporaryEvidenceDir must be tmp');
expect(manifest.storageRules?.finalFindingsDir === 'gamedesing', 'finalFindingsDir must be gamedesing');
expect(manifest.storageRules?.doNotCommitTemporaryEvidence === true, 'temporary evidence must not be committed');

for (const id of [
  'validator_output',
  'runtime_build_log',
  'runtime_test_log',
  'desktop_portrait_screenshot',
  'mobile_360_screenshot',
  'mobile_390_screenshot',
  'tap_target_audit',
  'analytics_disabled_audit',
  'manual_30min_route_notes',
  'parent_invite_copy',
  'playtest_observation_rows',
  'playtest_review_round'
]) {
  expect(evidenceIds.has(id), `runtime evidence missing ${id}`);
}

for (const file of [
  'tmp/build_validation_YYYYMMDD.log',
  'tmp/runtime_qa_YYYYMMDD.log',
  'tmp/viewport_evidence_YYYYMMDD.md',
  'tmp/route_30min_rehearsal_YYYYMMDD.md',
  'tmp/playtest_observations_round_01.jsonl'
]) {
  const entry = canonicalFiles.find((item) => item.pathPattern === file);
  expect(entry, `canonicalFiles missing ${file}`);
  for (const evidenceId of entry?.containsEvidenceIds ?? []) {
    expect(evidenceIds.has(evidenceId), `${file} references missing evidence ${evidenceId}`);
  }
}

for (const item of evidence) {
  expect(['tmp', 'gamedesing'].includes(item.store), `${item.id} has invalid store ${item.store}`);
  expect(item.requiredBefore, `${item.id} missing requiredBefore`);
  expect(Array.isArray(item.proves) && item.proves.length > 0, `${item.id} missing proves`);
  expect(item.expectedResult, `${item.id} missing expectedResult`);
}

for (const coverage of manifest.handoffNoGoCoverage ?? []) {
  expect(status.includes(coverage.noGo), `handoff_status.md does not mention no-go ${coverage.noGo}`);
  for (const id of coverage.evidenceIds ?? []) {
    expect(evidenceIds.has(id), `${coverage.noGo} references missing evidence ${id}`);
  }
}

for (const requiredEvidence of release.requiredEvidenceFiles ?? []) {
  if (requiredEvidence.id === 'screenshots') {
    expect(evidenceIds.has('desktop_portrait_screenshot'), 'release screenshots missing desktop evidence');
    expect(evidenceIds.has('mobile_360_screenshot'), 'release screenshots missing mobile 360 evidence');
    expect(evidenceIds.has('mobile_390_screenshot'), 'release screenshots missing mobile 390 evidence');
    continue;
  }
  if (requiredEvidence.id === 'runtime_log') {
    expect(evidenceIds.has('runtime_test_log'), 'release runtime_log missing runtime_test_log evidence');
    continue;
  }
  if (requiredEvidence.id === 'playtest_findings') {
    expect(evidenceIds.has('playtest_review_round'), 'release playtest_findings missing playtest_review_round evidence');
    continue;
  }
  if (requiredEvidence.id === 'playtest_observation_log') {
    expect(evidenceIds.has('playtest_observation_rows'), 'release playtest_observation_log missing observation rows evidence');
    continue;
  }
  expect(evidenceIds.has(requiredEvidence.id), `release evidence ${requiredEvidence.id} missing in runtime manifest`);
}

expect(readme.includes('data/runtime_evidence_manifest.json'), 'README missing data/runtime_evidence_manifest.json');
expect(runtime.includes('validate_runtime_evidence_manifest.mjs'), 'runtime_test_plan missing validate_runtime_evidence_manifest.mjs');
expect(gates.includes('data/runtime_evidence_manifest.json'), 'playtest_acceptance_gates.md missing data/runtime_evidence_manifest.json');
expect(server.includes("['data/runtime_evidence_manifest.json'"), 'editor whitelist missing data/runtime_evidence_manifest.json');
expect(server.includes("['tools/validate_runtime_evidence_manifest.mjs'"), 'editor whitelist missing validate_runtime_evidence_manifest.mjs');

console.log(JSON.stringify({
  version: manifest.version,
  checks: {
    evidence: evidence.length,
    canonicalFiles: canonicalFiles.length,
    noGoCoverage: manifest.handoffNoGoCoverage?.length ?? 0
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
