import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const balance = JSON.parse(await readFile(path.join(root, 'data/balance.json'), 'utf8'));
const vectors = JSON.parse(await readFile(path.join(root, 'data/reducer_test_vectors.json'), 'utf8'));
const errors = [];

function expect(condition, message) {
  if (!condition) errors.push(message);
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function baseState(overrides = {}) {
  return {
    ...clone(balance.initialState),
    ownedUpgrades: [],
    seenEvents: [],
    activeJob: null,
    analytics: [],
    blocked: false,
    lockedHint: null,
    ...clone(overrides),
  };
}

function hasRequirement(state, requirements = {}) {
  return Object.entries(requirements).every(([key, value]) => {
    if (key === 'ownedTransport') return state.ownedTransport.includes(value);
    if (key === 'upgrade') return state.ownedUpgrades.includes(value);
    return (state[key] ?? 0) >= value;
  });
}

function firstLockedHint(item, state) {
  for (const entry of item.lockedHints ?? []) {
    const value = item.requirements?.[entry.target] ?? item.unlock?.[entry.target];
    if (entry.target === 'ownedTransport' && !state.ownedTransport.includes(value)) return entry.text;
    if ((state[entry.target] ?? 0) < value) return entry.text;
  }
  return null;
}

function pushAnalytics(state, name) {
  if (!state.analytics.includes(name)) state.analytics.push(name);
}

function applyEffects(state, effects = []) {
  const beforeStatus = state.status;
  for (const effect of effects) {
    if (effect.type === 'add') state[effect.target] = (state[effect.target] ?? 0) + effect.value;
    if (effect.type === 'set') state[effect.target] = effect.value;
    if (effect.type === 'setMax') state[effect.target] = Math.max(state[effect.target] ?? 0, effect.value);
    if (effect.type === 'addFromResource') {
      state[effect.target] = (state[effect.target] ?? 0) + (state[effect.resource] ?? 0) * effect.multiplier;
    }
    if (effect.type === 'unlock' && effect.target === 'activity' && !state.unlockedActivities.includes(effect.id)) {
      state.unlockedActivities.push(effect.id);
    }
    if (effect.type === 'triggerEvent') {
      // Event presentation is tested through explicit chooseEvent vectors.
      state.pendingEvent = effect.id;
    }
  }
  state.status = Math.min(state.status, balance.statusRange.mvpCap);
  state.powerLabel = `${state.status}/67`;
  if (state.status !== beforeStatus) pushAnalytics(state, 'status_changed');
  if (state.status >= balance.statusRange.mvpTarget && state.finalReady) pushAnalytics(state, 'final_15_reached');
}

function action(state, input) {
  if (input.type === 'do67') {
    const item = balance.activities.find((entry) => entry.id === 'do_67');
    applyEffects(state, item.effects.filter((effect) => effect.type !== 'chanceAdd'));
    pushAnalytics(state, 'activity_used');
    return;
  }

  if (input.type === 'buyUpgrade') {
    const item = balance.upgrades.find((entry) => entry.id === input.upgradeId);
    state.memeCoins -= item.cost;
    state.ownedUpgrades.push(item.id);
    applyEffects(state, item.effects);
    pushAnalytics(state, 'upgrade_bought');
    return;
  }

  if (input.type === 'buyTransport') {
    const item = balance.transport.find((entry) => entry.id === input.transportId);
    state.memeCoins -= item.cost;
    state.ownedTransport.push(item.id);
    applyEffects(state, item.effects);
    return;
  }

  if (input.type === 'startJob') {
    const item = balance.jobs.find((entry) => entry.id === input.jobId);
    if (!hasRequirement(state, item.requirements)) {
      state.blocked = true;
      state.lockedHint = firstLockedHint(item, state);
      return;
    }
    state.activeJob = item.id;
    pushAnalytics(state, 'job_started');
    return;
  }

  if (input.type === 'claimJob') {
    const item = balance.jobs.find((entry) => entry.id === input.jobId);
    if (state.activeJob !== item.id) state.activeJob = item.id;
    applyEffects(state, item.effects);
    state.activeJob = null;
    pushAnalytics(state, 'job_completed');
    return;
  }

  if (input.type === 'chooseEvent') {
    const event = balance.events.find((entry) => entry.id === input.eventId);
    const choice = event.choices.find((entry) => entry.id === input.choiceId);
    if (!state.seenEvents.includes(event.id)) state.seenEvents.push(event.id);
    applyEffects(state, choice.effects);
    pushAnalytics(state, 'event_choice');
    return;
  }

  throw new Error(`Unknown action ${input.type}`);
}

function assertExpected(vector, state) {
  const expected = vector.expect ?? {};
  for (const [key, value] of Object.entries(expected)) {
    if (key === 'ownedUpgradesIncludes') {
      for (const id of value) expect(state.ownedUpgrades.includes(id), `${vector.id}: ownedUpgrades missing ${id}`);
    } else if (key === 'ownedTransportIncludes') {
      for (const id of value) expect(state.ownedTransport.includes(id), `${vector.id}: ownedTransport missing ${id}`);
    } else if (key === 'seenEventsIncludes') {
      for (const id of value) expect(state.seenEvents.includes(id), `${vector.id}: seenEvents missing ${id}`);
    } else if (key === 'analytics') {
      for (const id of value) expect(state.analytics.includes(id), `${vector.id}: analytics missing ${id}`);
    } else {
      expect(JSON.stringify(state[key]) === JSON.stringify(value), `${vector.id}: ${key} expected ${JSON.stringify(value)} got ${JSON.stringify(state[key])}`);
    }
  }
}

expect(vectors.balanceVersion === balance.version, `test vectors target balance ${vectors.balanceVersion}, current ${balance.version}`);
expect(Array.isArray(vectors.vectors) && vectors.vectors.length >= 8, 'expected at least 8 reducer vectors');

for (const vector of vectors.vectors ?? []) {
  const state = baseState(vector.given);
  for (const step of vector.actions ?? [vector.action]) action(state, step);
  assertExpected(vector, state);
}

console.log(JSON.stringify({
  version: vectors.version,
  checks: {
    vectors: vectors.vectors?.length ?? 0,
  },
  errors,
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
