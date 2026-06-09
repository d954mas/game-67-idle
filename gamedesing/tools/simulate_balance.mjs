import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const balancePath = path.resolve(__dirname, '../data/balance.json');
const balance = JSON.parse(await readFile(balancePath, 'utf8'));

const requiredCounts = {
  upgrades: 15,
  events: 4,
  districts: 4,
  jobs: 4,
};

function cloneState(source) {
  return JSON.parse(JSON.stringify(source));
}

function hasRequirement(state, requirements = {}) {
  return Object.entries(requirements).every(([key, value]) => {
    if (key === 'ownedTransport') return state.ownedTransport.includes(value);
    if (key === 'upgrade') return state.ownedUpgrades.includes(value);
    if (typeof value === 'boolean') return Boolean(state[key]) === value;
    return (state[key] ?? 0) >= value;
  });
}

function applyEffects(state, effects = [], eventLog = [], choiceIndex = 0) {
  for (const effect of effects) {
    if (effect.type === 'add') {
      state[effect.target] = (state[effect.target] ?? 0) + effect.value;
    }
    if (effect.type === 'set') {
      state[effect.target] = effect.value;
    }
    if (effect.type === 'setMax') {
      state[effect.target] = Math.max(state[effect.target] ?? 0, effect.value);
    }
    if (effect.type === 'unlock' && effect.target === 'activity' && !state.unlockedActivities.includes(effect.id)) {
      state.unlockedActivities.push(effect.id);
    }
    if (effect.type === 'triggerEvent') {
      const event = balance.events.find((item) => item.id === effect.id);
      if (event && !state.seenEvents.includes(event.id)) {
        state.seenEvents.push(event.id);
        const choice = event.choices[Math.min(choiceIndex, event.choices.length - 1)];
        eventLog.push({ eventId: event.id, choiceId: choice?.id ?? null });
        applyEffects(state, choice?.effects ?? [], eventLog, choiceIndex);
      }
    }
  }

  state.status = Math.min(state.status, balance.statusRange.mvpCap);
  state.powerLabel = `${state.status}/67`;
}

function buy(state, item, type, eventLog, choiceIndex) {
  state.memeCoins -= item.cost;
  if (type === 'upgrade') state.ownedUpgrades.push(item.id);
  if (type === 'housing') state.ownedHousing.push(item.id);
  if (type === 'transport') state.ownedTransport.push(item.id);
  applyEffects(state, item.effects, eventLog, choiceIndex);
}

function bestAvailableJob(state) {
  return [...balance.jobs]
    .filter((job) => hasRequirement(state, job.requirements))
    .sort((a, b) => {
      const rewardA = a.effects.find((effect) => effect.target === 'memeCoins')?.value ?? 0;
      const rewardB = b.effects.find((effect) => effect.target === 'memeCoins')?.value ?? 0;
      return rewardB / b.durationSeconds - rewardA / a.durationSeconds;
    })[0];
}

