// Node half of the record-or-compare golden bank. Shares the bank format with
// the C helper so a contract test and a native test can watch the same value.
//
// Compare mode (default): returns the recorded value; the caller asserts.
// Record mode (GAME_UPDATE_GOLDENS=1): stores the actual value and returns it.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

function bankPath(bank, env) {
  const dir = env.GAME_GOLDENS_DIR && env.GAME_GOLDENS_DIR.length > 0 ? env.GAME_GOLDENS_DIR : "tests/goldens";
  return join(dir, `${bank}.golden`);
}

function recording(env) {
  const flag = env.GAME_UPDATE_GOLDENS;
  return Boolean(flag) && flag !== "0";
}

export function readBank(bank, env = process.env) {
  const path = bankPath(bank, env);
  const entries = new Map();
  if (!existsSync(path)) return entries;
  for (const line of readFileSync(path, "utf8").split(/\r?\n/)) {
    const separator = line.indexOf("=");
    if (separator < 0) continue;
    const key = line.slice(0, separator).trim();
    if (!key || key.startsWith("#")) continue;
    entries.set(key, line.slice(separator + 1).trim());
  }
  return entries;
}

export function writeBank(bank, entries, env = process.env) {
  const path = bankPath(bank, env);
  mkdirSync(dirname(path), { recursive: true });
  const lines = [...entries.entries()]
    .sort((a, b) => (a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0))
    .map(([key, value]) => `${key} = ${value}`);
  writeFileSync(path, `${lines.join("\n")}\n`, "utf8");
}

// Returns the value the test should compare against: the recorded one, or the
// actual one when recording. Text must stay on one line to keep the bank format
// readable by both halves of the feature.
export function golden(bank, key, actual, env = process.env) {
  const rendered = typeof actual === "number" ? formatNumber(actual) : String(actual);
  if (/[\r\n]/.test(rendered)) throw new Error(`test-goldens: ${bank}/${key} must be one line`);
  const entries = readBank(bank, env);
  if (recording(env)) {
    entries.set(key, rendered);
    writeBank(bank, entries, env);
    return typeof actual === "number" ? Number(rendered) : rendered;
  }
  if (!entries.has(key)) {
    throw new Error(`test-goldens: no recorded value for '${bank}/${key}'. Record it with: node tools/game.mjs test --update-goldens`);
  }
  const recorded = entries.get(key);
  return typeof actual === "number" ? Number(recorded) : recorded;
}

function formatNumber(value) {
  return Number.isInteger(value) ? String(value) : String(Number(value.toPrecision(9)));
}
