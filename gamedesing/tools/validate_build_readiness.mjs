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

async function text(relPath) {
  return readFile(path.join(root, relPath), 'utf8');
}

const requiredFiles = [
  'prototype_mvp_spec.md',
  'dev_design_handoff_plan.md',
  'prototype_build_handoff.md',
  'prototype_technical_blueprint.md',
  'game_implementation_plan.md',
  'ui_bible.md',
  'screen_mockups_spec.md',
  'asset_generation_brief.md',
  'handoff_status.md',
  'implementation_backlog.md',
  'runtime_test_plan.md',
  'playtest_acceptance_gates.md',
  'playtest_script.md',
  'visual_contract.md',
  'p0_ui_copy.md',
  'fakeshots.html',
  'data/balance.json',
  'data/reducer_test_vectors.json',
  'data/asset_manifest.json',
  'data/analytics_events.json',
  'data/ui_flow.json',
  'data/ui_components.json',
  'data/asset_generation_queue.json',
  'data/release_readiness.json',
  'data/implementation_tasks.json',
  'data/risk_register.json',
  'data/playtest_observation_schema.json',
  'data/runtime_evidence_manifest.json',
];

for (const file of requiredFiles) {
  expect(await exists(file), `missing required build-readiness file: ${file}`);
}

const backlog = await text('implementation_backlog.md');
for (const phase of [
  'Phase 0. Project Shell',
  'Phase 1. Core State And Reducer',
  'Phase 2. Main Screen',
  'Phase 3. Tabs',
  'Phase 4. Events And Mini-Final',
  'Phase 5. Save, Offline, Analytics',
  'Phase 6. Visual QA',
]) {
  expect(backlog.includes(phase), `implementation_backlog missing ${phase}`);
}

const runtime = await text('runtime_test_plan.md');
for (const section of [
  'Smoke Test',
  'First 5 Minutes',
  'Full 30-Minute Route',
  'Save And Offline',
  'Analytics Safe Mode',
  'Viewport QA',
  'Parent/Guardian Test Readiness',
]) {
  expect(runtime.includes(section), `runtime_test_plan missing ${section}`);
}

const gates = await text('playtest_acceptance_gates.md');
expect(gates.includes('implementation_backlog.md'), 'gates do not reference implementation_backlog.md');
expect(gates.includes('runtime_test_plan.md'), 'gates do not reference runtime_test_plan.md');
expect(gates.includes('playtest_script.md'), 'gates do not reference playtest_script.md');

const readme = await text('README.md');
expect(readme.includes('implementation_backlog.md'), 'README missing implementation_backlog.md');
expect(readme.includes('game_implementation_plan.md'), 'README missing game_implementation_plan.md');
expect(readme.includes('dev_design_handoff_plan.md'), 'README missing dev_design_handoff_plan.md');
expect(readme.includes('ui_bible.md'), 'README missing ui_bible.md');
expect(readme.includes('screen_mockups_spec.md'), 'README missing screen_mockups_spec.md');
expect(readme.includes('asset_generation_brief.md'), 'README missing asset_generation_brief.md');
expect(readme.includes('asset_generation_queue.json'), 'README missing asset_generation_queue.json');
expect(readme.includes('runtime_test_plan.md'), 'README missing runtime_test_plan.md');
expect(readme.includes('playtest_script.md'), 'README missing playtest_script.md');
expect(readme.includes('validate_all.mjs'), 'README missing validate_all.mjs');

