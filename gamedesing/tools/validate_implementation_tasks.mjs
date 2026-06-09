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

const matrix = await json('data/implementation_tasks.json');
const backlog = await text('implementation_backlog.md');
const readme = await text('README.md');
const runtime = await text('runtime_test_plan.md');
const server = await text('server.mjs');
const toolsDir = path.join(root, 'tools');

const tasks = matrix.tasks ?? [];
const taskIds = new Set(tasks.map((task) => task.id));
expect(tasks.length === 8, `expected 8 implementation tasks, got ${tasks.length}`);

for (const phase of [0, 1, 2, 3, 4, 5, 6, 7]) {
  expect(tasks.some((task) => task.phase === phase), `missing task phase ${phase}`);
}

for (const task of tasks) {
  expect(task.id, 'task without id');
  expect(task.title, `${task.id} missing title`);
  expect(Array.isArray(task.deliverables) && task.deliverables.length > 0, `${task.id} missing deliverables`);
  expect(Array.isArray(task.acceptance) && task.acceptance.length > 0, `${task.id} missing acceptance`);
  for (const dep of task.dependsOn ?? []) {
    expect(taskIds.has(dep), `${task.id} depends on missing task ${dep}`);
  }
  for (const doc of task.sourceDocs ?? []) {
    expect(await exists(doc), `${task.id} references missing doc ${doc}`);
  }
  for (const validator of task.validators ?? []) {
    const file = validator === 'simulate_balance' ? 'simulate_balance.mjs' : `${validator}.mjs`;
    expect(await exists(path.join('tools', file)), `${task.id} references missing validator ${file}`);
  }
}

for (const phaseTitle of [
  'Phase 0. Project Shell',
  'Phase 1. Core State And Reducer',
  'Phase 2. Main Screen',
  'Phase 3. Tabs',
  'Phase 4. Events And Mini-Final',
  'Phase 5. Save, Offline, Analytics',
  'Phase 6. Visual QA'
]) {
  expect(backlog.includes(phaseTitle), `implementation_backlog missing ${phaseTitle}`);
}

expect(readme.includes('data/implementation_tasks.json'), 'README missing data/implementation_tasks.json');
expect(runtime.includes('validate_implementation_tasks.mjs'), 'runtime_test_plan missing validate_implementation_tasks.mjs');
expect(server.includes("['data/implementation_tasks.json'"), 'editor whitelist missing data/implementation_tasks.json');
expect(server.includes("['tools/validate_implementation_tasks.mjs'"), 'editor whitelist missing validate_implementation_tasks.mjs');

console.log(JSON.stringify({
  version: matrix.version,
  checks: {
    tasks: tasks.length,
    phases: 8,
    validatorsReferenced: new Set(tasks.flatMap((task) => task.validators ?? [])).size
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
