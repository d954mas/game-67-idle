import { existsSync, readdirSync, statSync } from "node:fs";
import { resolve } from "node:path";

export function localDate() {
  const now = new Date();
  const year = now.getFullYear();
  const month = String(now.getMonth() + 1).padStart(2, "0");
  const day = String(now.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

/* Per-session profile logs (written by the hook) live one file per session, so
 * parallel work -- different sessions, harnesses, or project cwds -- never mixes.
 * Layout mirrors hook_record_fast.c: sessions/<date>__<harness>__<sid8>.jsonl */
export function sessionsDir() {
  return resolve("tmp", "session_profiles", "sessions");
}

export function listSessionProfiles(dir = sessionsDir()) {
  if (!existsSync(dir)) return [];
  return readdirSync(dir)
    .filter((name) => name.endsWith(".jsonl"))
    .map((name) => resolve(dir, name));
}

export function todaySessionProfiles(dir = sessionsDir()) {
  const prefix = `${localDate()}__`;
  return listSessionProfiles(dir).filter((path) => path.split(/[\\/]/).pop().startsWith(prefix));
}

// Native hook logs and transcript readers must identify the same session even
// when the UUID is embedded in a Codex rollout filename.
export function deriveSessionId(raw) {
  const text = String(raw || "");
  const uuid = text.match(/[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}/);
  if (uuid) return { full: uuid[0], short: uuid[0].slice(0, 8) };
  const partial = text.match(/[0-9a-fA-F]{8}-[0-9a-fA-F]{4}/);
  if (partial) return { full: partial[0], short: partial[0].slice(0, 8) };
  return { full: "", short: "" };
}

/* The active session = the most-recently-written per-session log. */
export function latestSessionProfilePath() {
  const files = listSessionProfiles();
  let best = "";
  let bestMtime = -1;
  for (const file of files) {
    const mtime = statSync(file).mtimeMs;
    if (mtime > bestMtime) {
      best = file;
      bestMtime = mtime;
    }
  }
  return best;
}

export function parseArgs(argv) {
  const result = { values: {}, positionals: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--") {
      result.positionals.push(...argv.slice(index + 1));
      break;
    }
    if (!arg.startsWith("--")) {
      result.positionals.push(arg);
      continue;
    }
    const key = arg.slice(2);
    const next = argv[index + 1];
    if (next === undefined || next.startsWith("--")) {
      addArgValue(result.values, key, true);
      continue;
    }
    addArgValue(result.values, key, next);
    index += 1;
  }
  return result;
}

function addArgValue(values, key, value) {
  if (values[key] === undefined) {
    values[key] = value;
    return;
  }
  if (!Array.isArray(values[key])) values[key] = [values[key]];
  values[key].push(value);
}

export function listArg(values, key) {
  const value = values[key];
  if (value === undefined || value === true) return [];
  return Array.isArray(value) ? value : [value];
}

export function stringArg(values, key, fallback = "") {
  const value = values[key];
  if (value === undefined || value === true) return fallback;
  return Array.isArray(value) ? String(value[value.length - 1]) : String(value);
}

export function numberArg(values, key) {
  const value = stringArg(values, key, "");
  if (!value) return undefined;
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : Number.NaN;
}
