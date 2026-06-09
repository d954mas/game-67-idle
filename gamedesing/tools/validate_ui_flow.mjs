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

const ui = await readJson('data/ui_flow.json');
const balance = await readJson('data/balance.json');
const analytics = await readJson('data/analytics_events.json');
const mobileSpec = await readText('mobile_web_ux_spec.md');
const handoff = await readText('prototype_build_handoff.md');
const gates = await readText('playtest_acceptance_gates.md');
const server = await readText('server.mjs');

const screenIds = new Set((ui.screens ?? []).map((screen) => screen.id));
const analyticsIds = new Set((analytics.events ?? []).map((event) => event.id));
const collections = new Set([
  'activities',
  'cityDistricts',
  'jobs',
  'training',
  'housing',
  'transport',
  'upgrades',
  'events'
]);

for (const target of ['360x640', '390x844', 'desktop_portrait_frame']) {
  expect(ui.layoutTargets?.includes(target), `layoutTargets missing ${target}`);
}

expect(ui.globalRules?.primaryActionMinHeightPx >= 72, 'primary action must be at least 72px');
expect(ui.globalRules?.bottomTabMinHeightPx >= 56, 'bottom tabs must be at least 56px');
expect(ui.globalRules?.cardTapTargetMinPx >= 48, 'card tap target must be at least 48px');
expect(ui.globalRules?.noHoverOnlyInteractions === true, 'hover-only interactions must be disabled');

const tabs = ui.tabs ?? [];
expect(tabs.length === 4, `expected 4 P0 tabs, got ${tabs.length}`);
for (const label of ['Город', 'Дела', 'Улучшения', 'Дом']) {
  expect(tabs.some((tab) => tab.label === label), `tabs missing ${label}`);
  expect(mobileSpec.includes(label), `mobile_web_ux_spec.md does not mention tab ${label}`);
}
for (const tab of tabs) {
  expect(screenIds.has(tab.screenId), `tab ${tab.id} points to missing screen ${tab.screenId}`);
}

for (const flow of ui.flow ?? []) {
  expect(flow.from === 'any' || screenIds.has(flow.from), `flow from missing screen ${flow.from}`);
  expect(screenIds.has(flow.to), `flow to missing screen ${flow.to}`);
  expect(flow.trigger, `flow ${flow.from}->${flow.to} missing trigger`);
}

for (const requiredScreen of ['intro', 'main', 'city', 'deals', 'upgrades', 'home', 'event_modal', 'offline_modal', 'mini_final']) {
  expect(screenIds.has(requiredScreen), `screens missing ${requiredScreen}`);
}

for (const screen of ui.screens ?? []) {
  expect(Array.isArray(screen.requiredComponents) && screen.requiredComponents.length > 0, `${screen.id} missing requiredComponents`);
  expect(Array.isArray(screen.acceptance) && screen.acceptance.length > 0, `${screen.id} missing acceptance`);
  for (const collection of screen.rendersBalanceCollections ?? []) {
    expect(collections.has(collection), `${screen.id} references unknown collection ${collection}`);
    expect(Array.isArray(balance[collection]), `${screen.id} collection ${collection} is not in balance.json`);
  }
  for (const action of screen.playerActions ?? []) {
    if (action.analyticsEvent) {
      expect(analyticsIds.has(action.analyticsEvent), `${screen.id}.${action.id} references missing analytics event ${action.analyticsEvent}`);
    }
    if (action.balanceActivityId) {
      expect(balance.activities.some((activity) => activity.id === action.balanceActivityId), `${screen.id}.${action.id} references missing activity ${action.balanceActivityId}`);
    }
  }
}

const screenshotIds = new Set();
for (const screen of ui.screens ?? []) {
  if (screen.screenshotId) screenshotIds.add(screen.screenshotId);
  for (const state of screen.states ?? []) {
    if (state.screenshotId) screenshotIds.add(state.screenshotId);
    expect(state.id, `${screen.id} has state without id`);
    expect(state.trigger, `${screen.id}.${state.id} missing trigger`);
    expect(
      Array.isArray(state.requiredComponents) && state.requiredComponents.length > 0,
      `${screen.id}.${state.id} missing requiredComponents`
    );
  }
}
for (const screenshotId of ui.requiredScreenshotIds ?? []) {
  expect(screenshotIds.has(screenshotId), `required screenshot ${screenshotId} is not attached to a screen`);
}

expect(handoff.includes('data/ui_flow.json'), 'prototype_build_handoff.md does not reference data/ui_flow.json');
expect(gates.includes('data/ui_flow.json'), 'playtest_acceptance_gates.md does not reference data/ui_flow.json');
expect(server.includes("['data/ui_flow.json'"), 'editor whitelist missing data/ui_flow.json');
expect(server.includes("['tools/validate_ui_flow.mjs'"), 'editor whitelist missing validate_ui_flow.mjs');

console.log(JSON.stringify({
  version: ui.version,
  checks: {
    screens: ui.screens?.length ?? 0,
    tabs: tabs.length,
    flows: ui.flow?.length ?? 0,
    requiredScreenshots: ui.requiredScreenshotIds?.length ?? 0
  },
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