function simulateScenario(scenario) {
  const errors = [];
  const state = cloneState(balance.initialState);
  state.ownedUpgrades = [];
  state.seenEvents = [];

  const eventLog = [];
  const purchaseLog = [];
  let nextJobAt = scenario.firstJobAtSeconds ?? 20;

  const candidates = [
    ...balance.upgrades.map((item) => ({ ...item, type: 'upgrade' })),
    ...balance.housing.filter((item) => item.cost > 0).map((item) => ({ ...item, type: 'housing' })),
    ...balance.transport.filter((item) => !item.postMvp).map((item) => ({ ...item, type: 'transport' })),
  ];

  function alreadyOwned(item) {
    if (item.type === 'upgrade') return state.ownedUpgrades.includes(item.id);
    if (item.type === 'housing') return state.ownedHousing.includes(item.id);
    if (item.type === 'transport') return state.ownedTransport.includes(item.id);
    return false;
  }

  for (let second = 1; second <= scenario.durationMinutes * 60; second += 1) {
    state.memeCoins += state.incomePerSecond;
    state.memeCoins += state.clickPower * scenario.clickRatePerSecond;

    if (second >= nextJobAt) {
      const job = bestAvailableJob(state);
      if (job) {
        applyEffects(state, job.effects, eventLog, scenario.choiceIndex);
        nextJobAt = second + job.durationSeconds + scenario.jobCooldownSeconds;
      } else {
        nextJobAt = second + scenario.jobCooldownSeconds;
      }
    }

    for (const item of candidates) {
      if (alreadyOwned(item)) continue;
      if (!hasRequirement(state, item.unlock)) continue;
      if (state.memeCoins < item.cost) continue;
      buy(state, item, item.type, eventLog, scenario.choiceIndex);
      purchaseLog.push({
        minute: Number((second / 60).toFixed(1)),
        type: item.type,
        id: item.id,
        status: state.status,
        incomePerSecond: state.incomePerSecond,
        clickPower: state.clickPower,
      });
    }

    if (state.status >= scenario.targetStatus) break;
  }

  if (state.status < scenario.targetStatus) {
    errors.push(`target ${scenario.targetStatus}/67 not reached: got ${state.status}/67`);
  }

  if (purchaseLog.length === 0 || purchaseLog[0].minute > 0.5) {
    errors.push(`first purchase too slow: ${purchaseLog[0]?.minute ?? 'never'} min`);
  }

  const firstStatusUp = purchaseLog.find((entry) => entry.status >= 2);
  const targetSeconds = balance.economyRules.firstStatusUpTargetSeconds;
  if (scenario.enforceFirstStatusTarget !== false && (!firstStatusUp || firstStatusUp.minute * 60 > targetSeconds)) {
    errors.push(`first status up too slow: ${firstStatusUp?.minute ?? 'never'} min`);
  }

  if (scenario.requireAllEvents && eventLog.length < balance.events.length) {
    errors.push(`events seen ${eventLog.length}, expected ${balance.events.length}`);
  }

  return {
    name: scenario.name,
    reached: `${state.status}/67`,
    memeCoins: Math.floor(state.memeCoins),
    incomePerSecond: state.incomePerSecond,
    clickPower: state.clickPower,
    purchases: purchaseLog.length,
    firstPurchaseMinute: purchaseLog[0]?.minute ?? null,
    firstStatusUpMinute: firstStatusUp?.minute ?? null,
    events: eventLog,
    errors,
  };
}

const countErrors = [];
for (const [key, expected] of Object.entries(requiredCounts)) {
  const actual = balance[key === 'districts' ? 'cityDistricts' : key]?.length ?? 0;
  if (actual < expected) countErrors.push(`${key}: expected at least ${expected}, got ${actual}`);
}

const scenarios = [
  {
    name: 'active_first_choices',
    durationMinutes: balance.prototypeMinutes,
    targetStatus: balance.statusRange.mvpTarget,
    clickRatePerSecond: 0.65,
    jobCooldownSeconds: 20,
    choiceIndex: 0,
    requireAllEvents: true,
    enforceFirstStatusTarget: true,
  },
  {
    name: 'active_second_choices',
    durationMinutes: balance.prototypeMinutes,
    targetStatus: balance.statusRange.mvpTarget,
    clickRatePerSecond: 0.65,
    jobCooldownSeconds: 20,
    choiceIndex: 1,
    requireAllEvents: true,
    enforceFirstStatusTarget: true,
  },
  {
    name: 'low_engagement_first_choices',
    durationMinutes: balance.prototypeMinutes,
    targetStatus: 10,
    clickRatePerSecond: 0.35,
    jobCooldownSeconds: 55,
    firstJobAtSeconds: 45,
    choiceIndex: 0,
    requireAllEvents: false,
    enforceFirstStatusTarget: false,
  },
  {
    name: 'low_engagement_second_choices',
    durationMinutes: balance.prototypeMinutes,
    targetStatus: 10,
    clickRatePerSecond: 0.35,
    jobCooldownSeconds: 55,
    firstJobAtSeconds: 45,
    choiceIndex: 1,
    requireAllEvents: false,
    enforceFirstStatusTarget: false,
  },
].map(simulateScenario);

const errors = [...countErrors, ...scenarios.flatMap((scenario) => scenario.errors.map((error) => `${scenario.name}: ${error}`))];
const primary = scenarios[0];

const report = {
  reached: primary.reached,
  memeCoins: primary.memeCoins,
  incomePerSecond: primary.incomePerSecond,
  clickPower: primary.clickPower,
  purchases: primary.purchases,
  firstPurchaseMinute: primary.firstPurchaseMinute,
  firstStatusUpMinute: primary.firstStatusUpMinute,
  events: primary.events.map((entry) => entry.eventId),
  scenarios,
  errors,
};

console.log(JSON.stringify(report, null, 2));
if (errors.length > 0) process.exitCode = 1;
