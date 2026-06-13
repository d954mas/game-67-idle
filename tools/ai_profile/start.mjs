#!/usr/bin/env node
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import {
  appendRecord,
  buildRecord,
  parseArgs,
  profileScopePath,
  readProfileScope,
  stringArg,
  timestamp,
} from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/start.mjs --work-item <id> [--iteration <name>] [options]

options:
  --profile <path>      default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --scope <path>        default: tmp/session_profiles/current_scope.json
  --phase <phase>       default: profile_start
  --category <category> default: planning
  --intent <text>       default: Start profiled iteration <work-item>
  --notes <text>

Starts a profiled work item in one command by writing persistent scope metadata
and appending a phase_start checkpoint to the selected session profile.`);
  process.exit(2);
}

function writeScope(target, workItem, iteration) {
  const payload = {
    schema_version: 1,
    work_item: workItem,
    iteration,
    updated_at: timestamp(),
  };
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(payload, null, 2)}\n`, "utf8");
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const workItem = stringArg(values, "work-item", "").trim();
const iteration = stringArg(values, "iteration", "").trim();
if (!workItem) usage();

const scopePath = resolve(stringArg(values, "scope", profileScopePath()));
const phase = stringArg(values, "phase", "profile_start");
const category = stringArg(values, "category", "planning");
const intent = stringArg(
  values,
  "intent",
  `Start profiled iteration ${workItem}${iteration ? ` (${iteration})` : ""}`,
);

values["work-item"] = workItem;
if (iteration) values.iteration = iteration;
if (!values.phase) values.phase = phase;
if (!values.category) values.category = category;
if (!values.intent) values.intent = intent;
if (!values.result) values.result = "pass";
if (!values.value) values.value = "necessary_overhead";
if (!values.tool) values.tool = "ai_profile/start.mjs";

try {
  writeScope(scopePath, workItem, iteration);
  const profilePath = stringArg(values, "profile", "");
  const target = appendRecord(profilePath, buildRecord(values, { event_type: "phase_start", scope_path: scopePath }));
  const scope = readProfileScope(scopePath);
  console.log(`profile scope written: ${scope.path}`);
  console.log(`profile event appended: ${target}`);
} catch (error) {
  console.error(`profile start failed: ${error.message}`);
  process.exit(1);
}
