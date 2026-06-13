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
  node tools/ai_profile/gap_checkpoint.mjs --intent <text> [options]

options:
  --profile <path>             default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --min-gap-min <number>       default: 5; skip if latest-record gap is shorter
  --max-duration-min <number>  default: 60; cap inferred duration
  --phase <phase>              default: checkpoint
  --category <category>        default: reflection
  --value <productive|necessary_overhead|rework|waste|unknown> default: necessary_overhead
  --result <pass|fail|mixed|blocked|skipped|unknown> default: pass
  --ts <timestamp>             checkpoint end timestamp, mostly for tests
  --notes <text>
  --work-item <id>
  --iteration <name>

Writes a checkpoint only when the elapsed wall-clock gap since the latest
profile record is at least --min-gap-min. Use it before reflection or after a
long manual/research/review stretch to improve wall-clock coverage without
adding noise for short pauses.`);
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
      // status/review report invalid JSONL; gap checkpoint should not block on old lines.
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

const profilePath = stringArg(values, "profile", defaultProfilePath());
const checkpointTs = stringArg(values, "ts", timestamp());
const checkpointMs = Date.parse(checkpointTs);
const previous = latestRecord(parseProfile(profilePath));
const previousMs = previous ? eventTime(previous) : undefined;
const minGapMin = numberArg(values, "min-gap-min");
const minGapMs = Number.isFinite(minGapMin) ? minGapMin * 60 * 1000 : 5 * 60 * 1000;
const maxDurationMin = numberArg(values, "max-duration-min");
const capMs = Number.isFinite(maxDurationMin) ? maxDurationMin * 60 * 1000 : 60 * 60 * 1000;
const rawGapMs = previousMs !== undefined && Number.isFinite(checkpointMs) ? Math.max(0, checkpointMs - previousMs) : 0;

if (!previous) {
  console.log("gap checkpoint skipped: profile has no previous record");
  process.exit(0);
}
if (rawGapMs < minGapMs) {
  console.log(`gap checkpoint skipped: ${rawGapMs}ms gap is below ${minGapMs}ms threshold`);
  process.exit(0);
}

const durationMs = Number.isFinite(capMs) && capMs >= 0 ? Math.min(rawGapMs, capMs) : rawGapMs;
if (!values.phase) values.phase = "checkpoint";
if (!values.category) values.category = "reflection";
if (!values.result) values.result = "pass";
if (!values.value) values.value = "necessary_overhead";
if (!values.tool) values.tool = "ai_profile/gap_checkpoint.mjs";
values.ts = checkpointTs;
values["duration-ms"] = String(durationMs);

const extra = {
  event_type: "gap_checkpoint",
  previous_profile_line: previous.__line,
  previous_profile_ts: previous.ts || "",
  previous_profile_intent: previous.intent || "",
  raw_gap_ms: rawGapMs,
  min_gap_ms: minGapMs,
  duration_source: "gap_since_previous_record",
  duration_capped: durationMs < rawGapMs,
};

try {
  const target = appendRecord(profilePath, buildRecord(values, extra));
  console.log(`profile gap checkpoint appended: ${target}`);
  console.log(`- duration_ms: ${durationMs}${extra.duration_capped ? ` (capped from ${rawGapMs})` : ""}`);
  console.log(`- previous_line: ${previous.__line}`);
} catch (error) {
  console.error(`profile gap checkpoint failed: ${error.message}`);
  process.exit(1);
}
