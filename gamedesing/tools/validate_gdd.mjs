import { readFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const balancePath = path.join(root, 'data/balance.json');
const balance = JSON.parse(await readFile(balancePath, 'utf8'));
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

function ids(items) {
  return new Set(items.map((item) => item.id));
}

const eventIds = ids(balance.events);
const jobIds = ids(balance.jobs);
const trainingIds = ids(balance.training);
const activityIds = ids(balance.activities);
const stateFields = new Set([
  ...Object.keys(balance.initialState),
  'ownedUpgrades',
  'seenEvents',
  'activeJob',
  'lastSaveTimeMs',
  'sessionStartTimeMs',
]);
const requiredAnalyticsEvents = [
  'session_start',
  'first_click',
  'activity_used',
  'upgrade_bought',
  'status_changed',
  'job_started',
  'job_completed',
  'event_seen',
  'event_choice',
  'final_15_reached',
  'session_end',
];

expect(balance.statusRange?.start === 1, 'statusRange.start must be 1');
expect(balance.statusRange?.mvpTarget === 15, 'statusRange.mvpTarget must be 15');
expect(balance.statusRange?.finalTarget === 67, 'statusRange.finalTarget must be 67');
expect(balance.upgrades.length === 15, `expected 15 upgrades, got ${balance.upgrades.length}`);
expect(balance.events.length === 4, `expected 4 events, got ${balance.events.length}`);
expect(balance.cityDistricts.length === 4, `expected 4 city districts, got ${balance.cityDistricts.length}`);
expect(balance.jobs.length === 4, `expected 4 jobs, got ${balance.jobs.length}`);
expect(balance.training.length === 4, `expected 4 training entries, got ${balance.training.length}`);
expect(balance.housing.length >= 4, `expected at least 4 housing entries, got ${balance.housing.length}`);
expect(balance.transport.length >= 3, `expected at least 3 transport entries, got ${balance.transport.length}`);
expect(Array.isArray(balance.microReactions) && balance.microReactions.length >= 6, 'expected at least 6 microReactions');
expect(Array.isArray(balance.nextGoalTemplates) && balance.nextGoalTemplates.length >= 6, 'expected at least 6 nextGoalTemplates');

for (const district of balance.cityDistricts) {
  for (const jobId of district.jobs ?? []) {
    expect(jobIds.has(jobId), `district ${district.id} references missing job ${jobId}`);
  }
  for (const trainingId of district.training ?? []) {
    expect(trainingIds.has(trainingId), `district ${district.id} references missing training ${trainingId}`);
  }
}

function validateEffects(owner, effects = []) {
  for (const effect of effects) {
    if (['add', 'set', 'setMax', 'addFromResource', 'chanceAdd'].includes(effect.type)) {
      expect(stateFields.has(effect.target), `${owner} writes unknown state target ${effect.target}`);
    }
    if (effect.type === 'addFromResource') {
      expect(stateFields.has(effect.resource), `${owner} reads unknown state resource ${effect.resource}`);
    }
    if (effect.type === 'triggerEvent') {
      expect(eventIds.has(effect.id), `${owner} triggers missing event ${effect.id}`);
    }
    if (effect.type === 'unlock' && effect.target === 'activity') {
      expect(activityIds.has(effect.id), `${owner} unlocks missing activity ${effect.id}`);
    }
  }
}

for (const item of [
  ...balance.activities,
  ...balance.jobs,
  ...balance.training,
  ...balance.housing,
  ...balance.transport,
  ...balance.upgrades,
]) {
  validateEffects(item.id, item.effects);
  expect(item.visibleResult?.text, `${item.id} missing visibleResult.text`);
}

for (const event of balance.events) {
  expect(event.visibleResult?.text, `${event.id} missing visibleResult.text`);
  for (const choice of event.choices ?? []) {
    validateEffects(`${event.id}.${choice.id}`, choice.effects);
    expect(choice.visibleResult?.text, `${event.id}.${choice.id} missing visibleResult.text`);
  }
}

const finalEvent = balance.events.find((event) => event.id === 'final_banana_scheme');
for (const choice of finalEvent?.choices ?? []) {
  const setsFinalReady = choice.effects?.some(
    (effect) => effect.type === 'set' && effect.target === 'finalReady' && effect.value === true
  );
  expect(setsFinalReady, `final_banana_scheme.${choice.id} must set finalReady true`);
}

for (const item of [...balance.jobs, ...balance.upgrades, ...balance.housing, ...balance.transport]) {
  const requirementKeys = Object.keys(item.requirements ?? item.unlock ?? {});
  if (requirementKeys.length > 1) {
    expect(Array.isArray(item.requirementPriority), `${item.id} has multiple requirements but no requirementPriority`);
    expect(Array.isArray(item.lockedHints), `${item.id} has multiple requirements but no lockedHints`);
  }
}

for (const item of balance.transport.filter((transport) => transport.postMvp)) {
  expect(item.unlock?.status >= balance.statusRange.mvpTarget, `${item.id} postMvp transport must unlock after MVP target`);
  const statusEffect = item.effects?.some((effect) => effect.type === 'add' && effect.target === 'status');
  expect(!statusEffect, `${item.id} postMvp transport must not increase status inside P0 cap`);
}

for (const eventName of requiredAnalyticsEvents) {
  expect(balance.requiredAnalyticsEvents?.includes(eventName), `requiredAnalyticsEvents missing ${eventName}`);
}

const forbiddenPlayerText = [
  'измена',
  'бомж',
  'бедность',
  'ночевка',
  'мусор',
  'долги',
  'штраф',
  'унижение',
  'месть',
  'кризис',
];

const playerText = JSON.stringify({
  activities: balance.activities,
  cityDistricts: balance.cityDistricts,
  jobs: balance.jobs,
  training: balance.training,
  housing: balance.housing,
  transport: balance.transport,
  upgrades: balance.upgrades,
  events: balance.events,
  reactionPool: balance.reactionPool,
}).toLowerCase();

for (const word of forbiddenPlayerText) {
  expect(!playerText.includes(word), `forbidden player-facing word in balance: ${word}`);
}

function runNodeScript(scriptPath) {
  return new Promise((resolve) => {
    const child = spawn(process.execPath, [scriptPath], { cwd: path.resolve(root, '..') });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => {
      stdout += chunk;
    });
    child.stderr.on('data', (chunk) => {
      stderr += chunk;
    });
    child.on('close', (code) => resolve({ code, stdout, stderr }));
  });
}

const sim = await runNodeScript(path.join(root, 'tools/simulate_balance.mjs'));
expect(sim.code === 0, `simulate_balance failed: ${sim.stderr || sim.stdout}`);
if (sim.code === 0) {
  const report = JSON.parse(sim.stdout);
  expect(report.reached === '15/67', `balance sim reached ${report.reached}, expected 15/67`);
  expect(report.events.length === 4, `balance sim saw ${report.events.length} events, expected 4`);
}

const report = {
  version: balance.version,
  checks: {
    upgrades: balance.upgrades.length,
    events: balance.events.length,
    districts: balance.cityDistricts.length,
    jobs: balance.jobs.length,
    training: balance.training.length,
  },
  errors,
};

console.log(JSON.stringify(report, null, 2));
if (errors.length > 0) process.exitCode = 1;
