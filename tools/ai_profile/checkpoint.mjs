#!/usr/bin/env node
import { existsSync, readFileSync } from "node:fs";
import {
  appendRecord,
  buildRecord,
  defaultProfilePath,
  numberArg,
  parseArgs,
  stringArg,
  timestamp,
} from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/checkpoint.mjs --intent <text> [options]

options:
  --phase <phase>                             default: checkpoint
  --category <category>                       default: reflection
  --result <pass|fail|mixed|blocked|skipped|unknown> default: pass
  --value <productive|necessary_overhead|rework|waste|unknown> default: productive
  --profile <path>                            default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --duration-ms <number>                      explicit checkpoint duration
  --max-duration-min <number>                 default: 60; caps inferred duration
  --ts <timestamp>                            checkpoint end timestamp, mostly for tests
  --notes <text>
  --work-item <id>
  --iteration <name>

Records non-command work as a checkpoint. Without --duration-ms, duration is
inferred from the latest existing profile record to this checkpoint timestamp.`);
  process.exit(2);
}

function parseProfile(file) {
  if (!existsSync(file)) return [];
  const records = [];
  for (const [index, rawLine] of readFileSync(file, "utf8").split(/\r?\n/).entries()) {
    const line = rawLine.trim();
    if (!line) continue;
    try {
      records.push({ ...JSON.parse(line), __line: index + 1 });
    } catch {
      // Invalid lines are handled by status/review; checkpoint should not block on old telemetry.
    }
  }
  return records;
}

function eventTime(record) {
  const parsed = Date.parse(record.ts || "");
  return Number.isFinite(parsed) ? parsed : undefined;
}

function latestRecord(records) {
  return [...records].sort((a, b) => (eventTime(b) || 0) - (eventTime(a) || 0))[0];
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const intent = stringArg(values, "intent", "").trim();
if (!intent) usage();

if (!values.phase) values.phase = "checkpoint";
if (!values.category) values.category = "reflection";
if (!values.result) values.result = "pass";
if (!values.value) values.value = "productive";
if (!values.tool) values.tool = "ai_profile/checkpoint.mjs";

const profilePath = stringArg(values, "profile", defaultProfilePath());
const checkpointTs = stringArg(values, "ts", timestamp());
values.ts = checkpointTs;

const previous = latestRecord(parseProfile(profilePath));
const previousMs = previous ? eventTime(previous) : undefined;
const checkpointMs = Date.parse(checkpointTs);
const explicitDuration = numberArg(values, "duration-ms");
const maxDurationMin = numberArg(values, "max-duration-min");
const capMs = Number.isFinite(maxDurationMin) ? maxDurationMin * 60 * 1000 : 60 * 60 * 1000;
let inferredDurationMs = 0;
let durationCapped = false;

if (explicitDuration === undefined) {
  if (previousMs !== undefined && Number.isFinite(checkpointMs)) {
    inferredDurationMs = Math.max(0, checkpointMs - previousMs);
    if (Number.isFinite(capMs) && capMs >= 0 && inferredDurationMs > capMs) {
      inferredDurationMs = capMs;
      durationCapped = true;
    }
  }
  values["duration-ms"] = String(inferredDurationMs);
}

const extra = {
  event_type: "checkpoint",
};
if (previous) {
  extra.previous_profile_line = previous.__line;
  extra.previous_profile_ts = previous.ts || "";
  extra.previous_profile_intent = previous.intent || "";
}
if (explicitDuration === undefined) {
  extra.duration_source = "since_previous_record";
  extra.duration_capped = durationCapped;
} else {
  extra.duration_source = "explicit";
}

try {
  const target = appendRecord(profilePath, buildRecord(values, extra));
  console.log(`profile checkpoint appended: ${target}`);
  console.log(`- duration_ms: ${values["duration-ms"]} (${extra.duration_source}${durationCapped ? ", capped" : ""})`);
} catch (error) {
  console.error(`profile checkpoint failed: ${error.message}`);
  process.exit(1);
}