const server = await text('server.mjs');
expect(server.includes("['implementation_backlog.md'"), 'editor whitelist missing implementation_backlog.md');
expect(server.includes("['dev_design_handoff_plan.md'"), 'editor whitelist missing dev_design_handoff_plan.md');
expect(server.includes("['prototype_technical_blueprint.md'"), 'editor whitelist missing prototype_technical_blueprint.md');
expect(server.includes("['game_implementation_plan.md'"), 'editor whitelist missing game_implementation_plan.md');
expect(server.includes("['ui_bible.md'"), 'editor whitelist missing ui_bible.md');
expect(server.includes("['screen_mockups_spec.md'"), 'editor whitelist missing screen_mockups_spec.md');
expect(server.includes("['asset_generation_brief.md'"), 'editor whitelist missing asset_generation_brief.md');
expect(server.includes("['handoff_status.md'"), 'editor whitelist missing handoff_status.md');
expect(server.includes("['runtime_test_plan.md'"), 'editor whitelist missing runtime_test_plan.md');
expect(server.includes("['playtest_script.md'"), 'editor whitelist missing playtest_script.md');
expect(server.includes("['p0_ui_copy.md'"), 'editor whitelist missing p0_ui_copy.md');
expect(server.includes("['common/concept_map.md'"), 'editor whitelist missing common/concept_map.md');
expect(server.includes("['data/reducer_test_vectors.json'"), 'editor whitelist missing data/reducer_test_vectors.json');
expect(server.includes("['tools/validate_reducer_vectors.mjs'"), 'editor whitelist missing validate_reducer_vectors.mjs');
expect(server.includes("['data/asset_manifest.json'"), 'editor whitelist missing data/asset_manifest.json');
expect(server.includes("['tools/validate_assets.mjs'"), 'editor whitelist missing validate_assets.mjs');
expect(server.includes("['data/analytics_events.json'"), 'editor whitelist missing data/analytics_events.json');
expect(server.includes("['tools/validate_analytics.mjs'"), 'editor whitelist missing validate_analytics.mjs');
expect(server.includes("['data/ui_flow.json'"), 'editor whitelist missing data/ui_flow.json');
expect(server.includes("['data/ui_components.json'"), 'editor whitelist missing data/ui_components.json');
expect(server.includes("['data/asset_generation_queue.json'"), 'editor whitelist missing data/asset_generation_queue.json');
expect(server.includes("['tools/validate_ui_components.mjs'"), 'editor whitelist missing validate_ui_components.mjs');
expect(server.includes("['tools/validate_asset_generation_queue.mjs'"), 'editor whitelist missing validate_asset_generation_queue.mjs');
expect(server.includes("['tools/validate_ui_flow.mjs'"), 'editor whitelist missing validate_ui_flow.mjs');
expect(server.includes("['tools/validate_technical_blueprint.mjs'"), 'editor whitelist missing validate_technical_blueprint.mjs');
expect(server.includes("['data/release_readiness.json'"), 'editor whitelist missing data/release_readiness.json');
expect(server.includes("['tools/validate_release_readiness.mjs'"), 'editor whitelist missing validate_release_readiness.mjs');
expect(server.includes("['data/implementation_tasks.json'"), 'editor whitelist missing data/implementation_tasks.json');
expect(server.includes("['tools/validate_implementation_tasks.mjs'"), 'editor whitelist missing validate_implementation_tasks.mjs');
expect(server.includes("['data/risk_register.json'"), 'editor whitelist missing data/risk_register.json');
expect(server.includes("['tools/validate_risk_register.mjs'"), 'editor whitelist missing validate_risk_register.mjs');
expect(server.includes("['data/playtest_observation_schema.json'"), 'editor whitelist missing data/playtest_observation_schema.json');
expect(server.includes("['tools/validate_playtest_observation_schema.mjs'"), 'editor whitelist missing validate_playtest_observation_schema.mjs');
expect(server.includes("['tools/validate_handoff_status.mjs'"), 'editor whitelist missing validate_handoff_status.mjs');
expect(server.includes("['data/runtime_evidence_manifest.json'"), 'editor whitelist missing data/runtime_evidence_manifest.json');
expect(server.includes("['tools/validate_runtime_evidence_manifest.mjs'"), 'editor whitelist missing validate_runtime_evidence_manifest.mjs');
expect(server.includes("['tools/validate_all.mjs'"), 'editor whitelist missing validate_all.mjs');

const openQuestions = await text('open_questions.md');
expect(!openQuestions.includes('Собрать screen-by-screen fake shots'), 'open_questions still lists completed fake shots task');
expect(!openQuestions.includes('Отрендерить visual playtest board'), 'open_questions still lists completed visual board task');
expect(!openQuestions.includes('final player-facing copy'), 'open_questions still lists completed final copy task');

console.log(JSON.stringify({
  checks: {
    requiredFiles: requiredFiles.length,
    backlogPhases: 7,
    runtimeSections: 7,
  },
  errors,
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
